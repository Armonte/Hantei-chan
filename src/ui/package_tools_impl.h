#ifndef UI_PACKAGE_TOOLS_IMPL_H_GUARD
#define UI_PACKAGE_TOOLS_IMPL_H_GUARD

// ============================================================================
// MBAACC package tools UI (docs/HANTEI_WAVE2.md §6)
// ============================================================================
// Included from main_frame.cpp. The work is done by mbaacc_package.* (also
// available as the mbaaccpackage CLI); this file is the File-menu entry points
// and the report / confirmation windows. Every action works on the active
// character's .txt descriptor and never writes without a review step.

#include "../mbaacc_package.h"

namespace pkgui {
struct State {
	bool showReport = false;
	std::string title;
	std::string report;        // UTF-8
	std::string descriptor;    // UTF-8
	bool offerRepair = false;
	bool offerExtract = false;
	// consolidation review
	bool showConsolidate = false;
	char mainName[260] = {};
	bool mergeNotes = true;
	bool acknowledged = false;
};
State g;

std::string DescriptorOf(CharacterInstance* c)
{
	return c ? AnsiToUtf8(c->getTxtPath()) : std::string();
}
} // namespace pkgui

void MainFrame::DrawPackageToolsMenuItems()
{
	CharacterInstance* c = getActiveCharacter();
	const std::string descriptor = pkgui::DescriptorOf(c);
	const bool hasDescriptor = !descriptor.empty();
	if (!ImGui::BeginMenu("MBAACC package")) return;
	if (ImGui::MenuItem("Validate current character package...", nullptr, false, hasDescriptor)) {
		const auto r = mbpackage::ValidateCharacterPackage(descriptor);
		pkgui::g = pkgui::State{};
		pkgui::g.showReport = true;
		pkgui::g.title = "Package validation";
		pkgui::g.report = r.report;
		pkgui::g.descriptor = descriptor;
		pkgui::g.offerRepair = r.newlineRepairAvailable;
		for (const auto& row : r.rows) pkgui::g.offerExtract |= row.status == mbpackage::RowStatus::packed;
	}
	if (ImGui::MenuItem("Consolidate layered HA6...", nullptr, false, hasDescriptor)) {
		pkgui::g = pkgui::State{};
		pkgui::g.showConsolidate = true;
		pkgui::g.descriptor = descriptor;
		const size_t slash = descriptor.find_last_of("\\/");
		std::string stem = descriptor.substr(slash == std::string::npos ? 0 : slash + 1);
		stem = stem.substr(0, stem.find_last_of('.'));
		snprintf(pkgui::g.mainName, sizeof(pkgui::g.mainName), "%s.HA6", stem.c_str());
	}
	if (!hasDescriptor && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		ImGui::SetTooltip("Needs a character opened from its .txt descriptor.");
	ImGui::TextDisabled("Also: mbaaccpackage.exe (validate, repair-crlf,\nconsolidate, extract, pack-list, pack-extract)");
	ImGui::EndMenu();
}

void MainFrame::DrawPackageToolWindows()
{
	auto& g = pkgui::g;
	const ImVec2 mainPos = ImGui::GetMainViewport()->Pos;
	if (g.showReport) {
		ImGui::SetNextWindowPos(ImVec2(mainPos.x + 120, mainPos.y + 90), ImGuiCond_Appearing);
		ImGui::SetNextWindowSize(ImVec2(720, 420), ImGuiCond_Appearing);
		if (ImGui::Begin((g.title + "###pkg_report").c_str(), &g.showReport, ImGuiWindowFlags_NoDocking)) {
			if (g.offerRepair && ImGui::Button("Repair line endings (backup + CRLF)")) {
				const auto r = mbpackage::RepairDescriptorNewlines(g.descriptor);
				g.report = r.message;
				g.title = "Line-ending repair";
				g.offerRepair = false;
			}
			if (g.offerExtract) {
				if (g.offerRepair) ImGui::SameLine();
				if (ImGui::Button("Extract packed files beside the descriptor")) {
					const auto r = mbpackage::ExtractPackedRows(g.descriptor);
					g.report = r.message;
					g.title = "Extract from game archives";
					g.offerExtract = false;
				}
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("Writes rows found only in the game's .p archives as loose files.\n"
						"Existing files are never overwritten. PC/Steam cipher is detected per archive.");
			}
			ImGui::BeginChild("##report", ImVec2(0, 0), ImGuiChildFlags_Borders);
			ImGui::TextUnformatted(g.report.c_str());
			ImGui::EndChild();
		}
		ImGui::End();
	}
	if (g.showConsolidate) {
		ImGui::SetNextWindowPos(ImVec2(mainPos.x + 140, mainPos.y + 110), ImGuiCond_Appearing);
		ImGui::SetNextWindowSize(ImVec2(560, 0), ImGuiCond_Appearing);
		if (ImGui::Begin("Consolidate layered HA6###pkg_consolidate", &g.showConsolidate, ImGuiWindowFlags_NoDocking)) {
			ImGui::TextWrapped("%s", g.descriptor.c_str());
			ImGui::Separator();
			ImGui::TextWrapped("Keeps the highest [DataFile] layer byte-for-byte as the only HA6, if it already "
				"contains every pattern of the lower layers (refused otherwise). The descriptor, every layer "
				"and their notes are copied to a timestamped backup folder first; superseded layers are then "
				"removed from the character folder. Cancel to keep layered authoring.");
			ImGui::InputText("Canonical HA6 name", g.mainName, sizeof(g.mainName));
			ImGui::Checkbox("Merge .notes sidecars (higher layer wins)", &g.mergeNotes);
			ImGui::Checkbox("I understand the old layers leave the folder (backup kept)", &g.acknowledged);
			ImGui::BeginDisabled(!g.acknowledged);
			if (ImGui::Button("Back up and consolidate")) {
				const auto r = mbpackage::ConsolidateHa6Layers(g.descriptor, g.mainName, g.mergeNotes);
				g.showConsolidate = false;
				g.showReport = true;
				g.title = r.success ? "Consolidation complete" : "Consolidation refused";
				g.report = r.message;
			}
			ImGui::EndDisabled();
			ImGui::SameLine();
			if (ImGui::Button("Cancel")) g.showConsolidate = false;
		}
		ImGui::End();
	}
}

#endif /* UI_PACKAGE_TOOLS_IMPL_H_GUARD */
