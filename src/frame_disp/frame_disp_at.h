#ifndef FRAME_DISP_AT_H_GUARD
#define FRAME_DISP_AT_H_GUARD

#include "frame_disp_common.h"
#include "../cg.h"

// ============================================================================
// Attack (AT) Display
// ============================================================================
// Displays and edits Frame_AT data (hitboxes, damage, vectors, properties)
//
// Frame_AT controls:
// - Guard/Hit flags (blockability, hit conditions)
// - Damage values (HP, meter, guard damage)
// - Hitstop and untech time
// - Hit/guard vectors (knockback, blockstun)
// - Hit effects and sounds
// - Correction (proration/scaling)
// ============================================================================

inline void AtDisplay(Frame_AT *at, FrameData *frameData = nullptr, int patternIndex = -1, std::function<void()> onModified = nullptr)
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

	
	constexpr float width = 75.f;
	unsigned int flagIndex = -1;

	if(BitField("Guard Flags", &at->guard_flags, &flagIndex)) {
		markModified();
	}
	switch (flagIndex)
	{
		case 0: Tooltip("Stand blockable"); break;
		case 1: Tooltip("Air blockable"); break;
		case 2: Tooltip("Crouch blockable"); break;
		case 8: Tooltip("Miss if enemy is standing"); break;
		case 9: Tooltip("Miss if enemy is airborne"); break;
		case 10: Tooltip("Miss if enemy is crouching"); break;
		case 11: Tooltip("Miss if enemy is in hitstun (includes blockstun)"); break;
		case 12: Tooltip("Miss if enemy is in blockstun"); break;
		case 13: Tooltip("Miss if OTG"); break;
		case 14: Tooltip("Hit only in hitstun"); break;
		case 15: Tooltip("Can't hit playable character"); break;
	}

	flagIndex = -1;
	if(BitField("Hit Flags", &at->otherFlags, &flagIndex)) {
		markModified();
	}
	switch (flagIndex)
	{
		case 0: Tooltip("Chip health instead of red health"); break;
		case 1: Tooltip("Can't KO"); break;
		case 2: Tooltip("Make enemy unhittable"); break;
		case 3: Tooltip("Can't be clashed with"); break;
		case 4: Tooltip("Auto super jump cancel"); break;
		case 5: Tooltip("Don't increase combo counter"); break;
		case 6: Tooltip("Shake the screen on hit"); break;
		case 7: Tooltip("Not air techable"); break;
		case 8: Tooltip("Not ground techable (HKD)"); break;
		case 9: Tooltip("Friendly fire"); break;
		case 10: Tooltip("No self hitstop"); break;

		case 12: Tooltip("Lock burst"); break;
		case 13: Tooltip("Can't be shielded"); break;
		case 14: Tooltip("Can't critical"); break;

		case 16: Tooltip("Use custom blockstop"); break;
		case 17: Tooltip("OTG Relaunch"); break;
		case 18: Tooltip("Can't counterhit"); break;
		case 19: Tooltip("Unknown"); break;
		case 20: Tooltip("Use Type 2 Circuit Break"); break;
		case 21: Tooltip("Unknown"); break;
		case 22: Tooltip("Remove 1f of untech"); break;

		//Unused or don't exist in melty.
		//case 25: Tooltip("No hitstop on multihit?"); break;
		//case 29: Tooltip("Block enemy blast during Stun?"); break;
	}

	im::SetNextItemWidth(width * 2);
	if(im::InputInt("Custom Blockstop", &at->blockStopTime, 0,0)) {
		markModified();
	}

	im::SetNextItemWidth(width*2);
	if(im::Combo("Hitstop", &at->hitStop, hitStopList, IM_ARRAYSIZE(hitStopList))) {
		markModified();
	}
	im::SameLine(0.f, 20);
	im::SetNextItemWidth(width);
	if(im::InputInt("Custom##Hitstop", &at->hitStopTime, 0,0)) {
		markModified();
	}


	im::SetNextItemWidth(width);
	if(im::InputInt("Untech time", &at->untechTime, 0,0)) {
		markModified();
	}
	im::SameLine(0.f, 20); im::SetNextItemWidth(width);
	if(im::InputInt("Circuit break time", &at->breakTime, 0,0)) {
		markModified();
	}

	im::SetNextItemWidth(width);
	if(im::InputFloat("Extra gravity", &at->extraGravity, 0,0)) {
		markModified();
	}
	im::SameLine(0.f, 20);
	if(im::Checkbox("Hitgrab", &at->hitgrab)) {
		markModified();
	}


	im::SetNextItemWidth(width);
	if(im::InputInt("Correction %", &at->correction, 0, 0)) {
		markModified();
	}
	im::SameLine(0.f, 20); im::SetNextItemWidth(width*2);
	if(im::Combo("Type##Correction", &at->correction_type, "Normal\0Multiplicative\0Subtractive\0")) {
		markModified();
	}

	im::SetNextItemWidth(width);
	if(im::InputInt("VS damage", &at->red_damage, 0, 0)) {
		markModified();
	}
	im::SameLine(0.f, 20); im::SetNextItemWidth(width);
	if(im::InputInt("Damage", &at->damage, 0, 0)) {
		markModified();
	}

	im::SetNextItemWidth(width);
	if(im::InputInt("Guard damage", &at->guard_damage, 0, 0)) {
		markModified();
	}
	im::SameLine(0.f, 20); im::SetNextItemWidth(width);
	if(im::InputInt("Meter gain", &at->meter_gain, 0, 0)) {
		markModified();
	}

	im::Separator();
	auto comboWidth = (im::GetWindowWidth())/4.f;

	// Guard Vector
	im::Text("Guard Vector:");
	im::SameLine(0.f, 20);
	static bool guardVectorManual = false;
	if(im::Checkbox("Manual##gv", &guardVectorManual)) {
	}
	if(im::IsItemHovered()) {
		Tooltip("Enable manual vector entry");
	}

	static const char* const vectorLabels[] = {"Stand", "Air", "Crouch"};
	if(guardVectorManual) {
		// Manual mode - show InputInt3
		if(im::InputInt3("##guardVec", at->guardVector)) {
			markModified();
		}
	} else {
		// Dropdown mode - Stand, Air, Crouch with dropdowns
		for(int i = 0; i < 3; i++)
		{
			im::PushID(100+i);
			if(ShowComboWithManual(vectorLabels[i], &at->guardVector[i], hitVectorList, IM_ARRAYSIZE(hitVectorList), width*6, width)) {
			markModified();
		}
			im::PopID();
		}
	}

	// Guard Vector Flags
	for(int i = 0; i < 3; i++)
	{
		im::SetNextItemWidth(comboWidth);
		if(i > 0)
			im::SameLine();
		im::PushID(i);
		if(im::Combo("##GFLAG", &at->gVFlags[i], vectorFlags, IM_ARRAYSIZE(vectorFlags))) {
		markModified();
	}
		im::PopID();
	}

	im::Separator();

	// Hit Vector
	im::Text("Hit Vector:");
	im::SameLine(0.f, 20);
	static bool hitVectorManual = false;
	if(im::Checkbox("Manual##hv", &hitVectorManual)) {
	}
	if(im::IsItemHovered()) {
		Tooltip("Enable manual vector entry");
	}

	if(hitVectorManual) {
		// Manual mode - show InputInt3
		if(im::InputInt3("##hitVec", at->hitVector)) {
			markModified();
		}
	} else {
		// Dropdown mode - Stand, Air, Crouch with dropdowns
		for(int i = 0; i < 3; i++)
		{
			im::PushID(200+i);
			if(ShowComboWithManual(vectorLabels[i], &at->hitVector[i], hitVectorList, IM_ARRAYSIZE(hitVectorList), width*6, width)) {
			markModified();
		}
			im::PopID();
		}
	}

	// Hit Vector Flags
	for(int i = 0; i < 3; i++)
	{
		im::SetNextItemWidth(comboWidth);
		if(i > 0)
			im::SameLine();
		im::PushID(i);
		if(im::Combo("##HFLAG", &at->hVFlags[i], vectorFlags, IM_ARRAYSIZE(vectorFlags))) {
		markModified();
	}
		im::PopID();
	}
	im::Separator();

	im::SetNextItemWidth(150);
	if(im::Combo("Hit effect", &at->hitEffect, hitEffectList, IM_ARRAYSIZE(hitEffectList))) {
		markModified();
	}
	im::SameLine(0, 20.f);
	im::SetNextItemWidth(70);
	if(im::InputInt("ID##Hit effect", &at->hitEffect, 0, 0)) {
		markModified();
	}

	im::SetNextItemWidth(70);
	if(im::InputInt("Sound effect", &at->soundEffect, 0, 0)) {
		markModified();
	}
	im::SameLine(0, 20.f); im::SetNextItemWidth(120);

	if(im::Combo("Added effect", &at->addedEffect, addedEffectList, IM_ARRAYSIZE(addedEffectList))) {
		markModified();
	}



}

#endif /* FRAME_DISP_AT_H_GUARD */

