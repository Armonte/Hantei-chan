#include <imgui.h>
#include <imgui_stdlib.h>
#include "framedata.h"
#include "imgui_utils.h"

inline void PatternDisplay(Sequence *seq, FrameData *frameData = nullptr, int patternIndex = -1)
{
	constexpr float spacing = 50.0f;  // Compact width for small integer fields
	//ImGui::InputText("Code name", &seq->codeName);
	
	ImGui::SetNextItemWidth(spacing);
	if(ImGui::InputInt("PSTS", &seq->psts, 0, 0) && frameData && patternIndex >= 0) {
		frameData->mark_modified(patternIndex);
	}
	ImGui::SameLine(); ImGui::TextDisabled("(?)");
	if (ImGui::IsItemHovered())
		Tooltip("Determines shield properties\n"
			"0: Must be shielded according to stance\n"
			"1: Must be shielded according to level and stance (Level 0 can be shielded both ways)\n"
			"5: Must be shielded according to stance. Do not trigger H-Moon Shield Counter\n"
			"Projectiles can be shieded either way regardless");
	ImGui::SameLine();

	ImGui::SetNextItemWidth(spacing);
	if(ImGui::InputInt("Level", &seq->level, 0, 0) && frameData && patternIndex >= 0) {
		frameData->mark_modified(patternIndex);
	}
	ImGui::SameLine(); ImGui::TextDisabled("(?)");
	if (ImGui::IsItemHovered())
		Tooltip("Determines cancel and shield properties\n"
		"F Moon: Attacks can be canceled into higher level attacks only (0 can cancel into 0)\n"
		"C Moon: Rebeat penalty applied for canceling into lower level attacks\n"
		"Level 0 can be shielded either way if PSTS = 1\n"
		"Level 0 can be self canceled");
	ImGui::SameLine();

	const bool uni = frameData && frameData->usesUniFormat();
	ImGui::SetNextItemWidth(spacing);
	if(ImGui::InputInt(uni ? "PFLG" : "Flag", &seq->flag, 0, 0) && frameData && patternIndex >= 0) {
		frameData->mark_modified(patternIndex);
	}
	ImGui::SameLine(); ImGui::TextDisabled("(?)");
	if (ImGui::IsItemHovered())
	{
		if (uni)
			Tooltip("PFLG bit 0 = template pattern (UNI2/MBTL BaseData, names marked with a star).\n"
			"When the game loads a character, a template keeps its own frames (timing,\n"
			"state, attack data) and takes the layer sprites/offsets and the boxes from\n"
			"the character's pattern in the same slot (Han6_MergeTemplatePattern_PFLG).");
		else
			Tooltip("Info for the editor about the type of move\n"
			"No use in game - has no effect during gameplay\n"
			"Preset values: only 0, 1, or 2 used in Melty Blood");
	}

	if (uni)
	{
		// PSTS/PLVL are read and ignored by the UNI2/MBTL loaders.
		ImGui::SetNextItemWidth(spacing);
		if(ImGui::InputInt("PUPS", &seq->pups, 0, 0) && frameData && patternIndex >= 0) {
			if (seq->pups < 0) seq->pups = 0;
			if (seq->pups > 7) seq->pups = 7;
			frameData->mark_modified(patternIndex);
		}
		ImGui::SameLine(); ImGui::TextDisabled("(?)");
		if (ImGui::IsItemHovered())
			Tooltip("PUPS: palette file used while this pattern is drawn.\n"
			"0 = <cg>.pal, n = <cg>_pn.pal (n = 1..7), same palette number.\n"
			"(CharaPalette_LoadPalAndPupsVariants / Han6Draw_DrawLayer)");
		ImGui::SameLine();
		std::string code = seq->codeName;
		ImGui::SetNextItemWidth(spacing * 3);
		if (ImGui::InputText("Code name (PTCN)", &code) && frameData && patternIndex >= 0) {
			seq->codeName = code;
			frameData->mark_modified(patternIndex);
		}
	}
}