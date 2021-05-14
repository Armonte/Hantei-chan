#include <imgui.h>
#include <imgui_stdlib.h>
#include "framedata.h"

inline void PatternDisplay(Sequence *seq)
{
	constexpr float spacing = 80;
	ImGui::InputText("Code name", &seq->codeName);
	
	ImGui::SetNextItemWidth(40.f);
	ImGui::InputInt("PSTS", &seq->psts, 0, 0);

	ImGui::SetNextItemWidth(40.f);
	ImGui::InputInt("Level", &seq->level, 0, 0);

	ImGui::SetNextItemWidth(80.f);
	ImGui::InputInt("PFLG", &seq->flag, 0, 0);
}