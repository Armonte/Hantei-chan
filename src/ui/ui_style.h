#pragma once
// Shared layout tokens and helpers for the editor's panes (docs/ui/LAYOUT_AUDIT_2026-10-16.md).
// Every size here follows the font size (Preferences > font size, DPI), so a pane laid out with them keeps its proportions
// at 14 px and at 28 px, and Japanese labels (wider) get the room they need instead of a fixed pixel budget.
#include <imgui.h>
#include <imgui_internal.h>   // FindWindowByID / FindWindowSettingsByID (FitNextWindowToContent)
#include <algorithm>
#include <cfloat>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <vector>

namespace ui {

inline constexpr ImU32 EffectsColor = IM_COL32(80, 180, 255, 255);
inline constexpr ImU32 ConditionsColor = IM_COL32(255, 150, 60, 255);
inline ImVec4 AxisColor(int axis) {
	return axis == 0 ? ImVec4(.95f,.30f,.30f,1) : axis == 1 ? ImVec4(.32f,.83f,.32f,1) : ImVec4(.25f,.70f,1,1);
}
// Stack follows successful table begins, not the active host/tab. Nested forms
// and simultaneous detached panes must never borrow another form's layout mode.
inline std::vector<int> propertyFormStack; // 0 label form, 1 inline grid, 2 label-above grid

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

// First-use size that fits the content, for a tool window whose fixed first-use size was much bigger than what it
// shows. On the window's first appearance (nothing for it in imgui.ini and not created yet this session) it auto-resizes
// for a few frames between `minW` and `maxW` (design px, ui::Px; height up to 90% of the work area) and stays centred like
// FitNextWindow; after that it is an ordinary resizable window, and a size the user set (saved in imgui.ini) is never
// touched. `name` is the exact title passed to Begin. Returns the flags to OR into Begin's flags.
inline ImGuiWindowFlags FitNextWindowToContent(const char *name, float minW, float maxW)
{
	static ImGuiStorage state;   // per window id: 0 undecided, >0 auto-resize frames left, -1 done
	const ImGuiID id = ImHashStr(name);
	int left = state.GetInt(id, 0);
	if (left == 0) left = (ImGui::FindWindowByID(id) || ImGui::FindWindowSettingsByID(id)) ? -1 : 3;
	if (left < 0) { state.SetInt(id, -1); return 0; }
	state.SetInt(id, left == 1 ? -1 : left - 1);
	const ImGuiViewport *vp = ImGui::GetMainViewport();
	const float maxPxW = std::min(Px(maxW), vp->WorkSize.x * 0.92f);
	ImGui::SetNextWindowSizeConstraints(ImVec2(std::min(Px(minW), maxPxW), 0.f), ImVec2(maxPxW, vp->WorkSize.y * 0.90f));
	ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + vp->WorkSize.x * 0.5f, vp->WorkPos.y + vp->WorkSize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	return ImGuiWindowFlags_AlwaysAutoResize;
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

// LabelForm: label-first rows ("label | field") on a two-column table. One label column width per display (LabelColW over
// every label it shows) keeps the values of all its sections on one line, and the field fills the rest of the pane.
// Labels are LBL()/TXT() strings; the part from "##" on is not drawn. A label wider than the column wraps, and its full
// text is in a tooltip.
//   const float lw = ui::LabelColW({LBL("Damage"), LBL("Meter gain")});
//   if (ui::BeginLabelForm("dmg", lw)) { if (ImGui::InputInt(ui::FormRow(LBL("Damage")), &v, 0, 0)) ...; ui::EndLabelForm(); }
inline float LabelTextW(const char *label) { return ImGui::CalcTextSize(label, nullptr, true).x; }
inline float LabelColW(std::initializer_list<const char *> labels, float capFrac = 0.42f)
{
	float w = 0.f;
	for (const char *l : labels) w = std::max(w, LabelTextW(l));
	w += ImGui::GetStyle().ItemSpacing.x;
	const float cap = std::max(Em(5.f), ImGui::GetContentRegionAvail().x * capFrac);
	return std::min(w, cap);
}
inline bool BeginLabelForm(const char *id, float labelW)
{
	if (!ImGui::BeginTable(id, 2, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoSavedSettings)) return false;
	ImGui::TableSetupColumn("##label", ImGuiTableColumnFlags_WidthFixed, labelW);
	ImGui::TableSetupColumn("##value", ImGuiTableColumnFlags_WidthStretch);
	propertyFormStack.push_back(false);
	return true;
}
inline void EndLabelForm() { ImGui::EndTable(); propertyFormStack.pop_back(); }
inline int PropertyColumns(float available, float minimum, int maximum)
{
	return std::max(1, std::min(maximum, int(available / std::max(1.f, minimum))));
}
// Opt-in only: compound rows (pickers, toolbars, multiline editors) keep their
// full width unless the caller explicitly groups them. Existing widgets and
// modification callbacks stay in one path in both layouts.
inline bool BeginPropertyForm(const char* id, float labelW, bool compact, int maximum, float minimum, bool stacked = false)
{
	const int columns = PropertyColumns(ImGui::GetContentRegionAvail().x, minimum, maximum);
	const bool grid = compact && columns > 1 && (!stacked || columns >= 3);
	// Bounded real-pane trace, kept through manual acceptance of the layout.
	static unsigned serial = 0;
	const ImGuiID key = ImGui::GetID(id);
	auto* storage = ImGui::GetStateStorage();
	const int mode = (compact ? 100 : 0) + (grid ? columns : 1);
	if (storage->GetInt(key, -1) != mode) {
		storage->SetInt(key, mode);
		if (auto* f = std::fopen("compact_ui_trace.log", serial++ % 256 == 0 ? "w" : "a")) {
			std::fprintf(f, "frame=%d window=%s viewport=%u form=%s compact=%d columns=%d width=%.1f font=%.1f\n",
				ImGui::GetFrameCount(), ImGui::GetCurrentWindow()->Name, ImGui::GetWindowViewport()->ID,
				id, compact, grid ? columns : 1, ImGui::GetContentRegionAvail().x, ImGui::GetFontSize());
			std::fclose(f);
		}
	}
	if (!grid) return BeginLabelForm(id, labelW);
	if (!ImGui::BeginTable(id, columns, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings)) return false;
	propertyFormStack.push_back(stacked ? 2 : 1);
	return true;
}
// "##<label>": the hidden-label id of a field whose label is drawn in the label column ("##日本語###Damage" keeps the
// English "###Damage" id, so switching language keeps widget state).
inline const char *HiddenId(const char *label)
{
	static char buf[16][400];   // room for the long UNI2 / MBTL parameter labels (game_label_ui.h)
	static int slot = 0;
	slot = (slot + 1) & 15;
	std::snprintf(buf[slot], sizeof buf[slot], "##%s", label);
	return buf[slot];
}
// Starts a row: the label in the label column, then moves to the value column with the next item filling it.
// Returns the hidden id to give the field.
inline const char *FormRow(const char *label)
{
	if (!propertyFormStack.empty() && propertyFormStack.back()) {
		ImGui::TableNextColumn();
		const char* end = std::strstr(label, "##");
		if (propertyFormStack.back() == 2) {
			ImGui::TextUnformatted(label, end);
			ImGui::SetNextItemWidth(-FLT_MIN);
			return HiddenId(label);
		}
		const float labelW = ImGui::GetContentRegionAvail().x * .46f;
		const auto p = ImGui::GetCursorScreenPos();
		const ImVec2 textMin(p.x, p.y + ImGui::GetStyle().FramePadding.y);
		const ImVec2 textMax(p.x + labelW, textMin.y + ImGui::GetTextLineHeight());
		ImGui::RenderTextEllipsis(ImGui::GetWindowDrawList(), textMin, textMax, textMax.x, label, end, nullptr);
		ImGui::Dummy(ImVec2(labelW, ImGui::GetFrameHeight()));
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("%.*s", end ? int(end-label) : int(std::strlen(label)), label);
		ImGui::SameLine();
		ImGui::SetNextItemWidth(-FLT_MIN);
		return HiddenId(label);
	}
	ImGui::TableNextRow();
	ImGui::TableSetColumnIndex(0);
	ImGui::AlignTextToFramePadding();
	const char *end = std::strstr(label, "##");
	const float colW = ImGui::GetContentRegionAvail().x;
	const bool overflow = ImGui::CalcTextSize(label, end).x > colW;
	ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + colW);
	const bool axis = label[0] && !label[1] && (label[0]=='X'||label[0]=='Y'||label[0]=='Z');
	if(axis) ImGui::PushStyleColor(ImGuiCol_Text,AxisColor(label[0]=='X'?0:label[0]=='Y'?1:2));
	ImGui::TextUnformatted(label, end);
	if(axis) ImGui::PopStyleColor();
	ImGui::PopTextWrapPos();
	if (overflow && ImGui::IsItemHovered()) ImGui::SetTooltip("%.*s", end ? int(end - label) : int(std::strlen(label)), label);
	ImGui::TableSetColumnIndex(1);
	ImGui::SetNextItemWidth(-FLT_MIN);
	return HiddenId(label);
}
// A sub-header between the forms of one display (damage, hitstop, vectors ...).
inline void FormSection(const char *title)
{
	ImGui::Dummy(ImVec2(0.f, ImGui::GetStyle().ItemSpacing.y * 0.5f));
	ImGui::SeparatorText(title);
}

}  // namespace ui
