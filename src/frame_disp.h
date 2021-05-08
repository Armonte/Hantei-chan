#include <imgui.h>
#include "imgui_utils.h"
#include "framedata.h"



inline void AtDisplay(Frame_AT *at)
{
	static const char* const hitEffectList[] =
	{
		"Weak punch",
		"Medium punch",
		"Strong punch",
		"Weak kick",
		"Medium kick",
		"Strong kick",
		"Super punch",
		"Super kick",
		"Cut",
		"Burn",
		"Freeze",
		"Shock",
		"Big light",
		"Small light",
		"None",
		"hikari mabiki",
		"kiri mabiki"
		//Figure out the rest? Ryougi uses new ones
	};
	static const char* const addedEffectList[] =
	{
		"None",
		"Burn",
		"Freeze",
		"Shock",
		"Confuse"
	};
	
	constexpr float width = 75.f;
	unsigned int flagIndex = -1;
	BitField("Guard Flags", &at->guard_flags, &flagIndex);
	switch (flagIndex)
	{
		case 0: Tooltip("Stand blockable"); break;
		case 1: Tooltip("Air blockable"); break;
		case 2: Tooltip("Crouch blockable"); break;
		case 13: Tooltip("Miss if OTG"); break;
		//TODO: Figure out the rest lol
	}

	flagIndex = -1;
	BitField("Hit Flags", &at->otherFlags, &flagIndex);
	switch (flagIndex)
	{
		case 0: Tooltip("Enable chip damage"); break;
		case 1: Tooltip("Can't KO(?)"); break;
		case 2: Tooltip("Can't hit enemy on stun(?)"); break;
		case 3: Tooltip("Not to be matched with? Not counterhitable?"); break;

		case 5: Tooltip("Don't increase combo counter(?)"); break;
		case 6: Tooltip("Shake the screen on hit"); break;
		case 8: Tooltip("Not ground techable (HKD)"); break;
		case 9: Tooltip("Friendly fire(?)"); break;
		case 10: Tooltip("No self hitstop"); break;
		//TODO: Figure out the rest lol
	}

	ImGui::Checkbox("Hitgrab", &at->hitgrab);

	ImGui::SetNextItemWidth(width);
	ImGui::InputInt("Correction %", &at->correction, 0, 0);
	ImGui::Combo("Correction type", &at->correction_type, "Normal\0Multiplicative\0Substractive\0");

	ImGui::SetNextItemWidth(width);
	ImGui::InputInt("VS damage", &at->red_damage, 0, 0); ImGui::SameLine(0.f, 20); ImGui::SetNextItemWidth(width);
	ImGui::InputInt("Damage", &at->damage, 0, 0);

	ImGui::SetNextItemWidth(width);
	ImGui::InputInt("Guard damage", &at->guard_damage, 0, 0); ImGui::SameLine(0.f, 20); ImGui::SetNextItemWidth(width);
	ImGui::InputInt("Meter gain", &at->meter_gain, 0, 0);


	ImGui::InputInt3("Guard Vector", at->guardVector);
	ImGui::InputInt3("Hit Vector", at->hitVector);
	ImGui::SameLine(); ImGui::TextDisabled("(?)");
	if(ImGui::IsItemHovered())
		Tooltip("Stand, air and crouch.\nSee vector text file.");
	
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
	constexpr float width = 50.f;
	//ImGui::SetNextItemWidth(width);
	ImGui::InputInt("AFRT", &af->AFRT, 0, 0);
	ImGui::InputInt("Sprite", &af->spriteId);
	
	ImGui::Checkbox("Use .pat", &af->usePat); ImGui::SameLine(0.f, 20);
	ImGui::Checkbox("Interpolate", &af->interpolation);

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