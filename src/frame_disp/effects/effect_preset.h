#ifndef EFFECT_PRESET_H_GUARD
#include "../../i18n.h"
#include "effect_form.h"
#include "../../ui/ui_style.h"
#define EFFECT_PRESET_H_GUARD

// ============================================================================
// Effect Type 3: Preset Effects
// ============================================================================
// Spawn preset visual effects from a predefined library
// (dust clouds, sparks, force fields, screen overlays, etc.)
// ============================================================================

static inline void DrawEffectPreset_Type3(Frame_EF& effect, FrameData* frameData, int patternIndex, std::function<void()> markModified)
{
	int* p = effect.parameters;
	int& no = effect.number;
	const float width = ui::Px(75.f);   // follows the font size (ui_style.h)

	// Preset list and params: gonp's HA6 doc, 2026-11.
	const char* const presetEffects[] = {
				"0: Expanding circle",
				"1: Jump looking effect",
				"2: David star (lags, unused)",
				"3: Red hitspark",
				"4: Force field (unused)",
				"5: Bouncing white traces",
				"6: Fire (unused)",
				"7: Snow (unused)",
				"8: Blue flash (unused)",
				"9: Blue hitspark",
				"10: Superflash background",
				"14: Expanding dust (cropped)",
				"15: Horizontal wave (unused)",
				"16: Vertical wave (unused)",
				"19: Foggy rays (unused)",
				"20: 3D rotating waves (unused)",
				"21: Expanding jagged blue circle",
				"22: Unknown",
				"23: Blinding effect",
				"24: Blinding effect 2",
				"27: Dust cloud",
				"28: Dust cloud (large)",
				"29: Dust cloud (rotating)",
				"30: Massive dust cloud (unused)",
				"100: Big wallbounce distortion",
				"101: Small wallbounce distortion",
				"256: Unknown",
				"265: White glow impact",
				"300: Round win icon flare (crashes)",
				"301: Expanding yellow floor",
				"302: Angled red impact",
			};

			if(EfComboManual("Effect Number", &no, presetEffects, IM_ARRAYSIZE(presetEffects), width*2, width)) {
				markModified();
			}

			// Position (common to all)
			if(im::InputInt(EfRow(LBL("Position X")), &p[0], 0, 0)) markModified();
			if(im::InputInt(EfRow(LBL("Position Y")), &p[1], 0, 0)) markModified();

			// Type-specific parameters
			if(no == 1) { // Jump effect
				EfSection(TXT("--- Jump Effect ---"));
				im::DragInt(EfRow(LBL("Duration")), &p[2]);
				if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}
				im::DragInt(EfRow(LBL("Size")), &p[3]);
				if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}
				im::DragInt(EfRow(LBL("Growth rate")), &p[4]);
				if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}
			} else if(no == 3 || no == 9) { // Hitsparks
				EfSection(TXT("--- Hitspark ---"));
				if(no == 3) {
					if(im::InputInt(EfRow(LBL("Glow intensity")), &p[2], 0, 0)) markModified();
					if(im::InputInt(EfRow(LBL("Spark intensity")), &p[3], 0, 0)) markModified();
				} else {
					if(im::InputInt(EfRow(LBL("Spark intensity")), &p[2], 0, 0)) markModified();
				}
			} else if(no == 0) { // Expanding circle
				EfSection(TXT("--- Expanding circle ---"));
				if(im::InputInt(EfRow(LBL("Color")), &p[2], 0, 0)) markModified();
				if(im::InputInt(EfRow(LBL("Duration")), &p[3], 0, 0)) markModified();
				if(im::InputInt(EfRow(LBL("Type")), &p[4], 0, 0)) markModified();
			} else if(no == 16) { // Vertical wave
				if(im::InputInt(EfRow(LBL("Color")), &p[2], 0, 0)) markModified();
			} else if(no == 23 || no == 24) { // Blinding effect (24: same with a different curve)
				EfSection(TXT("--- Blinding effect ---"));
				if(im::InputInt(EfRow(LBL("Fade in duration (?)")), &p[2], 0, 0)) markModified();
				const char* const blindColors[] = {"0: White", "1: Black", "2: Red"};
				if(EfComboManual("Color", &p[3], blindColors, IM_ARRAYSIZE(blindColors), width*2, width)) markModified();
				if(im::Checkbox(EfRow(LBL("Don't obscure characters")), (bool*)&p[4])) markModified();
				im::SameLine(); im::TextDisabled("(?)");
				if(im::IsItemHovered()) Tooltip("Characters stay visible if this is 1");
				if(im::InputInt(EfRow(LBL("Fade out duration (?)")), &p[5], 0, 0)) markModified();
			} else if(no >= 27 && no <= 30) { // Dust clouds
				EfSection(TXT("--- Dust Cloud ---"));
				im::DragInt(EfRow(LBL("X speed")), &p[2]);
				if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}
				im::DragInt(EfRow(LBL("Y speed")), &p[3]);
				if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}
				im::DragInt(EfRow(LBL("Particle duration")), &p[4]);
				if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}
				if(i18n::Combo(EfRow(LBL("Color")), &p[5], "0: Brown\0001: Black\0002: Purple\0003: White\000")) {
					markModified();
				}
				im::DragInt(EfRow(LBL("Flags")), &p[6]);
				if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}
				im::SameLine(); im::TextDisabled("(?)");
				if(im::IsItemHovered()) Tooltip("If odd, particles go only toward one side (probably a flag)");
				im::DragInt(EfRow(LBL("Particle count")), &p[7]);
				if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}
	} else {
		// Generic
		im::Text(TXT("Parameters:"));
		if(im::InputScalarN("##params", ImGuiDataType_S32, p, 6, NULL, NULL, "%d", 0)) {
			markModified();
		}
	}
}
#endif /* EFFECT_PRESET_H_GUARD */
