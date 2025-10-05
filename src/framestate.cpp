#include "framestate.h"
#include <tinyalloc.h>
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
}

FrameState::~FrameState()
{
	// Don't close handles - let global cleanup handle it
	g_sharedMem.Cleanup();
	sharedMemHandle = nullptr;
	sharedMem = nullptr;
}

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
			{
				info.effectType = effect.type;
				info.usesEffectHA6 = false;
				info.patternId = effect.number;
				info.randomRange = effect.parameters[0];
				info.offsetX = effect.parameters[1];
				info.offsetY = effect.parameters[2];
				info.flagset1 = effect.parameters[3];
				info.flagset2 = effect.parameters[4];
				info.angle = effect.parameters[8];
				info.projVarDecrease = effect.parameters[9];
				isSpawnEffect = true;
				break;
			}

			case 111: // Spawn Random Relative Pattern (offset from parent)
			{
				info.effectType = effect.type;
				info.usesEffectHA6 = false;
				// FIX: For relative spawn, add offset to parent pattern ID
				info.patternId = parentPatternId + effect.number;
				info.randomRange = effect.parameters[0];
				info.offsetX = effect.parameters[1];
				info.offsetY = effect.parameters[2];
				info.flagset1 = effect.parameters[3];
				info.flagset2 = effect.parameters[4];
				info.angle = effect.parameters[8];
				info.projVarDecrease = effect.parameters[9];
				isSpawnEffect = true;
				break;
			}

			case 8:   // Spawn Actor (effect.ha6) - similar to type 1
			{
				info.effectType = effect.type;
				info.usesEffectHA6 = true;  // Type 8 uses effect.ha6
				info.patternId = effect.number;
				info.offsetX = effect.parameters[0];
				info.offsetY = effect.parameters[1];
				info.flagset1 = effect.parameters[2];
				info.flagset2 = effect.parameters[3];
				info.angle = 0;
				info.projVarDecrease = 0;
				info.randomRange = 0;
				isSpawnEffect = true;

				// Debug: Log Effect Type 8 usage
				printf("[Effect 8] Parent pattern %d spawning effect.ha6 pattern %d\n",
					   parentPatternId, info.patternId);

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

			case 1000: // Spawn and Follow (Dust of Osiris, Sion)
			{
				info.effectType = effect.type;
				info.usesEffectHA6 = false;
				info.patternId = effect.number;
				info.offsetX = 0;
				info.offsetY = 0;
				info.flagset1 = 0;
				info.flagset2 = 0;
				info.angle = 0;
				info.projVarDecrease = 0;
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

// Helper to calculate tick position from frame number
int CalculateTickFromFrame(FrameData* frameData, int patternId, int frameNum)
{
	if (!frameData) return 0;

	auto seq = frameData->get_sequence(patternId);
	if (!seq || seq->frames.empty()) return 0;

	// Clamp frame number
	if (frameNum < 0) frameNum = 0;
	if (frameNum >= seq->frames.size()) frameNum = seq->frames.size() - 1;

	// Sum durations of all frames before this one
	int tick = 0;
	for (int i = 0; i < frameNum && i < seq->frames.size(); i++) {
		tick += seq->frames[i].AF.duration;
	}

	return tick;
}

// Helper to calculate frame from tick position
int CalculateFrameFromTick(FrameData* frameData, int patternId, int tick)
{
	if (!frameData) return 0;

	auto seq = frameData->get_sequence(patternId);
	if (!seq || seq->frames.empty()) return 0;

	if (tick < 0) return 0;

	// Sum durations until we exceed the tick
	int tickAccum = 0;
	for (int i = 0; i < seq->frames.size(); i++) {
		int frameDuration = seq->frames[i].AF.duration;
		if (frameDuration <= 0) frameDuration = 1; // Safety

		if (tickAccum + frameDuration > tick) {
			// This is the frame we're currently on
			return i;
		}

		tickAccum += frameDuration;
	}

	// Past the end - return last frame
	return seq->frames.size() - 1;
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

	// Debug: Log data source selection
	if (depth == 0) {
		printf("[BuildSpawnTree] Pattern %d using %s\n",
			   patternId, usesEffectHA6 ? "effect.ha6" : "main character");
	}

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

				// Debug: Log inheritance decisions
				if (spawn.effectType == 8 || originalUsesEffectHA6 != spawn.usesEffectHA6) {
					printf("[Spawn Inheritance] Type %d pattern %d: usesEffectHA6 %s -> %s (parent: %s)\n",
						   spawn.effectType, spawn.patternId,
						   originalUsesEffectHA6 ? "true" : "false",
						   spawn.usesEffectHA6 ? "true" : "false",
						   usesEffectHA6 ? "effect.ha6" : "main");
				}

				// Set hierarchy fields
				spawn.depth = depth;
				spawn.parentSpawnIndex = parentSpawnIndex;
				spawn.absoluteSpawnFrame = absoluteSpawnFrame + frameIdx;
				spawn.spawnTick = currentTick;  // Set actual spawn tick

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
					allSpawns,
					visitedPatterns);
			}
		}

		// Accumulate ticks: add this frame's duration
		currentTick += frame.AF.duration;
	}

	// Remove pattern from visited set (allow it to be spawned in different branches)
	visitedPatterns.erase(patternId);
}
