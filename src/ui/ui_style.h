#pragma once
// Shared layout tokens and helpers for the editor's panes (docs/ui/LAYOUT_AUDIT_2026-10-16.md).
// Every size here follows the font size (Preferences > font size, DPI), so a pane laid out with them keeps its proportions
// at 14 px and at 28 px, and Japanese labels (wider) get the room they need instead of a fixed pixel budget.
#include <imgui.h>
#include <algorithm>

namespace ui {

// The font size the hand-tuned pixel widths in the code were picked at (ini.h gSettings.fontSize default).
constexpr float kDesignFontPx = 18.f;

// `px` design pixels at the current font size.
inline float Px(float px) { return px * ImGui::GetFontSize() / kDesignFontPx; }
// `n` font heights ("em").
inline float Em(float n) { return n * ImGui::GetFontSize(); }

// Spacing / size tokens.
inline float FieldW() { return Em(6.f); }         // a number field (drag / input int)
inline float WideFieldW() { return Em(12.f); }    // a combo / short text
inline float PathFieldW() { return Em(22.f); }    // a path text box
inline float FormCellMinW() { return Em(13.f); }  // one label + field cell of a FormGrid
inline float GroupGap() { return ImGui::GetStyle().ItemSpacing.y * 2.f; }

// The first-use size of a floating tool window, clamped to the main viewport (a 1200x820 window used to run off a
// 1280x720 screen) and centred on it under the same condition, so the whole window, title bar included, is reachable.
inline void FitNextWindow(float w, float h, ImGuiCond cond = ImGuiCond_FirstUseEver)
{
	const ImGuiViewport *vp = ImGui::GetMainViewport();
	const float maxW = vp->WorkSize.x * 0.92f, maxH = vp->WorkSize.y * 0.90f;
	const ImVec2 size(w > 0.f ? std::min(Px(w), maxW) : 0.f, h > 0.f ? std::min(Px(h), maxH) : 0.f);
	ImGui::SetNextWindowSize(size, cond);
	ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + vp->WorkSize.x * 0.5f, vp->WorkPos.y + vp->WorkSize.y * 0.5f), cond, ImVec2(0.5f, 0.5f));
}

// A "(?)" marker after the previous item: the long help text lives in its tooltip instead of eating rows.
inline void HelpMarker(const char *text)
{
	ImGui::SameLine();
	ImGui::TextDisabled("(?)");
	if (ImGui::BeginItemTooltip()) {
		ImGui::PushTextWrapPos(Em(28.f));
		ImGui::TextUnformatted(text);
		ImGui::PopTextWrapPos();
		ImGui::EndTooltip();
	}
}

// FormGrid: label-first cells ("label [field]") flowing into as many columns as the pane is wide.
//   if (ui::BeginFormGrid("cam")) { ui::FormCell("cam x"); ImGui::DragFloat("##camx", ...); ... ui::EndFormGrid(); }
// Each cell is one table column; the label sits on the frame baseline and the field takes the rest of the cell.
inline bool BeginFormGrid(const char *id, int cells, float minCellW = 0.f)
{
	if (minCellW <= 0.f) minCellW = FormCellMinW();
	const int fit = std::max(1, int(ImGui::GetContentRegionAvail().x / minCellW));
	const int cols = std::max(1, std::min(cells, fit));
	return ImGui::BeginTable(id, cols, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_PadOuterX);
}
// Starts the next cell with `label`; the next widget fills the rest of it (give it a "##id" label).
inline void FormCell(const char *label)
{
	ImGui::TableNextColumn();
	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted(label);
	ImGui::SameLine();
	ImGui::SetNextItemWidth(std::max(ImGui::GetContentRegionAvail().x, FieldW() * 0.6f));
}
inline void EndFormGrid() { ImGui::EndTable(); }

}  // namespace ui
