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

				// Debug: Log Effect Type 8 usage (commented out to reduce spam)
				// printf("[Effect 8] Parent pattern %d spawning effect.ha6 pattern %d\n",
				// 	   parentPatternId, info.patternId);

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

// Simulate animation flow from tick 0 to target tick, following loops/jumps
// Returns the frame that would be active at the target tick
int SimulateAnimationFlow(FrameData* frameData, int patternId, int targetTick)
{
	if (!frameData) return 0;

	auto seq = frameData->get_sequence(patternId);
	if (!seq || seq->frames.empty()) return 0;

	if (targetTick < 0) return 0;

	int currentFrame = 0;
	int currentTick = 0;
	int loopCounter = 0;
	int frameDuration = 0;
	const int MAX_ITERATIONS = 100000; // Safety limit
	int iterations = 0;

	// Initialize loop counter from first frame if it has one
	if (!seq->frames.empty() && seq->frames[0].AF.loopCount > 0) {
		loopCounter = seq->frames[0].AF.loopCount;
	}

	while (currentTick < targetTick && iterations < MAX_ITERATIONS) {
		iterations++;

		// Get current frame data
		if (currentFrame < 0 || currentFrame >= seq->frames.size()) {
			// Invalid frame - return last valid frame
			return seq->frames.size() - 1;
		}

		auto& frame = seq->frames[currentFrame];
		int frameDur = frame.AF.duration;
		if (frameDur <= 0) frameDur = 1; // Safety

		// Check if we need to advance to next frame
		if (frameDuration >= frameDur) {
			// Time to advance frame
			frameDuration = 0;

			// Calculate next frame based on animation type
			int nextFrame = currentFrame;

			if (frame.AF.aniType == 1) {
				// Sequential advance
				if (currentFrame + 1 >= seq->frames.size()) {
					// Reached end - stop
					return currentFrame;
				} else {
					nextFrame = currentFrame + 1;
				}
			}
			else if (frame.AF.aniType == 2) {
				// Jump/loop logic
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
				// aniType 0 or other - stop animation
				return currentFrame;
			}

			// Update loop counter from new frame if it has one
			if (nextFrame >= 0 && nextFrame < seq->frames.size()) {
				if (seq->frames[nextFrame].AF.loopCount > 0) {
					loopCounter = seq->frames[nextFrame].AF.loopCount;
				}
			}

			currentFrame = nextFrame;
		} else {
			// Still in current frame - advance tick
			int ticksToAdvance = std::min(frameDur - frameDuration, targetTick - currentTick);
			frameDuration += ticksToAdvance;
			currentTick += ticksToAdvance;
		}
	}

	// Safety: if we hit max iterations, return current frame
	if (iterations >= MAX_ITERATIONS) {
		printf("[WARNING] SimulateAnimationFlow hit max iterations for pattern %d at tick %d\n", patternId, targetTick);
	}

	return currentFrame;
}

