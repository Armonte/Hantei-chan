#include <imgui.h>
#include <imgui_stdlib.h>
#include "framedata.h"

inline void PatternDisplay(Sequence *seq)
{
	constexpr float spacing = 80;
	float width = 115;
	float height = ImGui::GetTextLineHeightWithSpacing()*5.5;

	ImGui::BeginChild("child", ImVec2(width, height), true);
	ImGui::Text("Hitboxes:"); ImGui::SameLine(spacing);
	ImGui::Text("%llu", seq->hitboxes.size());
	
	ImGui::Text("nAT:"); ImGui::SameLine(spacing);
	ImGui::Text("%llu", seq->AT.size());

	ImGui::Text("nAS:"); ImGui::SameLine(spacing);
	ImGui::Text("%llu", seq->AS.size());

	ImGui::Text("nEF:"); ImGui::SameLine(spacing);
	ImGui::Text("%llu", seq->EF.size());

	ImGui::Text("nIF:"); ImGui::SameLine(spacing);
	ImGui::Text("%llu", seq->IF.size());
	ImGui::EndChild();

	ImGui::SameLine();
	ImGui::BeginChild("child2", ImVec2(ImGui::GetWindowWidth()-width-60,height), true, ImGuiWindowFlags_HorizontalScrollbar);
	ImGui::SetNextItemWidth(140);
	ImGui::InputText("Codename", &seq->codeName);
	
	ImGui::SetNextItemWidth(40.f);
	ImGui::InputInt("PSTS", &seq->psts, 0, 0);

	ImGui::SetNextItemWidth(40.f);
	ImGui::InputInt("Level", &seq->level, 0, 0);

	ImGui::SetNextItemWidth(80.f);
	ImGui::InputInt("Flag (PFLG)", &seq->flag, 0, 0);

	ImGui::EndChild();
}