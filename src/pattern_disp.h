#include <imgui.h>
#include <imgui_stdlib.h>
#include "framedata.h"
#include "imgui_utils.h"

inline void PatternDisplay(Sequence *seq)
{
	constexpr float spacing = 80.0f;
	//ImGui::InputText("Code name", &seq->codeName);
	
	ImGui::SetNextItemWidth(spacing);
	ImGui::InputInt("PSTS", &seq->psts, 0, 0);
	ImGui::SameLine(); ImGui::TextDisabled("(?)");
	if (ImGui::IsItemHovered())
		Tooltip("Determines shield properties\n"
			"0: Must be shielded according to stance\n"
			"1: Must be shielded according to level and stance (Level 0 can be shielded both ways)\n"
			"5: Must be shielded according to stance. Do not trigger H-Moon Shield Counter\n"
			"Projectiles can be shieded either way regardless");

	ImGui::SetNextItemWidth(spacing);
	ImGui::InputInt("Level", &seq->level, 0, 0);
	ImGui::SameLine(); ImGui::TextDisabled("(?)");
	if (ImGui::IsItemHovered())
		Tooltip("Determines cancel and shield properties\n"
		"F Moon: Attacks can be canceled into higher level attacks only (0 can cancel into 0)\n"
		"C Moon: Rebeat penalty applied for canceling into lower level attacks\n"
		"Level 0 can be shielded either way if PSTS = 1\n"
		"Level 0 can be self canceled");

	ImGui::SetNextItemWidth(spacing);
	ImGui::InputInt("Flag", &seq->flag, 0, 0);
}