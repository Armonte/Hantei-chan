#ifndef EFFECT_FORM_H_GUARD
#define EFFECT_FORM_H_GUARD
// Label-first rows for the effect (EF) smart UIs (docs/ui/LAYOUT_AUDIT_2026-10-16.md item 9, round 3).
//
// Same look as ui::BeginLabelForm / FormRow (label column, then the field filling the pane) but without a table, so
// the ~200 effect fields can sit between tree nodes, bit fields and notes without opening / closing a form around
// each of them. One label column width per window: the widest effect label drawn in the previous frame, capped at
// 42% of the pane (a longer, e.g. Japanese, label wraps and shows its full text in a tooltip). Every row keeps a small
// gutter on the right for the "(?)" marker the effect UIs put after a field (`im::SameLine(); im::TextDisabled("(?)")`).
//   if(im::InputInt(EfRow(LBL("Damage")), &p[0], 0, 0)) markModified();
#include <imgui.h>
#include <cfloat>
#include <cstring>
#include <cctype>
#include "../../ui/ui_style.h"
#include "../../i18n.h"

namespace ef_form {

inline float HelpGutterW() { return ImGui::CalcTextSize("(?)").x + ImGui::GetStyle().ItemSpacing.x; }

// Label column width for this window (measured last frame), and the per-frame measurement of this frame's labels.
inline float LabelW(float measure)
{
	ImGuiStorage *st = ImGui::GetStateStorage();
	const ImGuiID kW = 0x6566574Cu, kAcc = 0x65664163u, kFrame = 0x65664672u; // fixed keys in the window storage
	const int frame = ImGui::GetFrameCount();
	if (st->GetInt(kFrame, -1) != frame) {
		const float acc = st->GetFloat(kAcc, 0.f);
		if (acc > 0.f) st->SetFloat(kW, acc);
		st->SetFloat(kAcc, 0.f);
		st->SetInt(kFrame, frame);
	}
	st->SetFloat(kAcc, std::max(st->GetFloat(kAcc, 0.f), measure));
	const float w = st->GetFloat(kW, ui::Em(8.f));
	const float cap = std::max(ui::Em(5.f), ImGui::GetContentRegionAvail().x * 0.42f);
	return std::min(w, cap);
}

}  // namespace ef_form

// Draws `label` (an LBL()/TXT() string; the part from "##" on is hidden) in the label column and returns the hidden id
// for the field, whose width is set to fill the row up to the "(?)" gutter.
inline const char *EfRow(const char *label)
{
	const char *end = std::strstr(label, "##");
	const float textW = ImGui::CalcTextSize(label, end).x;
	const float lw = ef_form::LabelW(textW + ImGui::GetStyle().ItemSpacing.x);
	const float x0 = ImGui::GetCursorPosX();
	const float textMax = lw - ImGui::GetStyle().ItemSpacing.x;
	ImGui::AlignTextToFramePadding();
	ImGui::PushTextWrapPos(x0 + textMax);
	ImGui::TextUnformatted(label, end);
	ImGui::PopTextWrapPos();
	if (textW > textMax && ImGui::IsItemHovered())
		ImGui::SetTooltip("%.*s", end ? int(end - label) : int(std::strlen(label)), label);
	ImGui::SameLine(x0 + lw);
	ImGui::SetNextItemWidth(-ef_form::HelpGutterW());
	return ui::HiddenId(label);
}

// A dimmed note under the previous row, starting in the value column (game label titles and similar hints).
inline void EfNote(const char *text)
{
	const float lw = ef_form::LabelW(0.f);
	ImGui::SetCursorPosX(ImGui::GetCursorPosX() + lw);
	ImGui::PushTextWrapPos(0.f);
	ImGui::TextDisabled("%s", text);
	ImGui::PopTextWrapPos();
}

// Sub-header between the groups of one effect. Takes the existing "--- Name ---" strings (already translated) and
// drops the dashes.
inline void EfSection(const char *title)
{
	while (*title == '-' || *title == ' ') ++title;
	size_t n = std::strlen(title);
	while (n > 0 && (title[n - 1] == '-' || title[n - 1] == ' ')) --n;
	char buf[192];
	std::snprintf(buf, sizeof buf, "%.*s", int(n), title);
	ui::FormSection(buf);
}

// ShowComboWithManual (frame_disp_common.h) as a label-first row: same "N: name" list, translated items and the
// "Custom value" entry for undocumented numbers.
static inline bool EfComboManual(const char *label, int *value, const char *const *items, int itemCount, float = 0.f, float defaultWidth = 75.f)
{
	bool changed = false;
	int selectedIndex = -1, parsedValue;
	for (int i = 0; i < itemCount; i++)
		if (sscanf(items[i], "%d:", &parsedValue) == 1 && parsedValue == *value) { selectedIndex = i; break; }
	char customBuffer[64];
	const char *preview;
	if (selectedIndex >= 0) preview = TXT(items[selectedIndex]);
	else { snprintf(customBuffer, sizeof(customBuffer), TXT("Custom: %d"), *value); preview = customBuffer; }

	if (ImGui::BeginCombo(EfRow(LBL(label)), preview)) {
		for (int i = 0; i < itemCount; i++) {
			const bool selected = (i == selectedIndex);
			if (ImGui::Selectable(TXT(items[i]), selected) && sscanf(items[i], "%d:", &parsedValue) == 1) {
				*value = parsedValue;
				changed = true;
			}
			if (selected) ImGui::SetItemDefaultFocus();
		}
		ImGui::Separator();
		ImGui::SetNextItemWidth(defaultWidth);
		if (ImGui::InputInt(LBL("Custom value"), value, 0, 0)) changed = true;
		ImGui::EndCombo();
	}
	return changed;
}

#endif /* EFFECT_FORM_H_GUARD */
