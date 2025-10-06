#ifndef FRAME_DISP_AS_H_GUARD
#define FRAME_DISP_AS_H_GUARD

#include "frame_disp_common.h"
#include "../cg.h"

// ============================================================================
// Action State (AS) Display
// ============================================================================
// Displays and edits Frame_AS data (movement, physics, cancels, state)
//
// Frame_AS controls:
// - Movement vectors (speed, acceleration, max speed)
// - Physics flags (momentum, ground tech, guard points)
// - State properties (standing/airborne/crouching)
// - Cancel conditions (normals, specials)
// - Invincibility frames
// - Sinewave movement
// ============================================================================

inline void AsDisplay(Frame_AS *as, FrameData *frameData = nullptr, int patternIndex = -1, std::function<void()> onModified = nullptr)
{
	// Helper lambda to mark both frameData and character as modified
	auto markModified = [&]() {
		if (frameData && patternIndex >= 0) {
			frameData->mark_modified(patternIndex);
		}
		if (onModified) {
			onModified();
		}
	};

	constexpr float width = 103.f;

	unsigned int flagIndex = -1;
	if(BitField("Movement Flags", &as->movementFlags, &flagIndex, 8)) {
		markModified();
	}
	switch (flagIndex)
	{
		case 0: Tooltip("Set Y"); break;
		case 4: Tooltip("Set X"); break;
		case 1: Tooltip("Add Y"); break;
		case 5: Tooltip("Add X"); break;
	}

	im::SetNextItemWidth(width*2);
	if(im::InputInt2("Speed", as->speed)) {
		markModified();
	}
	im::SameLine(0.f, 20); im::SetNextItemWidth(width);
	if(im::InputInt("Max X speed", &as->maxSpeedX, 0, 0)) {
		markModified();
	}
	im::SetNextItemWidth(width*2);
	if(im::InputInt2("Accel", as->accel)) {
		markModified();
	}
	
	im::Separator();
	flagIndex = -1;
	if(BitField("Flagset 1", &as->statusFlags[0], &flagIndex)) {
		markModified();
	}
	switch (flagIndex)
	{
		case 0: Tooltip("Vector influences other animations (dash momentum)"); break;
		case 1: Tooltip("Force clean vector (kill dash momentum)"); break;
		case 2: Tooltip("Don't transition to walking"); break;
		case 4: Tooltip("Can ground tech"); break;
		case 5: Tooltip("Unknown"); break;
		case 8: Tooltip("Guard point high and mid"); break;
		case 9: Tooltip("Guard point air blockable"); break;
		case 12: Tooltip("Unknown"); break;
		case 31: Tooltip("Vector initialization only at the beginning (?)"); break;
	}

	flagIndex = -1;
	if(BitField("Flagset 2", &as->statusFlags[1], &flagIndex)) {
		markModified();
	}
	switch (flagIndex)
	{
		case 0: Tooltip("Can always EX cancel"); break;
		case 2: Tooltip("Can only jump cancel"); break;
		case 31: Tooltip("Can't block"); break;
	}

	im::SetNextItemWidth(width);
	if(im::InputInt("Number of hits", &as->hitsNumber, 0, 0)) {
		markModified();
	}
	im::SameLine(0,20.f);
	if(im::Checkbox("Player can move", &as->canMove)) {
		markModified();
	}
	if(im::Combo("State", &as->stanceState, stateList, IM_ARRAYSIZE(stateList))) {
		markModified();
	}
	if(im::Combo("Invincibility", &as->invincibility, invulList, IM_ARRAYSIZE(invulList))) {
		markModified();
	}
	if(im::Combo("Counterhit", &as->counterType, counterList, IM_ARRAYSIZE(counterList))) {
		markModified();
	}
	if(im::Combo("Cancel normal", &as->cancelNormal, cancelList, IM_ARRAYSIZE(cancelList))) {
		markModified();
	}
	if(im::Combo("Cancel special", &as->cancelSpecial, cancelList, IM_ARRAYSIZE(cancelList))) {
		markModified();
	}
	

	im::Separator();
	flagIndex = -1;
	if(BitField("Sine flags", &as->sineFlags, &flagIndex, 8)) {
		markModified();
	}
	switch (flagIndex)
	{
		case 0: Tooltip("Use Y"); break;
		case 4: Tooltip("Use X"); break;
	}
	if(im::InputInt4("Sinewave", as->sineParameters)) {
		markModified();
	}
	im::SameLine();
	im::TextDisabled("(?)");
	if(im::IsItemHovered())
		Tooltip("Sine parameters:\nX dist, Y dist\nX frequency, Y frequency");
	if(im::InputFloat2("Phases", as->sinePhases)) {
		markModified();
	}
}

#endif /* FRAME_DISP_AS_H_GUARD */

