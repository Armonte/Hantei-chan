#include <imgui.h>
#include "imgui_utils.h"
#include "framedata.h"



inline void AtDisplay(Frame_AT *at)
{
	const char* const hitEffectList[] = {
		"Weak punch",
		"Medium punch",
		"Strong punch",
		"Weak kick",
		"Medium kick",
		"Strong kick",
		"Super punch",
		"Super kick",
		"Slash (sparks)",
		"Burn",
		"Freeze",
		"Shock",
		"Big light (SE only?)",
		"Small light (SE only?)",
		"None",
		"Strong hit",
		"Double slash",
		"Super slash",
		"Tiny cut",
		"Fat cut",
		"Big fat cut",
		"Faint wave",
		//Are there more is it OOB?
	};

	const char* const addedEffectList[] = {
		"None",
		"Burn",
		"Freeze",
		"Shock",
		"Confuse"
	};

	const char* const vectorFlags[] = {
		"Default",
		"Reverse",
		"Nullify",
		"Both?"
	};

	const char* const hitStopList[] = {
		"Weak",
		"Medium",
		"Strong",
		"None",
		"Stronger",
		"Strongest",
		"Weakest"
	};
	
	constexpr float width = 75.f;
	unsigned int flagIndex = -1;

	BitField("Guard Flags", &at->guard_flags, &flagIndex);
	switch (flagIndex)
	{
		case 0: Tooltip("Stand blockable"); break;
		case 1: Tooltip("Air blockable"); break;
		case 2: Tooltip("Crouch blockable"); break;
		case 8: Tooltip("Miss if enemy is standing?"); break;
		case 9: Tooltip("Miss if enemy is airborne?"); break;
		case 10: Tooltip("Miss if enemy is crouching?"); break;
		case 11: Tooltip("Miss if enemy is in bound?"); break;
		case 12: Tooltip("Miss if enemy is in blockstun?"); break;
		case 13: Tooltip("Miss if OTG"); break;
		case 14: Tooltip("Hit only in bound?"); break;
		case 15: Tooltip("Can't hit playable character?"); break;
	}

	flagIndex = -1;
	BitField("Hit Flags", &at->otherFlags, &flagIndex);
	switch (flagIndex)
	{
		case 0: Tooltip("Enable chip damage"); break;
		case 1: Tooltip("Can't KO"); break;
		case 2: Tooltip("Make enemy unhittable"); break;
		case 3: Tooltip("Can't be clashed with"); break;
		case 4: Tooltip("Auto super jump cancel"); break;
		case 5: Tooltip("Don't increase combo counter"); break;
		case 6: Tooltip("Shake the screen on hit"); break;
		case 7: Tooltip("Not air techable"); break;
		case 8: Tooltip("Not ground techable (HKD)"); break;
		case 9: Tooltip("Friendly fire?"); break;
		case 10: Tooltip("No self hitstop"); break;
		case 16: Tooltip("Use custom blockstop"); break;
		case 22: Tooltip("Mystery flag 22"); break;
		case 25: Tooltip("No hitstop on multihit?"); break;
		case 29: Tooltip("Block enemy blast during Stun?"); break;
	}

	ImGui::InputInt("Blockstop", &at->blockStopTime, 0,0);

	ImGui::SetNextItemWidth(width*2);
	ImGui::Combo("Hitstop", &at->hitStop, hitStopList, IM_ARRAYSIZE(hitStopList)); ImGui::SameLine(0.f, 20);
	ImGui::SetNextItemWidth(width);
	ImGui::InputInt("Custom##Hitstop", &at->hitStopTime, 0,0);


	ImGui::SetNextItemWidth(width);
	ImGui::InputInt("Untech time", &at->untechTime, 0,0);  ImGui::SameLine(0.f, 20); ImGui::SetNextItemWidth(width);
	ImGui::InputInt("Circuit break time", &at->breakTime, 0,0);

	ImGui::SetNextItemWidth(width);
	ImGui::InputFloat("Extra gravity", &at->extraGravity, 0,0); ImGui::SameLine(0.f, 20);
	ImGui::Checkbox("Hitgrab", &at->hitgrab);
	

	ImGui::SetNextItemWidth(width);
	ImGui::InputInt("Correction %", &at->correction, 0, 0); ImGui::SameLine(0.f, 20); ImGui::SetNextItemWidth(width*2);
	ImGui::Combo("Type##Correction", &at->correction_type, "Normal\0Multiplicative\0Substractive\0");

	ImGui::SetNextItemWidth(width);
	ImGui::InputInt("VS damage", &at->red_damage, 0, 0); ImGui::SameLine(0.f, 20); ImGui::SetNextItemWidth(width);
	ImGui::InputInt("Damage", &at->damage, 0, 0);

	ImGui::SetNextItemWidth(width);
	ImGui::InputInt("Guard damage", &at->guard_damage, 0, 0); ImGui::SameLine(0.f, 20); ImGui::SetNextItemWidth(width);
	ImGui::InputInt("Meter gain", &at->meter_gain, 0, 0);

	ImGui::Separator();
	auto comboWidth = (ImGui::GetWindowWidth())/4.f;
	ImGui::InputInt3("Guard Vector", at->guardVector);
	for(int i = 0; i < 3; i++)
	{	
		ImGui::SetNextItemWidth(comboWidth);
		if(i > 0)
			ImGui::SameLine();
		ImGui::PushID(i); 
		ImGui::Combo("##GFLAG", &at->gVFlags[i], vectorFlags, IM_ARRAYSIZE(vectorFlags));
		ImGui::PopID();
	}

	ImGui::Separator();
	ImGui::InputInt3("Hit Vector", at->hitVector);
	ImGui::SameLine(); ImGui::TextDisabled("(?)");
	if(ImGui::IsItemHovered())
		Tooltip("Stand, air and crouch.\nSee vector text file.");
	
	for(int i = 0; i < 3; i++)
	{	
		ImGui::SetNextItemWidth(comboWidth);
		if(i > 0)
			ImGui::SameLine();
		ImGui::PushID(i); 
		ImGui::Combo("##HFLAG", &at->hVFlags[i], vectorFlags, IM_ARRAYSIZE(vectorFlags));
		ImGui::PopID();
	}
	ImGui::Separator();
	
	ImGui::SetNextItemWidth(150);
	ImGui::Combo("Hit effect", &at->hitEffect, hitEffectList, IM_ARRAYSIZE(hitEffectList)); ImGui::SameLine(0, 20.f);
	ImGui::SetNextItemWidth(70);
	ImGui::InputInt("ID##Hit effect", &at->hitEffect, 0, 0); 
	
	ImGui::SetNextItemWidth(70);
	ImGui::InputInt("Sound effect", &at->soundEffect, 0, 0); ImGui::SameLine(0, 20.f); ImGui::SetNextItemWidth(120);

	ImGui::Combo("Added effect", &at->addedEffect, addedEffectList, IM_ARRAYSIZE(addedEffectList));



}

