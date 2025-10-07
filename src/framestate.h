#ifndef FRAMESTATE_H_GUARD
#define FRAMESTATE_H_GUARD

#include "framedata.h"
#include "hitbox.h"
#include <linear_allocator.hpp>
#include <vector>
#include <set>
#include <glm/vec4.hpp>

struct CopyData {
	Frame_AS as{};
	Frame_AF_T<LinearAllocator> af{};
	Frame_T<LinearAllocator> frame{};
	Sequence_T<LinearAllocator> pattern{};
	std::vector<Frame_T<LinearAllocator>, LinearAllocator<Frame_T<LinearAllocator>>> frames{};
	Frame_AT at{};
	std::vector<Frame_EF, LinearAllocator<Frame_EF>> efGroup{};
	std::vector<Frame_IF, LinearAllocator<Frame_IF>> ifGroup{};
	Frame_IF ifSingle{};
	Frame_EF efSingle{};
	BoxList_T<LinearAllocator> boxes;
	Hitbox box;
};

// Spawned pattern visualization data
struct SpawnedPatternInfo {
	int effectIndex;      // Which effect spawned this
	int effectType;       // Effect type (1, 3, 8, 11, 101, 111, 1000)
	bool usesEffectHA6;   // True if type 8 (pulls from effect.ha6)
	bool isPresetEffect;  // True if type 3 (preset procedural effect, not pattern)
	int patternId;        // Pattern to spawn (or preset number for type 3)
	int offsetX, offsetY; // Position offset
	int flagset1;         // Spawn behavior flags
	int flagset2;         // Child property flags
	int angle;            // Rotation angle (clockwise, 10000 = 360°)
	int projVarDecrease;  // Projectile variable decrease on despawn
	int randomRange;      // For effect types 11/111

	// Parent frame tracking (stateless approach)
	int parentFrame;      // The parent frame where this spawn effect exists

	// Hierarchy tracking (for recursive spawn trees)
	int depth;                          // 0 = direct child, 1 = grandchild, 2 = great-grandchild, etc.
	int parentSpawnIndex;               // Index in spawnedPatterns array (-1 for root-level spawns)
	std::vector<int> childSpawnIndices; // Indices of children in spawnedPatterns array

	// Timeline tracking
	int absoluteSpawnFrame;             // Global frame when this spawns (relative to main pattern frame 0)
	int spawnTick;                      // Actual game tick when this spawns (sum of parent frame durations)
	int patternFrameCount;              // Total frames in the spawned pattern's sequence
	int lifetime;                       // Calculated frames this lives (respects aniType)
	bool isRecursive;                   // True if pattern spawns itself (cycle detected)

	// Visualization state
	bool visible;         // Show/hide this spawned pattern
	float alpha;          // Transparency (0.0 - 1.0)
	glm::vec4 tintColor;  // RGB tint color

	SpawnedPatternInfo() :
		effectIndex(-1), effectType(0), usesEffectHA6(false), isPresetEffect(false), patternId(-1), offsetX(0), offsetY(0),
		flagset1(0), flagset2(0), angle(0), projVarDecrease(0), randomRange(0),
		parentFrame(0),
		depth(0), parentSpawnIndex(-1),
		absoluteSpawnFrame(0), spawnTick(0), patternFrameCount(0), lifetime(0), isRecursive(false),
		visible(true), alpha(0.6f), tintColor(0.5f, 0.7f, 1.0f, 1.0f) {}
};

// Active spawn instance (created during animation when spawn effects fire)
struct ActiveSpawnInstance {
	int spawnTick;                // Game tick when this instance was created
	int patternId;                // Pattern being spawned (or preset number for type 3)
	bool usesEffectHA6;           // Whether to use effect.ha6 or main character data
	bool isPresetEffect;          // True if type 3 (preset procedural effect)
	int offsetX, offsetY;         // Spawn position offsets
	int flagset1, flagset2;       // Spawn flags
	int angle;                    // Rotation
	int projVarDecrease;          // Projectile variable
	glm::vec4 tintColor;          // Visualization tint
	float alpha;                  // Visualization alpha

	ActiveSpawnInstance() :
		spawnTick(0), patternId(-1), usesEffectHA6(false), isPresetEffect(false),
		offsetX(0), offsetY(0), flagset1(0), flagset2(0),
		angle(0), projVarDecrease(0),
		tintColor(0.5f, 0.7f, 1.0f, 1.0f), alpha(0.6f) {}
};

// Visualization settings
struct VisualizationSettings {
	bool showSpawnedPatterns;    // Master toggle
	bool showPresetEffects;       // Show Effect Type 3 preset effects
	bool presetEffectsAllFrames;  // Show Type 3 on all frames (vs spawn frame only)
	bool autoDetect;              // Auto-detect from effects
	bool showOffsetLines;         // Show lines from parent to spawned
	bool showLabels;              // Show pattern ID labels
	bool animateWithMain;         // Sync animation with main pattern
	float spawnedOpacity;         // Global opacity multiplier

	// Timeline settings
	bool showTimeline;            // Show timeline view
	int timelineZoom;             // Frames per unit

	VisualizationSettings() :
		showSpawnedPatterns(true), showPresetEffects(true), presetEffectsAllFrames(false),
		autoDetect(true), showOffsetLines(true), showLabels(true),
		animateWithMain(true), spawnedOpacity(1.0f),
		showTimeline(false), timelineZoom(1) {}
};

struct FrameState
{
	int pattern;
	int frame;
	int spriteId;
	int selectedLayer = 0;  // Track currently selected layer in UI

	// Animation playback
	bool animating = false;
	int animeSeq = 0;
	int currentTick = 0;  // Actual game ticks elapsed (accounts for frame durations)
	int previousFrame = -1;  // Track previous frame for spawn detection

	// Spawned pattern visualization (static tree for UI)
	std::vector<SpawnedPatternInfo> spawnedPatterns;
	VisualizationSettings vizSettings;
	int selectedSpawnedPattern = -1;  // Currently selected in UI
	bool forceSpawnTreeRebuild = false;  // Set by undo/redo to force rebuild

	// Active spawn instances (created dynamically during animation)
	std::vector<ActiveSpawnInstance> activeSpawns;

	// Shared memory clipboard for cross-instance copy/paste
	CopyData *copied;

	FrameState();
	~FrameState();

private:
	void *sharedMemHandle = nullptr;
	void *sharedMem = nullptr;
};

// Utility function to parse spawned patterns from effects (single frame)
std::vector<SpawnedPatternInfo> ParseSpawnedPatterns(const std::vector<Frame_EF>& effects, int parentFrame, int parentPatternId = -1);

// Helper to calculate tick position from frame number (sums frame durations)
int CalculateTickFromFrame(class FrameData* frameData, int patternId, int frameNum);

// Helper to calculate frame from tick position
int CalculateFrameFromTick(class FrameData* frameData, int patternId, int tick);

// Recursive function to build full spawn tree
void BuildSpawnTreeRecursive(
	class FrameData* mainFrameData,
	class FrameData* effectFrameData,
	int patternId,
	bool usesEffectHA6,
	int parentSpawnIndex,
	int depth,
	int absoluteSpawnFrame,
	int absoluteSpawnTick,
	int accumulatedOffsetX,
	int accumulatedOffsetY,
	std::vector<SpawnedPatternInfo>& allSpawns,
	std::set<int>& visitedPatterns);

#endif /* FRAMESTATE_H_GUARD */
