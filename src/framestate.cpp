#include "framestate.h"
#include "framedata.h"
#include "mv_script.h"
#include "preview_sim.h"
#include <tinyalloc.h>
#include <tuple>
#include <algorithm>
#include <windows.h>

constexpr const wchar_t *sharedMemHandleName = L"hanteichan-shared_mem";

// Global shared memory - initialized once per process
struct SharedMemoryGlobal {
	void *handle = nullptr;
	void *memory = nullptr;
	CopyData *copyData = nullptr;
	bool initialized = false;
	int refCount = 0;

	void Initialize() {
		if (initialized) {
			refCount++;
			return;
		}

		SYSTEM_INFO sInfo;
		GetSystemInfo(&sInfo);

		// 16MB for tinyalloc buffer
		size_t bufSize = 0x100 * sInfo.dwAllocationGranularity;
		// Additional space for CopyData struct at end
		size_t appendSize = (1 + sizeof(CopyData) / sInfo.dwAllocationGranularity) * sInfo.dwAllocationGranularity;

		handle = CreateFileMapping(
			INVALID_HANDLE_VALUE,
			NULL,
			PAGE_READWRITE,
			0,
			bufSize + appendSize,
			sharedMemHandleName);

		auto exists = GetLastError();

		// Base address for consistent mapping across instances
		void *baseAddress = (void*)((size_t)sInfo.lpMinimumApplicationAddress + sInfo.dwAllocationGranularity * 0x5000);

		memory = MapViewOfFileEx(
			handle,
			FILE_MAP_ALL_ACCESS,
			0, 0,
			bufSize + appendSize,
			baseAddress);

		// Initialize tinyalloc with buffer only if this is the first instance
		if (exists != ERROR_ALREADY_EXISTS) {
			// First instance - initialize tinyalloc and create CopyData
			ta_init(memory, (char*)memory + bufSize, 65535, 256, 16, false);
			copyData = new((char*)memory + bufSize) CopyData;
		} else {
			// Subsequent instances - reuse existing tinyalloc and CopyData
			// Just set the heap pointer without re-initializing
			copyData = reinterpret_cast<CopyData*>((char*)memory + bufSize);
		}

		initialized = true;
		refCount = 1;
	}

	void Cleanup() {
		refCount--;
		// NOTE: We intentionally do NOT cleanup when refCount hits 0
		// because tinyalloc's ta_init() can only be called once per process
		// (it has a static init_times counter that asserts == 0).
		// The shared memory will be cleaned up when the process exits.
		// This is fine since it's a process-wide resource.
	}
};

static SharedMemoryGlobal g_sharedMem;

FrameState::FrameState()
{
	// Initialize shared memory once per process
	g_sharedMem.Initialize();

	// All FrameStates share the same CopyData
	copied = g_sharedMem.copyData;
	sharedMemHandle = g_sharedMem.handle;
	sharedMem = g_sharedMem.memory;
	
	// Initialize animation sequence with a default frame for PatEditor preview
	auto frame = &animationSequence.frames.emplace_back();
	frame->AF.aniType = 1;
	auto layer = &frame->AF.layers.emplace_back();
	layer->usePat = true;
	layer->spriteId = 0;
}

FrameState::~FrameState()
{
	// Don't close handles - let global cleanup handle it
	g_sharedMem.Cleanup();
	sharedMemHandle = nullptr;
	sharedMem = nullptr;
}

// ---------------------------------------------------------------------------
// MBTL move-script spawns (mv_script.h) — shared enumeration used by the
// spawn tree, the timeline tick collector and the seek simulator so all
// three (and the live-playback injection that reads the tree entries'
// spawnTick) agree on timing.
// ---------------------------------------------------------------------------

