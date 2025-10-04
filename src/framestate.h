#ifndef FRAMESTATE_H_GUARD
#define FRAMESTATE_H_GUARD

#include "framedata.h"
#include "hitbox.h"
#include <linear_allocator.hpp>
#include <vector>
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
	int patternId;        // Pattern to spawn
	int offsetX, offsetY; // Position offset
	int flagset1;         // Spawn behavior flags
	int flagset2;         // Child property flags
	int angle;            // Rotation angle (clockwise, 10000 = 360°)
	int projVarDecrease;  // Projectile variable decrease on despawn
	int randomRange;      // For effect types 11/111

	// Parent frame tracking (stateless approach)
	int parentFrame;      // The parent frame where this spawn effect exists

	// Visualization state
	bool visible;         // Show/hide this spawned pattern
	float alpha;          // Transparency (0.0 - 1.0)
	glm::vec4 tintColor;  // RGB tint color

	SpawnedPatternInfo() :
		effectIndex(-1), patternId(-1), offsetX(0), offsetY(0),
		flagset1(0), flagset2(0), angle(0), projVarDecrease(0), randomRange(0),
		parentFrame(0),
		visible(true), alpha(0.6f), tintColor(0.5f, 0.7f, 1.0f, 1.0f) {}
};

// Visualization settings
struct VisualizationSettings {
	bool showSpawnedPatterns;    // Master toggle
	bool autoDetect;              // Auto-detect from effects
	bool showOffsetLines;         // Show lines from parent to spawned
	bool showLabels;              // Show pattern ID labels
	bool animateWithMain;         // Sync animation with main pattern
	float spawnedOpacity;         // Global opacity multiplier

	// Timeline settings
	bool showTimeline;            // Show timeline view
	int timelineZoom;             // Frames per unit

	VisualizationSettings() :
		showSpawnedPatterns(true), autoDetect(true),
		showOffsetLines(true), showLabels(true),
		animateWithMain(true), spawnedOpacity(0.6f),
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

	// Spawned pattern visualization
	std::vector<SpawnedPatternInfo> spawnedPatterns;
	VisualizationSettings vizSettings;
	int selectedSpawnedPattern = -1;  // Currently selected in UI

	// Shared memory clipboard for cross-instance copy/paste
	CopyData *copied;

	FrameState();
	~FrameState();

private:
	void *sharedMemHandle = nullptr;
	void *sharedMem = nullptr;
};

// Utility function to parse spawned patterns from effects
std::vector<SpawnedPatternInfo> ParseSpawnedPatterns(const std::vector<Frame_EF>& effects, int parentFrame);

#endif /* FRAMESTATE_H_GUARD */
