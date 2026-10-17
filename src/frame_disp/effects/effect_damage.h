#ifndef EFFECT_DAMAGE_H_GUARD
#include "../../i18n.h"
#include "effect_form.h"
#include "../../ui/ui_style.h"
#define EFFECT_DAMAGE_H_GUARD

// ============================================================================
// Effect Type 5: Held victim command
// ============================================================================
// MBAA Effect5_HeldVictimCommand 0x45D490 (MBAA_NAME_AUDIT.md 2.1): acts on the
// HELD victim (throw/grab target), not on "the opponent" in general.
// No0 added status, No1 damage, No2 flip victim facing, No3 victim meter.
// ============================================================================

static inline void DrawEffectDamage_Type5(Frame_EF& effect, FrameData* frameData, int patternIndex, std::function<void()> markModified)
{
	int* p = effect.parameters;
	int& no = effect.number;

			const char* const damageTypes[] = {
				"0: Set added effect on victim",
				"1: Damage held victim",
				"2: Flip victim facing",
				"3: Add to victim meter",
				"4: No-op in MBAA (obsolete)",
			};

			int typeIndex = -1;
			int knownTypes[] = {0, 1, 2, 3, 4};
			for(int j = 0; j < IM_ARRAYSIZE(knownTypes); j++) {
				if(no == knownTypes[j]) {
					typeIndex = j;
					break;
				}
			}

			if(typeIndex >= 0) {
				if(i18n::Combo(EfRow(LBL("Damage Type")), &typeIndex, damageTypes, IM_ARRAYSIZE(damageTypes))) {
					no = knownTypes[typeIndex];
					markModified();
				}
			} else {
				if(im::InputInt(EfRow(LBL("Type")), &no, 0, 0)) {
					markModified();
				}
			}

			if(no == 0) { // Set added effect
				int effectTypes[] = {1, 2, 3, 4, 100};
				const char* effectNames[] = {
					"1: Burn",
					"2: Freeze",
					"3: Shock",
					"4: Confuse",
					"100: Black sprite",
				};

				// Find current index
				int currentIdx = 0;
				for(int i = 0; i < IM_ARRAYSIZE(effectTypes); i++) {
					if(p[0] == effectTypes[i]) {
						currentIdx = i;
						break;
					}
				}

				if(i18n::Combo(EfRow(LBL("Effect")), &currentIdx, effectNames, IM_ARRAYSIZE(effectNames))) {
					p[0] = effectTypes[currentIdx];
					markModified();
				}
			} else if(no == 1) { // Damage opponent
				im::DragInt(EfRow(LBL("Damage")), &p[0]);
				if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}

				if(i18n::Combo(EfRow(LBL("Add to hit count")), &p[1], "0\0001\0002\000")) {
					markModified();
				}

				im::DragInt(EfRow(LBL("Hitstop")), &p[2]);
				if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}

				im::DragInt(EfRow(LBL("Hit sound")), &p[3]);
				if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}
				im::SameLine(); im::TextDisabled("(?)");
				if(im::IsItemHovered()) {
					Tooltip("Same as in AT data");
				}

				if(i18n::Combo(EfRow(LBL("Hit scaling")), &p[4], "0\0001\000")) {
					markModified();
				}

				im::DragInt(EfRow(LBL("Red damage")), &p[5]);
				if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}
			} else if(no == 2) { // Flip victim facing
				im::TextDisabled(TXT("No parameters"));
			} else if(no == 3) { // Victim meter
				if(im::InputInt(EfRow(LBL("Meter change")), &p[0], 0, 0)) markModified();
				im::SameLine(); im::TextDisabled("(?)");
				if(im::IsItemHovered()) Tooltip("Victim meter += value, clamped 0..30000");
			} else if(no == 4) { // Not handled by MBAA
				im::DragInt(EfRow(LBL("Unknown")), &p[0]);
				if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}
				im::SameLine(); im::TextDisabled("(?)");
				if(im::IsItemHovered()) {
					Tooltip("Always -10 in data; MBAA's EF5 handler only knows No 0-3, so this does nothing");
				}
			} else {
				// Unknown damage type
				im::Text(TXT("Parameters:"));
				if(im::InputScalarN("##params", ImGuiDataType_S32, p, 6, NULL, NULL, "%d", 0)) {
					markModified();
				}
			}

}
#endif /* EFFECT_DAMAGE_H_GUARD */