namespace {

// Frame index in seq whose AF.frameId (ha6 AFID) equals afid, or -1.
int FrameIndexForAFID(const Sequence* seq, int afid)
{
	if (!seq) return -1;
	for (int i = 0; i < (int)seq->frames.size(); i++) {
		if (seq->frames[i].AF.frameId == afid)
			return i;
	}
	return -1;
}

// Tick at the start of the given frame (durations <= 0 count as 1 tick).
int TickAtFrameStart(const Sequence* seq, int frameIdx)
{
	if (!seq) return 0;
	int tick = 0;
	for (int i = 0; i < frameIdx && i < (int)seq->frames.size(); i++) {
		int dur = seq->frames[i].AF.duration;
		tick += (dur > 0 ? dur : 1);
	}
	return tick;
}

// One resolved script spawn occurrence with frame-accurate timing.
struct ScriptSpawnNode {
	const MvScriptSpawn* src = nullptr;
	int patternId = -1;      // -1 for impact-effect markers
	bool isImpact = false;
	int parentIndex = -1;    // index of parent node in the output list
	int frameInParent = 0;   // frame index in the parent pattern
	int tick = 0;            // absolute tick from root pattern start
	int offsetX = 0, offsetY = 0; // accumulated (parent + own)
	int depth = 0;           // 0 = spawned by the move, 1 = by the spawned object
	std::string source;
};

// Flatten the script spawns of a root pattern into timed nodes:
//  - depth 0: spawns of the move block itself, timed by matching the script's
//    frame-ID gate (frameIdRef) against the root pattern's AFIDs.
//  - depth 1: spawns declared by the spawned object's own template block
//    (mv=/mvname= reference), timed against the spawned pattern's AFIDs and
//    chained under their parent node instead of being flattened to the root.
void EnumerateScriptSpawnNodes(FrameData* mainFrameData, int patternId,
                               std::vector<ScriptSpawnNode>& out)
{
	const MvScriptIndex* mvIndex = MvScriptIndex::Lookup(mainFrameData);
	const std::vector<MvScriptSpawn>* list =
		mvIndex ? mvIndex->spawnsForPattern(patternId) : nullptr;
	if (!list) return;

	auto rootSeq = mainFrameData->get_sequence(patternId);
	if (!rootSeq || rootSeq->frames.empty()) return;

	// Template blocks whose spawns will be chained under a resolved entry of
	// this pattern; their flattened duplicates are skipped at root level.
	std::set<std::string> chainedTemplates;
	for (const auto& ss : *list) {
		if (!ss.isImpactEffect && ss.patternId >= 0 && ss.patternId != patternId &&
		    !ss.mvName.empty() && mvIndex->spawnsForTemplate(ss.mvName))
			chainedTemplates.insert(ss.mvName);
	}

	for (const auto& ss : *list) {
		if (!ss.ownerMove.empty() && chainedTemplates.count(ss.ownerMove))
			continue; // appears as a grandchild below instead

		// Timing: match the script's frame-ID gate against the pattern AFIDs.
		int rootFrame = 0;
		if (ss.frameIdRef >= 0) {
			int f = FrameIndexForAFID(rootSeq, ss.frameIdRef);
			if (f >= 0) rootFrame = f;
		}
		int rootTick = TickAtFrameStart(rootSeq, rootFrame);

		if (ss.isImpactEffect) {
			ScriptSpawnNode node;
			node.src = &ss;
			node.isImpact = true;
			node.frameInParent = rootFrame;
			node.tick = rootTick;
			node.offsetX = ss.offsetX;
			node.offsetY = ss.offsetY;
			node.source = ss.source;
			out.push_back(node);
			continue;
		}

		if (ss.patternId < 0 || ss.patternId == patternId)
			continue; // unresolved: panel-only (box_pane lists them)
		auto childSeq = mainFrameData->get_sequence(ss.patternId);
		if (!childSeq || childSeq->frames.empty())
			continue;

		int rootIdx = (int)out.size();
		{
			ScriptSpawnNode node;
			node.src = &ss;
			node.patternId = ss.patternId;
			node.frameInParent = rootFrame;
			node.tick = rootTick;
			node.offsetX = ss.offsetX;
			node.offsetY = ss.offsetY;
			node.source = ss.source;
			out.push_back(node);
		}

		// Grandchildren: the spawned object's template block's own spawns.
		const std::vector<MvScriptSpawn>* tplList =
			ss.mvName.empty() ? nullptr : mvIndex->spawnsForTemplate(ss.mvName);
		if (!tplList)
			continue;
		for (const auto& tpl : *tplList) {
			if (tpl.isImpactEffect)
				continue;
			if (tpl.patternId < 0 || tpl.patternId == ss.patternId)
				continue;
			auto gcSeq = mainFrameData->get_sequence(tpl.patternId);
			if (!gcSeq || gcSeq->frames.empty())
				continue;
			int childFrame = 0;
			if (tpl.frameIdRef >= 0) {
				int f = FrameIndexForAFID(childSeq, tpl.frameIdRef);
				if (f >= 0) childFrame = f;
			}
			ScriptSpawnNode node;
			node.src = &tpl;
			node.patternId = tpl.patternId;
			node.parentIndex = rootIdx;
			node.frameInParent = childFrame;
			node.tick = rootTick + TickAtFrameStart(childSeq, childFrame);
			node.offsetX = ss.offsetX + tpl.offsetX;
			node.offsetY = ss.offsetY + tpl.offsetY;
			node.depth = 1;
			node.source = "via " + ss.mvName + "  " + tpl.source;
			out.push_back(node);
		}
	}
}

} // namespace