inline void AfDisplay(Frame_AF *af)
{
	const char* const interpolationList[] = {
		"None",
		"Linear",
		"Slow->Fast",
		"Fast->Slow",
		"Fast middle",
		"Slow middle", //Not used, but it works.
	};

	constexpr float width = 50.f;
	ImGui::InputInt("AFRT", &af->AFRT, 0, 0);

	ImGui::SetNextItemWidth(width*3);
	ImGui::InputInt("Sprite", &af->spriteId); ImGui::SameLine(0, 20.f);
	ImGui::Checkbox("Use .pat", &af->usePat);

	
	ImGui::Combo("Interpolation", &af->interpolationType, interpolationList, IM_ARRAYSIZE(interpolationList));

	ImGui::SetNextItemWidth(width);
	ImGui::InputInt("Jump to", &af->jump, 0, 0); ImGui::SameLine(0.f, 20); ImGui::SetNextItemWidth(width);
	ImGui::InputInt("Landing frame", &af->landJump, 0, 0);

	ImGui::SetNextItemWidth(width);
	ImGui::InputInt("Priority", &af->priority, 0, 0); ImGui::SetNextItemWidth(width);
	ImGui::InputInt("Loop N times", &af->loopCount, 0, 0); ImGui::SameLine(0,20); ImGui::SetNextItemWidth(width);
	ImGui::InputInt("End of loop", &af->loopEnd, 0, 0);

	ImGui::SetNextItemWidth(width);
	ImGui::DragInt("X", &af->offset_x);
	ImGui::SameLine();
	ImGui::SetNextItemWidth(width);
	ImGui::DragInt("Y", &af->offset_y);

	ImGui::InputInt("Duration", &af->duration, 0, 0);

	ImGui::InputInt("AniFlag", &af->aniFlag, 0, 0);

	int mode = af->blend_mode-1;
	if(mode < 1)
		mode = 0;
	if (ImGui::Combo("Blend Mode", &mode, "Normal\0Additive\0Substractive\0"))
	{
		af->blend_mode=mode+1;
	}
	ImGui::ColorEdit4("Color", af->rgba);

	ImGui::DragFloat3("Rot XYZ", af->rotation, 0.005); 
	ImGui::DragFloat2("Scale", af->scale, 0.1);
}