#pragma once
// Row-wrapping helpers for button / checkbox rows (docs/ui/LAYOUT_AUDIT_2026-10-16.md, round 2).
// They build on i18n::SameLineFit, so a row continues on the same line only while the next item still fits the pane;
// at 1280 or in Japanese the row breaks before an item instead of clipping it ("Cle...", "Update fr...").
#include <imgui.h>
#include <algorithm>
#include <initializer_list>
#include "../i18n.h"

namespace ui {

// Call before a Button(label) that should follow the previous item on the same line when it fits.
inline void SameLineButton(const char *label, float spacing = -1.f) { i18n::SameLineFit(i18n::ButtonWidth(label), spacing); }
// Same for a Checkbox(label).
inline void SameLineCheckbox(const char *label, float spacing = -1.f) { i18n::SameLineFit(i18n::FieldWidth(ImGui::GetFrameHeight(), label), spacing); }
// Same for an item of `itemWidth` followed by its label (InputInt etc.).
inline void SameLineField(float itemWidth, const char *label, float spacing = -1.f) { i18n::SameLineFit(i18n::FieldWidth(itemWidth, label), spacing); }

// Equal-width button grid: every cell is as wide as the widest label, as many columns as the pane holds, so a row of
// buttons wraps into even rows (2+2, 3+1 ...) instead of a ragged line.
//   ui::ButtonGrid g({LBL("Copy all"), LBL("Paste all")}); if (g.Button(LBL("Copy all"))) ...; g.Button(...)
struct ButtonGrid {
	float cellW = 0.f, x0 = 0.f, spacing = 0.f;
	int cols = 1, i = 0;
	explicit ButtonGrid(std::initializer_list<const char *> labels, float extraW = 0.f)
	{
		for (const char *l : labels) cellW = std::max(cellW, i18n::ButtonWidth(l));
		cellW += extraW;
		spacing = ImGui::GetStyle().ItemSpacing.x;
		x0 = ImGui::GetCursorPosX();
		const float avail = ImGui::GetContentRegionAvail().x;
		cols = std::max(1, int((avail + spacing) / (cellW + spacing)));
		const int n = std::max(1, (int)labels.size());
		cols = std::min(cols, n);
		const int rows = (n + cols - 1) / cols;
		cols = (n + rows - 1) / rows;                         // even rows: 4 items in 3 columns become 2 + 2
		cellW = std::max(cellW, (avail - spacing * (cols - 1)) / cols); // fill the row
	}
	void Next()
	{
		if (i % cols) ImGui::SameLine(x0 + (i % cols) * (cellW + spacing));
		else if (i > 0) ImGui::SetCursorPosX(x0);             // later rows start under the first cell
		++i;
	}
	bool Button(const char *label) { Next(); return ImGui::Button(label, ImVec2(cellW, 0.f)); }
};

} // namespace ui