// Parse spawned patterns from effects in a frame
std::vector<SpawnedPatternInfo> ParseSpawnedPatterns(const std::vector<Frame_EF>& effects, int parentFrame, int parentPatternId)
{
	std::vector<SpawnedPatternInfo> spawned;

	for (size_t i = 0; i < effects.size(); i++)
	{
		const Frame_EF& effect = effects[i];
		SpawnedPatternInfo info;
		info.effectIndex = static_cast<int>(i);
		info.parentFrame = parentFrame;  // Tag with parent frame
		info.parentPatternId = parentPatternId;

		bool isSpawnEffect = false;

		switch (effect.type)
		{
			case 1:   // Spawn Pattern (absolute)
			{
				info.effectType = effect.type;
				info.usesEffectHA6 = false;
				info.patternId = effect.number;
				info.offsetX = effect.parameters[0];
				info.offsetY = effect.parameters[1];
				info.flagset1 = effect.parameters[2];
				info.flagset2 = effect.parameters[3];
				info.angle = effect.parameters[7];
				info.projVarDecrease = effect.parameters[8];
				info.randomRange = 0;
				isSpawnEffect = true;
				break;
			}

			case 101: // Spawn Relative Pattern (offset from parent)
			{
				info.effectType = effect.type;
				info.usesEffectHA6 = false;
				// FIX: For relative spawn, add offset to parent pattern ID
				info.patternId = parentPatternId + effect.number;
				info.offsetX = effect.parameters[0];
				info.offsetY = effect.parameters[1];
				info.flagset1 = effect.parameters[2];
				info.flagset2 = effect.parameters[3];
				info.angle = effect.parameters[7];
				info.projVarDecrease = effect.parameters[8];
				info.randomRange = 0;
				isSpawnEffect = true;
				break;
			}

			case 11:  // Spawn Random Pattern (absolute)
			case 111: // Spawn Random Relative Pattern (offset from parent)
			{
				// MBAA Effect11_SpawnRandomPatterns 0x454E30: p1/p2 base offset,
				// p3/p4 random rectangle (p4 == 30000: circle of radius p3),
				// p5 pattern range, p6 count, p7/p8 flagsets, p9 angle +
				// rand(p10), p11 projectile var. (1-based pN = parameters[N-1])
				info.effectType = effect.type;
				info.usesEffectHA6 = false;
				info.patternId = effect.number + (effect.type == 111 ? parentPatternId : 0);
				info.offsetX = effect.parameters[0];
				info.offsetY = effect.parameters[1];
				info.randomRange = effect.parameters[4];
				info.flagset1 = effect.parameters[6];
				info.flagset2 = effect.parameters[7];
				info.angle = effect.parameters[8];
				info.projVarDecrease = effect.parameters[10];
				isSpawnEffect = true;
				break;
			}

			case 8:   // Spawn Actor (effect.ha6)
			case 108: // Spawn Relative Actor (effect.ha6; 100 < type < 1000 is relative)
			{
				// MBAA Effect8_SpawnEffectHa6Actor 0x4551B0 passes the EF record
				// straight to Effect_InitializeComplex: same layout as EF1,
				// including the angle in p8.
				info.effectType = effect.type;
				info.usesEffectHA6 = true;
				info.patternId = effect.number + (effect.type == 108 ? parentPatternId : 0);
				info.offsetX = effect.parameters[0];
				info.offsetY = effect.parameters[1];
				info.flagset1 = effect.parameters[2];
				info.flagset2 = effect.parameters[3];
				info.angle = effect.parameters[7];
				info.projVarDecrease = effect.parameters[8];
				info.randomRange = 0;
				isSpawnEffect = true;
				break;
			}

			case 3:   // Spawn Preset Effect
			{
				info.effectType = effect.type;
				info.usesEffectHA6 = false;
				info.isPresetEffect = true;  // Mark as preset effect, not pattern
				info.patternId = effect.number;  // Preset effect number (0-302, see Effect_Type3_Implementation.md)
				info.offsetX = effect.parameters[0];  // X position
				info.offsetY = effect.parameters[1];  // Y position
				info.flagset1 = 0;  // Preset effects don't use flagsets
				info.flagset2 = 0;
				info.angle = 0;
				info.projVarDecrease = 0;
				info.randomRange = 0;
				isSpawnEffect = true;
				break;
			}

			case 1000: // Spawn once (Effect1_SpawnPattern: EF1 layout, guarded by var p6)
			{
				info.effectType = effect.type;
				info.usesEffectHA6 = false;
				info.patternId = effect.number;
				info.offsetX = effect.parameters[0];
				info.offsetY = effect.parameters[1];
				info.flagset1 = effect.parameters[2];
				info.flagset2 = effect.parameters[3];
				info.angle = effect.parameters[7];
				info.projVarDecrease = effect.parameters[8];
				info.randomRange = 0;
				isSpawnEffect = true;
				break;
			}

			default:
				break;
		}

		if (isSpawnEffect)
		{
			// Assign default tint colors based on spawn index
			int spawnIdx = static_cast<int>(spawned.size());
			switch (spawnIdx % 3)
			{
				case 0: info.tintColor = glm::vec4(0.5f, 0.7f, 1.0f, 1.0f); break; // Blue
				case 1: info.tintColor = glm::vec4(0.5f, 1.0f, 0.5f, 1.0f); break; // Green
				case 2: info.tintColor = glm::vec4(1.0f, 1.0f, 0.5f, 1.0f); break; // Yellow
			}

			spawned.push_back(info);
		}
	}

	return spawned;
}

