#ifndef EFFECT_VISUAL_H_GUARD
#include "../../i18n.h"
#include "effect_form.h"
#include "../../ui/ui_style.h"
#define EFFECT_VISUAL_H_GUARD

// ============================================================================
// Effect Type 2: Visual Effects
// ============================================================================
// Superflash, heat distortion, speed lines, screen effects, etc.
// Param names for No 4/26/50/261/1260: gonp's HA6 doc, 2026-11.
// Various visual-only effects that don't spawn patterns or cause damage
// ============================================================================

static inline void DrawEffectVisual_Type2(Frame_EF& effect, FrameData* frameData, int patternIndex, std::function<void()> markModified)
{
	int* p = effect.parameters;
	int& no = effect.number;
	const float width = ui::Px(75.f);   // follows the font size (ui_style.h)

	// Sub-type dropdown
			// Sub-No table: MBAA_NAME_AUDIT.md 2.2 (Effect2_CreateByNo 0x455620).
			// 2, 3, 5-25 and 27-49 are no-ops in MBAA.
			if(effect.type == 10002) {
				im::TextDisabled(TXT("Type 10002: same sub-No table as type 2, run in the late (deferred) pass"));
			}
			const char* const subTypes[] = {
				"0: System fx 5 (20f, attached to owner)",
				"1: System fx 6 (90f, rising)",
				"4: Random sparkle/speed line (Ciel 182, 183)",
				"26: (Blood) Heat activation",
				"50: Superflash",
				"210: (no MBAA handler?) (Ciel)",
				"256: System fx 17",
				"257: Particle linked to parent",
				"258: Particle inside hurtbox 0",
				"259: System fx 279 (duration p1)",
				"260: Motion line (prev -> current pos)",
				"261: Trailing effect",
				"1260: Fading circle (Boss Aoko)",
			};
			if(EfComboManual("Effect Number", &no, subTypes, IM_ARRAYSIZE(subTypes), width*2, width)) {
				markModified();
			}

			// Sub-type specific parameters
			if(no == 50) { // Superflash
				EfSection(TXT("--- Superflash Parameters ---"));

				im::DragInt(EfRow(LBL("Position X")), &p[0]);
				if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}
				im::DragInt(EfRow(LBL("Position Y")), &p[1]);
				if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}

				if(i18n::Combo(EfRow(LBL("Freeze Mode")), &p[2],
					"0: Freeze self\000"
					"1: Don't freeze self (freezes projectiles)\000"
					"2: Don't freeze self or opponent\000"
					"3: No flash\000")) {
					markModified();
				}

				im::DragInt(EfRow(LBL("Freeze/blackout duration")), &p[3]);
				if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}
				im::SameLine(); im::TextDisabled("(?)");
				if(im::IsItemHovered()) Tooltip("0 = default 30f");

				if(i18n::Combo(EfRow(LBL("Portrait")), &p[4],
					"0: EX portrait\000"
					"1: AD portrait\000"
					"255: No portrait\000")) {
					markModified();
				}

				if(im::Checkbox(EfRow(LBL("Draw in front")), (bool*)&p[5])) markModified();

				im::DragInt(EfRow(LBL("Don't spend meter")), &p[8]);
				if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}
				im::SameLine(); im::TextDisabled("(?)");
				if(im::IsItemHovered()) Tooltip("Non-zero: meter is not spent, but the meter spend value is still set");

				im::DragInt(EfRow(LBL("Meter gain mult")), &p[9]);
				if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}
				im::SameLine(); im::TextDisabled("(?)");
				if(im::IsItemHovered()) Tooltip("256 = 1.0x (default 110), requires Don't spend meter != 0");

				im::DragInt(EfRow(LBL("Mult duration")), &p[10]);
				if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}
				im::SameLine(); im::TextDisabled("(?)");
				if(im::IsItemHovered()) Tooltip("Frames (default 420f), requires Don't spend meter != 0");

	} else if(no == 4) { // Random spawning sparkle/speed line (gonp's HA6 doc, 2026-11)
		EfSection(TXT("--- Random Sparkle / Speed Line ---"));
		if(im::InputInt(EfRow("X1"), &p[0], 0, 0)) markModified();
		if(im::InputInt(EfRow("Y1"), &p[1], 0, 0)) markModified();
		if(im::InputInt(EfRow("X2"), &p[2], 0, 0)) markModified();
		if(im::InputInt(EfRow("Y2"), &p[3], 0, 0)) markModified();
		if(im::InputInt(EfRow(LBL("Stretch (?)")), &p[4], 0, 0)) markModified();
	} else if(no == 26) { // (Blood) Heat
		if(im::InputInt(EfRow(LBL("Position X")), &p[0], 0, 0)) markModified();
		if(im::InputInt(EfRow(LBL("Position Y")), &p[1], 0, 0)) markModified();
	} else if(no == 261) { // Trailing effect
		EfSection(TXT("--- Trailing effect ---"));
		if(im::InputInt(EfRow(LBL("Offset X")), &p[0], 0, 0)) markModified();
		if(im::InputInt(EfRow(LBL("Offset Y")), &p[1], 0, 0)) markModified();
		if(im::InputInt(EfRow(LBL("Trail pattern")), &p[2], 0, 0)) markModified();
		if(frameData && p[2] >= 0 && p[2] < frameData->get_sequence_count()) EfNote(frameData->GetDecoratedName(p[2]).c_str());
		if(im::InputInt(EfRow(LBL("Trail sprite width (X)")), &p[3], 0, 0)) markModified();
	} else if(no == 1260) { // Fading circle (Boss Aoko); always spawns at the origin
		EfSection(TXT("--- Fading circle (spawns at origin) ---"));
		if(im::InputInt(EfRow(LBL("Color (index?)")), &p[2], 0, 0)) markModified();
		if(im::InputInt(EfRow(LBL("Unknown (must not be 6)")), &p[3], 0, 0)) markModified();
		if(im::InputInt(EfRow(LBL("Sprite scale")), &p[4], 0, 0)) markModified();
	} else {
		// Generic parameters for other sub-types
		im::Text(TXT("Parameters:"));
		if(im::InputScalarN("##params", ImGuiDataType_S32, p, 6, NULL, NULL, "%d", 0)) {
			markModified();
		}
		if(im::InputScalarN("##params2", ImGuiDataType_S32, p+6, 6, NULL, NULL, "%d", 0)) {
			markModified();
		}
	}
}

#endif /* EFFECT_VISUAL_H_GUARD */
