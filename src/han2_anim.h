#ifndef HAN2_ANIM_H_GUARD
#define HAN2_ANIM_H_GUARD

// Game-rule animation playback for RBO / GOF2 characters: steps the pattern exactly the way the engine's frame advance does
// (Actor_TickFrame 0x43F830 + Actor_AdvanceByAniFlag 0x43F640: tick counter vs frame duration, then the ani flag decides next frame,
// jump, loop or the pattern to jump to). Runs on a fixed logic rate independent of the render rate.
#include <cstdint>

class FrameData;

namespace han2 {

struct AnimState {
	int pattern = 0, frame = 0;
	int ticksInFrame = 0;     // logic ticks spent in the current frame
	int loopCounter = 0;      // ANI_LOOP_COUNTED repeat counter (frame.loopCount)
	int totalTicks = 0;       // since the pattern was entered
	bool ended = false;       // a state-dependent / empty target was reached
	int jumpedToPattern = -1; // last ANI_END_TO_PATTERN target
};

// Resets to frame 0 of `pattern`.
void AnimStart(const FrameData &fd, AnimState &s, int pattern);
// One logic tick. Returns true when the visible frame changed. followPatternJumps: continue into the pattern an "end" frame names.
bool AnimTick(const FrameData &fd, AnimState &s, bool followPatternJumps, bool loopPattern);
// Moves to the next / previous frame without waiting (does not touch totalTicks).
void AnimStepFrame(const FrameData &fd, AnimState &s, int dir);

} // namespace han2
#endif