// First tick the runtime flow enters frameNum (the tick simulator's root
// track: engine loop rules, native IFs, runtime IFs assumed false). Frames the
// flow never reaches fall back to the authored start tick.
int CalculateTickFromFrame(FrameData* frameData, int patternId, int frameNum)
{
	if (!frameData) return 0;

	auto seq = frameData->get_sequence(patternId);
	if (!seq || seq->frames.empty()) return 0;

	if (frameNum < 0) frameNum = 0;
	if (frameNum >= (int)seq->frames.size()) frameNum = (int)seq->frames.size() - 1;

	// Root flow only: spawned children never influence it, so no effect data.
	// One cached simulator (UI thread): repeated calls for the same pattern,
	// e.g. while dragging the frame slider, reuse its checkpoints.
	static preview::PreviewSim s_rootFlowSim;
	preview::Options o;
	o.horizonTicks = 3000;
	o.maxActors = 64;
	s_rootFlowSim.setInputs(frameData, nullptr, patternId, o);
	int t = s_rootFlowSim.firstTickOfRootFrame(frameNum);
	if (t >= 0) return t;

	int tick = 0;
	for (int i = 0; i < frameNum; i++)
		tick += std::max(1, seq->frames[i].AF.duration);
	return tick;
}

// Find loop period by simulating animation flow and detecting when we return to frame 0
// Uses the actual Hantei animation logic (aniType/aniFlag/jump) to properly detect loops
int FindLoopPeriod(FrameData* frameData, int patternId, int maxTicks)
{
	if (!frameData) return 0;
	
	auto seq = frameData->get_sequence(patternId);
	if (!seq || seq->frames.empty()) return 0;
	
	// Follow the reachable control flow. A go-to can close a cycle long before
	// the last physical frame (later frames may be alternate branches), so the
	// last frame's aniType does not decide whether the pattern loops.
	// Track (frame, loopCounter) state to detect true loops
	struct AnimationState {
		int frame;
		int loopCounter;
		bool operator<(const AnimationState& other) const {
			if (frame != other.frame) return frame < other.frame;
			return loopCounter < other.loopCounter;
		}
	};
	
	std::map<AnimationState, int> stateToTick;  // state -> first tick we saw it at
	int currentFrame = 0;
	int currentTick = 0;
	int loopCounter = 0;
	int frameDuration = 0;
	
	// Initialize loop counter from first frame
	if (!seq->frames.empty() && seq->frames[0].AF.loopCount > 0) {
		loopCounter = seq->frames[0].AF.loopCount;
	}
	
	for (int i = 0; i < maxTicks; i++) {
		// Compare and record states only at frame boundaries: a frame keeps
		// the same (frame, loopCounter) for its whole duration, so checking
		// every tick reported any multi-tick first frame as a 1-tick loop.
		if (frameDuration == 0) {
			AnimationState currentState = {currentFrame, loopCounter};
			auto seen = stateToTick.find(currentState);
			if (seen != stateToTick.end()) {
				const int period = currentTick - seen->second;
				if (period > 0) return period;
			}
			stateToTick[currentState] = currentTick;
		}
		
		// Advance using actual animation logic
		if (currentFrame < 0 || currentFrame >= seq->frames.size()) {
			break;
		}
		
		auto& frame = seq->frames[currentFrame];
		int frameDur = frame.AF.duration;
		if (frameDur <= 0) frameDur = 1;
		
		if (frameDuration >= frameDur) {
			frameDuration = 0;
			
			// Calculate next frame using actual Hantei animation logic
			int nextFrame = currentFrame;
			if (frame.AF.aniType == 1) {
				// Sequential advance
				if (currentFrame + 1 >= seq->frames.size()) {
					break;  // Reached end
				}
				nextFrame = currentFrame + 1;
			}
			else if (frame.AF.aniType == 2) {
				// Jump/loop logic (same as GetNextFrame in main_ui_impl.h)
				if ((frame.AF.aniFlag & 0x2) && loopCounter < 0) {
					// Loop count exhausted - use loopEnd
					if (frame.AF.aniFlag & 0x8) {
						nextFrame = currentFrame + frame.AF.loopEnd;
					} else {
						nextFrame = frame.AF.loopEnd;
					}
				} else {
					// Decrement loop counter if needed
					if (frame.AF.aniFlag & 0x2) {
						loopCounter--;
					}
					// Jump to next frame
					if (frame.AF.aniFlag & 0x4) {
						nextFrame = currentFrame + frame.AF.jump;
					} else {
						nextFrame = frame.AF.jump;
					}
				}
			} else {
				break;  // aniType 0 - stops
			}
			
			// Update loop counter from new frame if it has one
			if (nextFrame >= 0 && nextFrame < seq->frames.size()) {
				if (seq->frames[nextFrame].AF.loopCount > 0) {
					loopCounter = seq->frames[nextFrame].AF.loopCount;
				}
			}
			
			currentFrame = nextFrame;
		} else {
			frameDuration++;
			currentTick++;
		}
	}
	
	return std::max(1, currentTick);  // no cycle: duration of the reachable path
}