// Find loop period by simulating animation flow and detecting when we return to frame 0
// Uses the actual Hantei animation logic (aniType/aniFlag/jump) to properly detect loops
int FindLoopPeriod(FrameData* frameData, int patternId, int maxTicks)
{
	if (!frameData) return 0;
	
	auto seq = frameData->get_sequence(patternId);
	if (!seq || seq->frames.empty()) return 0;
	
	// Check if pattern actually loops (aniType 2 on last frame)
	bool patternLoops = false;
	if (!seq->frames.empty()) {
		auto& lastFrame = seq->frames.back();
		patternLoops = (lastFrame.AF.aniType == 2);
	}
	
	if (!patternLoops) {
		// Non-looping pattern - calculate total duration
		int totalDuration = 0;
		for (int i = 0; i < seq->frames.size(); i++) {
			int dur = seq->frames[i].AF.duration;
			if (dur <= 0) dur = 1;
			totalDuration += dur;
		}
		return totalDuration;
	}
	
	// For looping patterns, simulate animation flow and detect when we return to frame 0
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
		// Check if we've seen this animation state before
		AnimationState currentState = {currentFrame, loopCounter};
		if (stateToTick.find(currentState) != stateToTick.end()) {
			// Found a cycle! Return the period
			int firstTick = stateToTick[currentState];
			int period = currentTick - firstTick;
			if (period > 0) {
				return period;
			}
		}
		
		// Record this state (only at frame boundaries to avoid duplicates)
		if (frameDuration == 0) {
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
	
	return 0;  // No loop detected
}

// Simulate animation flow and collect all spawn ticks (including loop iterations and nested spawns)
// This is the source of truth for timeline visualization
std::map<int, std::vector<int>> CollectAllSpawnTicks(
	FrameData* mainFrameData,
	FrameData* effectFrameData,
	int patternId,
	int maxTicks,
	bool isEffectHA6,
	int parentSpawnTick)
{
	std::map<int, std::vector<int>> spawnTicks;  // compositeKey -> vector of spawn ticks
	
	FrameData* sourceData = isEffectHA6 ? effectFrameData : mainFrameData;
	if (!sourceData) return spawnTicks;
	
	auto seq = sourceData->get_sequence(patternId);
	if (!seq || seq->frames.empty()) return spawnTicks;
	
	int currentFrame = 0;
	int currentTick = 0;
	int loopCounter = 0;
	int frameDuration = 0;
	int lastFrameEntered = -1;  // Track when we enter a new frame
	
	// Initialize loop counter
	if (!seq->frames.empty() && seq->frames[0].AF.loopCount > 0) {
		loopCounter = seq->frames[0].AF.loopCount;
	}
	
	// Track visited states to detect infinite loops
	struct AnimationState {
		int frame;
		int loopCounter;
		bool operator<(const AnimationState& other) const {
			if (frame != other.frame) return frame < other.frame;
			return loopCounter < other.loopCounter;
		}
	};
	std::set<AnimationState> visitedStates;
	
	const int MAX_ITERATIONS = 100000; // Safety limit
	int iterations = 0;
	
	while (currentTick < maxTicks && iterations < MAX_ITERATIONS) {
		iterations++;
		
		// Don't break on infinite loops - we want to collect all spawn ticks including loop iterations
		// Just track states to avoid infinite recursion in nested spawns
		AnimationState currentState = {currentFrame, loopCounter};
		if (visitedStates.find(currentState) != visitedStates.end() && visitedStates.size() > 10) {
			// We've seen this state before and collected enough data - continue but don't break
			// This allows us to collect spawns from multiple loop iterations
		}
		visitedStates.insert(currentState);
		
		// Check if we just entered a new frame (frame boundary)
		if (currentFrame != lastFrameEntered && frameDuration == 0) {
			lastFrameEntered = currentFrame;
			
			// Check if this frame has spawn effects
			if (currentFrame >= 0 && currentFrame < seq->frames.size()) {
				auto& frame = seq->frames[currentFrame];
				if (!frame.EF.empty()) {
					// Parse spawn effects
					auto frameSpawns = ParseSpawnedPatterns(frame.EF, currentFrame, patternId);
					
					// Record spawn ticks for each spawn
					// Use a composite key (patternId + usesEffectHA6 flag) to distinguish
					// between main pattern spawns and effect.ha6 spawns with same patternId
					for (const auto& spawn : frameSpawns) {
						// Create composite key: patternId * 2 + (usesEffectHA6 ? 1 : 0)
						// This ensures main pattern 20 and effect.ha6 pattern 20 are stored separately
						int compositeKey = spawn.patternId * 2 + (spawn.usesEffectHA6 ? 1 : 0);
						
						// Calculate spawn tick (current tick when we enter this frame, relative to parent)
						int absoluteSpawnTick = parentSpawnTick + currentTick;
						spawnTicks[compositeKey].push_back(absoluteSpawnTick);
						
						// Recursively collect spawns from nested patterns
						// Only recurse if the spawned pattern exists and we haven't gone too deep
						if (!spawn.isPresetEffect && currentTick < maxTicks - 100) {
							FrameData* nestedSourceData = spawn.usesEffectHA6 ? effectFrameData : mainFrameData;
							if (nestedSourceData) {
								auto nestedSeq = nestedSourceData->get_sequence(spawn.patternId);
								if (nestedSeq && !nestedSeq->frames.empty()) {
									// Calculate how long this nested pattern runs
									int nestedMaxTicks = spawn.lifetime < 9999 ? spawn.lifetime : maxTicks - currentTick;
									if (nestedMaxTicks > 0) {
										// Recursively collect spawns from nested pattern
										auto nestedSpawns = CollectAllSpawnTicks(
											mainFrameData, effectFrameData, spawn.patternId,
											nestedMaxTicks, spawn.usesEffectHA6, absoluteSpawnTick);
										
										// Merge nested spawns into our result
										for (const auto& nestedPair : nestedSpawns) {
											spawnTicks[nestedPair.first].insert(
												spawnTicks[nestedPair.first].end(),
												nestedPair.second.begin(),
												nestedPair.second.end()
											);
										}
									}
								}
							}
						}
					}
				}
			}
		}
		
		// Advance animation using actual Hantei logic
		if (currentFrame < 0 || currentFrame >= seq->frames.size()) {
			break;
		}
		
		auto& frame = seq->frames[currentFrame];
		int frameDur = frame.AF.duration;
		if (frameDur <= 0) frameDur = 1;
		
		if (frameDuration >= frameDur) {
			frameDuration = 0;
			
			// Calculate next frame
			int nextFrame = currentFrame;
			if (frame.AF.aniType == 1) {
				if (currentFrame + 1 >= seq->frames.size()) {
					break;  // Reached end
				}
				nextFrame = currentFrame + 1;
			}
			else if (frame.AF.aniType == 2) {
				if ((frame.AF.aniFlag & 0x2) && loopCounter < 0) {
					if (frame.AF.aniFlag & 0x8) {
						nextFrame = currentFrame + frame.AF.loopEnd;
					} else {
						nextFrame = frame.AF.loopEnd;
					}
				} else {
					if (frame.AF.aniFlag & 0x2) {
						loopCounter--;
					}
					if (frame.AF.aniFlag & 0x4) {
						nextFrame = currentFrame + frame.AF.jump;
					} else {
						nextFrame = frame.AF.jump;
					}
				}
			} else {
				break;  // aniType 0 - stops
			}
			
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
	
	return spawnTicks;
}

// Helper to calculate frame from tick position
int CalculateFrameFromTick(FrameData* frameData, int patternId, int tick)
{
	// Use flow simulation to properly handle loops/jumps
	return SimulateAnimationFlow(frameData, patternId, tick);
}

// Simulate animation and create spawns up to target tick (for seeking)
// Populates activeSpawns with spawns that would exist at targetTick
void SimulateSpawnsToTick(
	FrameData* mainFrameData,
	FrameData* effectFrameData,
	int patternId,
	int targetTick,
	std::vector<ActiveSpawnInstance>& activeSpawns)
{
	if (!mainFrameData) return;
	
	auto seq = mainFrameData->get_sequence(patternId);
	if (!seq || seq->frames.empty()) return;
	
	// Clear existing spawns
	activeSpawns.clear();
	
	// Simulate animation from tick 0 to targetTick, creating spawns as we go
	int currentFrame = 0;
	int currentTick = 0;
	int loopCounter = 0;
	int frameDuration = 0;
	int lastFrameEntered = -1;
	
	// Initialize loop counter
	if (!seq->frames.empty() && seq->frames[0].AF.loopCount > 0) {
		loopCounter = seq->frames[0].AF.loopCount;
	}
	
	// Track spawns by composite key to avoid duplicates
	std::set<int> createdSpawnKeys;  // compositeKey + spawnTick
	
	while (currentTick <= targetTick) {
		// Check if we just entered a frame (frame boundary)
		// We need to create spawns every time we enter a frame, even if we've seen it before (loops)
		if (frameDuration == 0) {
			// Only create spawns if this is actually a new frame entry (not just continuing in the same frame)
			bool isNewFrameEntry = (currentFrame != lastFrameEntered);
			lastFrameEntered = currentFrame;
			
			if (isNewFrameEntry) {
			
			// Check if this frame has spawn effects
			if (currentFrame >= 0 && currentFrame < seq->frames.size()) {
				auto& frame = seq->frames[currentFrame];
				if (!frame.EF.empty()) {
					// Parse spawn effects
					auto frameSpawns = ParseSpawnedPatterns(frame.EF, currentFrame, patternId);
					
					// Create spawn instances for each spawn
					for (const auto& spawn : frameSpawns) {
						int compositeKey = spawn.patternId * 2 + (spawn.usesEffectHA6 ? 1 : 0);
						int spawnKey = compositeKey * 1000000 + currentTick;  // Unique key for this spawn at this tick
						
						// Only create if we haven't already created this spawn at this tick
						if (createdSpawnKeys.find(spawnKey) == createdSpawnKeys.end()) {
							createdSpawnKeys.insert(spawnKey);
							
							ActiveSpawnInstance instance;
							instance.spawnTick = currentTick;
							instance.patternId = spawn.patternId;
							instance.usesEffectHA6 = spawn.usesEffectHA6;
							instance.isPresetEffect = spawn.isPresetEffect;
							instance.offsetX = spawn.offsetX;
							instance.offsetY = spawn.offsetY;
							instance.flagset1 = spawn.flagset1;
							instance.flagset2 = spawn.flagset2;
							instance.angle = spawn.angle;
							instance.projVarDecrease = spawn.projVarDecrease;
							instance.tintColor = spawn.tintColor;
							instance.alpha = 1.0f;
							instance.currentFrame = 0;
							instance.frameDuration = 0;
							instance.previousFrame = -1;
							instance.loopCounter = 0;
							
							// Initialize loop counter and calculate current frame for spawned pattern
							if (!instance.isPresetEffect) {
								FrameData* sourceData = instance.usesEffectHA6 ? effectFrameData : mainFrameData;
								if (sourceData) {
									auto spawnSeq = sourceData->get_sequence(instance.patternId);
									if (spawnSeq && !spawnSeq->frames.empty()) {
										instance.loopCounter = spawnSeq->frames[0].AF.loopCount;
										
										// Calculate how many ticks have elapsed for this spawn
										int elapsedTicks = targetTick - currentTick;
										if (elapsedTicks > 0) {
											// Simulate the spawned pattern to find its current frame
											instance.currentFrame = SimulateAnimationFlow(sourceData, instance.patternId, elapsedTicks);
											// Calculate frame duration by simulating backwards
											// For now, just set a reasonable value
											instance.frameDuration = elapsedTicks;
										}
									}
								}
							}
							
							activeSpawns.push_back(instance);
						}
					}
				}
			}
			}
		}
		
		// Advance animation
		if (currentFrame < 0 || currentFrame >= seq->frames.size()) {
			break;
		}
		
		auto& frame = seq->frames[currentFrame];
		int frameDur = frame.AF.duration;
		if (frameDur <= 0) frameDur = 1;
		
		if (frameDuration >= frameDur) {
			frameDuration = 0;
			
			// Calculate next frame
			int nextFrame = currentFrame;
			if (frame.AF.aniType == 1) {
				if (currentFrame + 1 >= seq->frames.size()) {
					break;
				}
				nextFrame = currentFrame + 1;
			}
			else if (frame.AF.aniType == 2) {
				if ((frame.AF.aniFlag & 0x2) && loopCounter < 0) {
					if (frame.AF.aniFlag & 0x8) {
						nextFrame = currentFrame + frame.AF.loopEnd;
					} else {
						nextFrame = frame.AF.loopEnd;
					}
				} else {
					if (frame.AF.aniFlag & 0x2) {
						loopCounter--;
					}
					if (frame.AF.aniFlag & 0x4) {
						nextFrame = currentFrame + frame.AF.jump;
					} else {
						nextFrame = frame.AF.jump;
					}
				}
			} else {
				break;
			}
			
			if (nextFrame >= 0 && nextFrame < seq->frames.size()) {
				if (seq->frames[nextFrame].AF.loopCount > 0) {
					loopCounter = seq->frames[nextFrame].AF.loopCount;
				}
			}
			
			currentFrame = nextFrame;
			// Reset lastFrameEntered when we change frames so we can detect re-entry on loops
			lastFrameEntered = -1;
		} else {
			frameDuration++;
			currentTick++;
		}
	}
	
	// Now advance all spawns to their correct frames at targetTick
	for (auto& spawn : activeSpawns) {
		if (spawn.isPresetEffect) continue;
		
		int elapsedTicks = targetTick - spawn.spawnTick;
		if (elapsedTicks < 0) {
			// Spawn hasn't happened yet - remove it
			spawn.currentFrame = -1;
			continue;
		}
		
		FrameData* sourceData = spawn.usesEffectHA6 ? effectFrameData : mainFrameData;
		if (sourceData) {
			// Simulate the spawned pattern to find its current frame
			spawn.currentFrame = SimulateAnimationFlow(sourceData, spawn.patternId, elapsedTicks);
		}
	}
	
	// Remove invalid spawns
	for (auto it = activeSpawns.begin(); it != activeSpawns.end(); ) {
		if (it->currentFrame < 0) {
			it = activeSpawns.erase(it);
		} else {
			++it;
		}
	}
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

	// Remove pattern from visited set (allow it to be spawned in different branches)
	visitedPatterns.erase(patternId);
}
