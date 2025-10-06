#ifndef FRAME_DISP_COMMON_H_GUARD
#define FRAME_DISP_COMMON_H_GUARD

#include <imgui.h>
#include "../imgui_utils.h"
#include "../framedata.h"
#include "../framedata_labels.h"
#include <functional>

namespace im = ImGui;

// ============================================================================
// Common Helper Functions for Frame Display UI
// ============================================================================

// Helper function for combo with manual entry support
static inline bool ShowComboWithManual(const char* label, int* value, const char* const* items, int itemCount, float comboWidth, float defaultWidth = 75.f) {
	bool changed = false;
	if(comboWidth < 0) comboWidth = defaultWidth*2;
	// Find if current value is in the list
	int selectedIndex = -1;
	int parsedValue;
	for(int i = 0; i < itemCount; i++) {
		// Parse "N: Description" format
		if(sscanf(items[i], "%d:", &parsedValue) == 1) {
			if(parsedValue == *value) {
				selectedIndex = i;
				break;
			}
		}
	}

	// Determine preview text
	const char* preview;
	char customBuffer[64];
	if(selectedIndex >= 0) {
		preview = items[selectedIndex];
	} else {
		snprintf(customBuffer, sizeof(customBuffer), "Custom: %d", *value);
		preview = customBuffer;
	}

	im::SetNextItemWidth(comboWidth);
	if(im::BeginCombo(label, preview)) {
		// Show all predefined items
		for(int i = 0; i < itemCount; i++) {
			bool selected = (i == selectedIndex);
			if(im::Selectable(items[i], selected)) {
				// Parse and set value
				if(sscanf(items[i], "%d:", &parsedValue) == 1) {
					*value = parsedValue;
					changed = true;
				}
			}
			if(selected)
				im::SetItemDefaultFocus();
		}

		// Add manual entry option
		im::Separator();
		im::SetNextItemWidth(defaultWidth);
		if(im::InputInt("Custom value", value, 0, 0)) {
			changed = true;
		}

		im::EndCombo();
	}
	return changed;

	// Show visual indicator for custom values
	if(selectedIndex < 0) {
		im::SameLine();
		im::TextColored(ImVec4(1.0f, 0.7f, 0.0f, 1.0f), "⚠");
		if(im::IsItemHovered()) {
			Tooltip("Using undocumented value");
		}
	}
}

// Placeholder for hit vector table display (currently unused)
inline void HitVectorDisplay()
{
	if (im::BeginTable("HitVectors", 17,
		ImGuiTableFlags_RowBg |
		ImGuiTableFlags_SizingFixedFit))
	{
		im::TableSetupColumn("Description");
		im::TableSetupColumn("VecCnt");
		im::TableSetupColumn("UkemiTime");
		im::TableSetupColumn("Prio");
		im::TableSetupColumn("PrioAni");
		im::TableSetupColumn("KoCheck");
		im::TableSetupColumn("VecNum");
		im::TableSetupColumn("HitAni");
		im::TableSetupColumn("GuardAni");
		im::TableSetupColumn("Time");
		im::TableSetupColumn("VecTime");
		im::TableSetupColumn("flag");
		im::TableSetupColumn("VecNum");
		im::TableSetupColumn("HitAni");
		im::TableSetupColumn("GuardAni");
		im::TableSetupColumn("Time");
		im::TableSetupColumn("VecTime");
		im::TableHeadersRow();
		im::EndTable();
	}
}

#endif /* FRAME_DISP_COMMON_H_GUARD */