// Recursive function to build full spawn tree
void BuildSpawnTreeRecursive(
	FrameData* mainFrameData,
	FrameData* effectFrameData,
	int patternId,
	bool usesEffectHA6,
	int parentSpawnIndex,
	int depth,
	int absoluteSpawnFrame,
	int absoluteSpawnTick,
	int accumulatedOffsetX,
	int accumulatedOffsetY,
	std::vector<SpawnedPatternInfo>& allSpawns,
	std::set<int>& visitedPatterns)
{
	// Safety: limit max recursion depth
	constexpr int MAX_DEPTH = 10;
	if (depth > MAX_DEPTH) {
		return;
	}

	// Cycle detection: check if we're already processing this pattern
	if (visitedPatterns.find(patternId) != visitedPatterns.end()) {
		// Mark the parent spawn as recursive if it exists
		if (parentSpawnIndex >= 0 && parentSpawnIndex < allSpawns.size()) {
			allSpawns[parentSpawnIndex].isRecursive = true;
		}
		return;
	}

	// Get the appropriate frame data source
	FrameData* sourceFrameData = usesEffectHA6 ? effectFrameData : mainFrameData;

	// Debug: Log data source selection (commented out to reduce spam)
	// if (depth == 0) {
	// 	printf("[BuildSpawnTree] Pattern %d using %s\n",
	// 		   patternId, usesEffectHA6 ? "effect.ha6" : "main character");
	// }

	if (!sourceFrameData) {
		printf("[BuildSpawnTree ERROR] Pattern %d needs %s but data is NULL!\n",
			   patternId, usesEffectHA6 ? "effect.ha6" : "main character");
		return;
	}

	// Get the pattern's sequence
	auto sequence = sourceFrameData->get_sequence(patternId);
	if (!sequence || sequence->frames.empty()) return;

	// Add pattern to visited set
	visitedPatterns.insert(patternId);

	// Calculate pattern lifetime
	int frameCount = sequence->frames.size();
	int lifetime = frameCount;

	// Check if pattern loops (aniType == 2) or stops (aniType == 0 or 1)
	if (frameCount > 0) {
		auto& lastFrame = sequence->frames.back();
		if (lastFrame.AF.aniType == 2) {
			// Looping pattern - set lifetime to a large number for now
			lifetime = 9999;  // Effectively infinite
		}
	}

	// Track tick accumulation as we iterate frames
	int currentTick = absoluteSpawnTick;

	// Iterate through all frames in this pattern
	for (int frameIdx = 0; frameIdx < frameCount; frameIdx++) {
		auto& frame = sequence->frames[frameIdx];

		if (!frame.EF.empty()) {
			// Parse spawn effects in this frame
			auto frameSpawns = ParseSpawnedPatterns(frame.EF, frameIdx, patternId);

			// Process each spawn found in this frame
			for (auto& spawn : frameSpawns) {
				// IMPORTANT: Data source inheritance rules
				// 1. Effect Type 8 always explicitly uses effect.ha6 (set by ParseSpawnedPatterns)
				// 2. Other effect types inherit parent's data source (if parent uses effect.ha6)
				// 3. Default is main character data (set by ParseSpawnedPatterns)

				bool originalUsesEffectHA6 = spawn.usesEffectHA6;

				// Apply inheritance: if parent uses effect.ha6, children should too
				// Exception: Type 8 already explicitly sets effect.ha6, so skip it
				if (usesEffectHA6) {
					if (spawn.effectType == 8) {
						// Type 8 already set usesEffectHA6 = true in ParseSpawnedPatterns
						// Don't override it (though the value should be the same)
					} else {
						// Non-Type-8 children inherit parent's data source
						spawn.usesEffectHA6 = true;
					}
				}

				// Debug: Log inheritance decisions (commented out to reduce spam)
				// if (spawn.effectType == 8 || originalUsesEffectHA6 != spawn.usesEffectHA6) {
				// 	printf("[Spawn Inheritance] Type %d pattern %d: usesEffectHA6 %s -> %s (parent: %s)\n",
				// 		   spawn.effectType, spawn.patternId,
				// 		   originalUsesEffectHA6 ? "true" : "false",
				// 		   spawn.usesEffectHA6 ? "true" : "false",
				// 		   usesEffectHA6 ? "effect.ha6" : "main");
				// }

				// Set hierarchy fields
				spawn.depth = depth;
				spawn.parentSpawnIndex = parentSpawnIndex;
				spawn.absoluteSpawnFrame = absoluteSpawnFrame + frameIdx;
				spawn.spawnTick = currentTick;  // Set actual spawn tick

				// Accumulate offsets from parent hierarchy
				spawn.offsetX += accumulatedOffsetX;
				spawn.offsetY += accumulatedOffsetY;

			// Calculate child pattern's lifetime
			// Skip pattern loading for Effect Type 3 (preset effects)
			if (!spawn.isPresetEffect) {
				FrameData* childSource = spawn.usesEffectHA6 ? effectFrameData : mainFrameData;

				// Warn if Effect Type 8 needs effect.ha6 but it's null
				if (spawn.effectType == 8 && !effectFrameData) {
					printf("[ERROR] Effect Type 8 pattern %d requires effect.ha6 but it's not loaded!\n",
						   spawn.patternId);
				}

				if (childSource) {
					auto childSeq = childSource->get_sequence(spawn.patternId);
					if (childSeq && !childSeq->frames.empty()) {
						spawn.patternFrameCount = childSeq->frames.size();
						spawn.lifetime = spawn.patternFrameCount;

						// Check child's aniType
						auto& childLastFrame = childSeq->frames.back();
						if (childLastFrame.AF.aniType == 2) {
							spawn.lifetime = 9999;  // Looping
						}
					} else {
						printf("[WARNING] Pattern %d not found in %s\n",
							   spawn.patternId, spawn.usesEffectHA6 ? "effect.ha6" : "main character");
					}
				} else {
					printf("[ERROR] Data source is NULL for pattern %d (needs %s)\n",
						   spawn.patternId, spawn.usesEffectHA6 ? "effect.ha6" : "main character");
				}
			} else {
				// Preset effects don't have patterns - instant effect
				spawn.patternFrameCount = 0;
				spawn.lifetime = 0;  // Instant, no duration
			}

			// Assign tint color based on depth
			switch (depth % 4) {
				case 0: spawn.tintColor = glm::vec4(0.5f, 0.7f, 1.0f, 1.0f); break;  // Blue
				case 1: spawn.tintColor = glm::vec4(0.5f, 1.0f, 0.5f, 1.0f); break;  // Green
				case 2: spawn.tintColor = glm::vec4(1.0f, 1.0f, 0.5f, 1.0f); break;  // Yellow
				case 3: spawn.tintColor = glm::vec4(1.0f, 0.5f, 0.5f, 1.0f); break;  // Red
			}

			// Add this spawn to the output array
			int currentSpawnIndex = allSpawns.size();
			allSpawns.push_back(spawn);

			// Update parent's child indices if this has a parent
			if (parentSpawnIndex >= 0 && parentSpawnIndex < allSpawns.size()) {
				allSpawns[parentSpawnIndex].childSpawnIndices.push_back(currentSpawnIndex);
			}

				// Recursively process the spawned pattern's children
				BuildSpawnTreeRecursive(
					mainFrameData,
					effectFrameData,
					spawn.patternId,
					spawn.usesEffectHA6,
					currentSpawnIndex,
					depth + 1,
					spawn.absoluteSpawnFrame,
					spawn.spawnTick,  // Pass spawn tick to children
					spawn.offsetX,    // Pass accumulated X offset to children
					spawn.offsetY,    // Pass accumulated Y offset to children
					allSpawns,
					visitedPatterns);
			}
		}

		// Accumulate ticks: add this frame's duration
		currentTick += frame.AF.duration;
	}

	// Root pattern only: merge MBTL move-script spawns (mv_script.h). MBTL
	// moves spawn projectile patterns from Squirrel scripts instead of ha6 EF
	// data; the character's MvScriptIndex (registered by CharacterInstance)
	// tells us which patterns this move spawns. Spawn frames/ticks come from
	// the scripts' frame-ID gates matched against the pattern's AFIDs, and
	// spawns declared by the spawned object's template block are chained
	// under it as depth-1 children. The live-playback injection reads these
	// entries' spawnTick, so playback follows the same timing.
	if (depth == 0 && parentSpawnIndex == -1 && !usesEffectHA6) {
		std::vector<ScriptSpawnNode> nodes;
		EnumerateScriptSpawnNodes(mainFrameData, patternId, nodes);
		// Map node index -> allSpawns index for parent chaining.
		std::vector<int> nodeToSpawnIndex(nodes.size(), -1);
		for (size_t ni = 0; ni < nodes.size(); ni++) {
			const auto& node = nodes[ni];
			int parentEntryIdx = node.parentIndex >= 0 ? nodeToSpawnIndex[node.parentIndex] : -1;
			if (node.parentIndex >= 0 && parentEntryIdx < 0)
				continue; // parent was skipped

			SpawnedPatternInfo spawn;
			spawn.isScriptSpawn = true;
			spawn.scriptSource = node.source;
			spawn.effectIndex = -1;   // no ha6 EF backs this spawn
			spawn.effectType = 0;
			spawn.usesEffectHA6 = false;
			spawn.patternId = node.patternId;
			spawn.offsetX = node.offsetX + accumulatedOffsetX;
			spawn.offsetY = node.offsetY + accumulatedOffsetY;
			spawn.parentFrame = node.frameInParent;
			spawn.depth = node.depth;
			spawn.parentSpawnIndex = parentEntryIdx;
			// Like ha6 entries: parent's absolute frame + frame within parent.
			spawn.absoluteSpawnFrame = node.frameInParent +
				(node.parentIndex >= 0 ? nodes[node.parentIndex].frameInParent : 0);
			spawn.spawnTick = node.tick;
			spawn.visible = true;
			spawn.tintColor = glm::vec4(1.0f, 0.7f, 0.4f, 1.0f);  // Orange: script spawn

			if (node.isImpact) {
				// SetImpactHitEffect marker: preset-effect style crosshair,
				// no pattern behind it.
				spawn.isPresetEffect = true;
				spawn.patternId = -1;
				spawn.patternFrameCount = 0;
				spawn.lifetime = 0;
			} else {
				auto childSeq = mainFrameData->get_sequence(node.patternId);
				if (!childSeq || childSeq->frames.empty())
					continue;
				spawn.patternFrameCount = childSeq->frames.size();
				spawn.lifetime = spawn.patternFrameCount;
				if (childSeq->frames.back().AF.aniType == 2) {
					spawn.lifetime = 9999;  // Looping
				}
			}

			int currentSpawnIndex = allSpawns.size();
			nodeToSpawnIndex[ni] = currentSpawnIndex;
			allSpawns.push_back(spawn);

			if (parentEntryIdx >= 0 && parentEntryIdx < (int)allSpawns.size())
				allSpawns[parentEntryIdx].childSpawnIndices.push_back(currentSpawnIndex);

			// Recurse into the spawned pattern's own ha6 spawns.
			if (!node.isImpact) {
				BuildSpawnTreeRecursive(
					mainFrameData,
					effectFrameData,
					node.patternId,
					false,
					currentSpawnIndex,
					node.depth + 1,
					spawn.absoluteSpawnFrame,
					spawn.spawnTick,
					spawn.offsetX,
					spawn.offsetY,
					allSpawns,
					visitedPatterns);
			}
		}
	}

	// Remove pattern from visited set (allow it to be spawned in different branches)
	visitedPatterns.erase(patternId);
}

