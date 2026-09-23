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

// Small arrow button that opens a filterable list of the character's patterns
// (issue #57). `extra` adds special values shown above the list (e.g. IF 18's
// 256). Returns true when *value was changed.
struct PatternPickerExtra { int value; const char* label; };
static inline bool PatternPickerButton(const char* id, int* value, FrameData* frameData,
                                       const PatternPickerExtra* extra = nullptr, int extraCount = 0)
{
	if(!frameData) return false;
	bool changed = false;
	im::PushID(id);
	im::SameLine(0, 2.f);
	if(im::ArrowButton("##pick", ImGuiDir_Down)) im::OpenPopup("##patternPicker");
	if(im::IsItemHovered()) im::SetTooltip("Pick a pattern");
	if(im::BeginPopup("##patternPicker")) {
		static char filter[64] = "";
		if(im::IsWindowAppearing()) { filter[0] = 0; im::SetKeyboardFocusHere(); }
		im::SetNextItemWidth(300.f);
		im::InputTextWithHint("##filter", "Filter by number or name", filter, sizeof(filter));
		auto matches = [](const std::string& text, const char* f) {
			if(!f[0]) return true;
			std::string a = text, b = f;
			for(auto& c : a) c = (char)tolower((unsigned char)c);
			for(auto& c : b) c = (char)tolower((unsigned char)c);
			return a.find(b) != std::string::npos;
		};
		if(im::BeginChild("##list", ImVec2(420.f, 320.f), ImGuiChildFlags_None)) {
			for(int i = 0; i < extraCount; ++i) {
				char buf[128];
				snprintf(buf, sizeof(buf), "%d: %s", extra[i].value, extra[i].label);
				if(!matches(buf, filter)) continue;
				if(im::Selectable(buf, *value == extra[i].value)) {
					*value = extra[i].value; changed = true; im::CloseCurrentPopup();
				}
			}
			const int count = frameData->get_sequence_count();
			for(int n = 0; n < count; ++n) {
				const std::string name = frameData->GetDecoratedName(n);
				if(!matches(name, filter)) continue;
				const bool selected = *value == n;
				if(im::Selectable(name.c_str(), selected)) {
					*value = n; changed = true; im::CloseCurrentPopup();
				}
				if(selected && im::IsWindowAppearing()) im::SetScrollHereY();
			}
		}
		im::EndChild();
		im::EndPopup();
	}
	im::PopID();
	return changed;
}

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

