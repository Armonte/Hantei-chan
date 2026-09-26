#ifndef AUTHORING_FRAME_SUMMARY_H_GUARD
#define AUTHORING_FRAME_SUMMARY_H_GUARD
// [authoring] Inline frame data for the moves a tuning points at (tag-in / tag-out patterns, assist actions):
// startup / active / recovery in game ticks, plus a per-frame strip (duration, attack box, hurt boxes, invulnerable)
// for a timeline widget (docs/HANTEI_AUTHORING_MODE.md §8.9 "inline frame data ... startup/active/recovery,
// hitboxes"). Read-only over the editor's FrameData (HA6 / MBAC HA4); base game files are never written (§8.5).
#include <string>
#include <vector>

class FrameData;

namespace authoring {

struct FrameStrip {
	int frame = 0;            // frame index in the pattern
	int startTick = 0;        // 0-based tick this frame starts on (in the walk below)
	int duration = 1;         // ticks
	bool attack = false;      // an attack box (hitbox kind "attack") on this frame
	bool hurt = false;        // a hurt box on this frame
	bool invuln = false;      // strike-invulnerable (no hurt box, or the frame's invulnerability flags)
	int boxes = 0;            // box count (all kinds)
};

struct MoveSummary {
	bool valid = false;       // the pattern exists and has frames
	int pattern = -1;
	std::string name;         // UTF-8 pattern name
	int frames = 0;
	int totalTicks = 0;       // ticks walked (to the pattern end, or to the first repeat of a loop)
	int startup = -1;         // ticks before the first attack tick (the first active tick is startup + 1); -1 = no attack
	int active = 0;           // ticks from the first to the last attack tick (inclusive)
	int recovery = 0;         // ticks after the last attack tick until the walk ends
	std::string activeText;   // active ranges, 1-based ticks: "12-14, 18-20"
	bool loops = false;       // the walk ended on a loop / jump back
	std::string note;         // e.g. "loops at frame 6", "ends by a jump to pattern 0"
	std::vector<FrameStrip> strip;
	std::string Text() const; // "startup 11, active 12-14 (3), recovery 20, total 34"
};

// Walk a pattern the way the game plays it on its own: frame by frame (AF duration), following the frame's
// jump / loop flags inside the pattern until the end, a jump out, or the first repeated frame.
MoveSummary SummarizePattern(const FrameData& fd, int pattern);

} // namespace authoring

#endif