// ---------------------------------------------------------------------------
// Preview simulator binding
// ---------------------------------------------------------------------------

preview::PreviewSim& FrameState::BindPreviewSim(FrameData* mainData, FrameData* effectData)
{
	if (!previewSim)
		previewSim = std::make_shared<preview::PreviewSim>();

	// MBTL move-script spawns become scheduled spawns (same frame-ID timing
	// the spawn tree uses). Rebuilt when the pattern or data changes.
	const uint64_t version = mainData ? mainData->dataVersion : 0;
	if (mainData != scheduleData || pattern != schedulePattern || version != scheduleVersion) {
		scheduleData = mainData;
		schedulePattern = pattern;
		scheduleVersion = version;
		scriptSchedule.clear();
		if (mainData) {
			std::vector<ScriptSpawnNode> nodes;
			EnumerateScriptSpawnNodes(mainData, pattern, nodes);
			for (const auto& n : nodes) {
				preview::ScheduledSpawn e;
				e.tick = n.tick;
				e.patternId = n.isImpact ? -1 : n.patternId;
				e.isMarker = n.isImpact;
				// Own offset: script nodes accumulate parent + own offsets.
				e.offsetX = n.src ? n.src->offsetX : n.offsetX;
				e.offsetY = n.src ? n.src->offsetY : n.offsetY;
				e.parentIndex = n.parentIndex;
				scriptSchedule.push_back(e);
			}
		}
	}

	preview::Options o = previewOptions;
	// The half-scale PAT owner rule is MBAA's; UNI/MBTL data keeps 1:1.
	o.patOwnerHalfScale = previewOptions.patOwnerHalfScale && mainData && !mainData->usesUniFormat();
	previewSim->setInputs(mainData, effectData, pattern, o, scriptSchedule);
	return *previewSim;
}

const SpawnedPatternInfo* FindSpawnTreeEntry(const std::vector<SpawnedPatternInfo>& tree,
	int srcPattern, int srcFrame, int srcEffectIndex, bool effectHa6, bool isScript, int pattern)
{
	const SpawnedPatternInfo* fallback = nullptr;
	for (const auto& sp : tree) {
		if (isScript) {
			if (sp.isScriptSpawn && sp.patternId == pattern) return &sp;
			continue;
		}
		if (sp.isScriptSpawn) continue;
		if (sp.parentPatternId == srcPattern && sp.parentFrame == srcFrame &&
		    sp.effectIndex == srcEffectIndex && sp.usesEffectHA6 == effectHa6)
			return &sp;
		if (!fallback && sp.patternId == pattern && sp.usesEffectHA6 == effectHa6)
			fallback = &sp;
	}
	return fallback;
}
