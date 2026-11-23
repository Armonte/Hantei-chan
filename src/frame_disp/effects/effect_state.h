#ifndef EFFECT_STATE_H_GUARD
#include "../../i18n.h"
#include "effect_form.h"
#define EFFECT_STATE_H_GUARD

// ============================================================================
// Effect Types 4, 14: Opponent State Changes
// ============================================================================
// Type 4:  Set Opponent State (no bounce reset)
// Type 14: Set Opponent State (reset bounces)
//
// Controls opponent's state: knockback, positioning, animation, hitstun, etc.
// Param / flag names: gonp's HA6 doc, 2026-11 (param numbering from 1, p1 = p[0]).
// ============================================================================

static inline void DrawEffectState_Type4_14(Frame_EF& effect, FrameData* frameData, int patternIndex, std::function<void()> markModified)
{
	int* p = effect.parameters;
	int& no = effect.number;
	const float width = ui::Px(75.f);   // follows the font size (ui_style.h)

	// Pattern dropdown for opponent state
			if(frameData) {
				std::string currentName = frameData->GetDecoratedName(no);
				if(im::BeginCombo(EfRow(LBL("Opponent Pattern")), currentName.c_str())) {
					for(int i = 0; i < frameData->get_sequence_count(); i++) {
						bool selected = (no == i);
						std::string name = frameData->GetDecoratedName(i);
						if(im::Selectable(name.c_str(), selected)) {
							no = i;
							markModified();
						}
						if(selected)
							im::SetItemDefaultFocus();
					}
					im::EndCombo();
				}
			} else {
				if(im::InputInt(EfRow(LBL("Opponent Pattern")), &no, 0, 0)) {
					markModified();
				}
			}
			im::SameLine(); im::TextDisabled("(?)");
			if(im::IsItemHovered()) {
				Tooltip("Usually only 23, 24, 26, 30, 350, 354");
			}

			// Position
			im::DragInt(EfRow(LBL("X pos")), &p[0]);
			if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}
			im::DragInt(EfRow(LBL("Y pos")), &p[1]);
			if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}
			if(effect.type == 14) {
				im::SameLine(); im::TextDisabled("(?)");
				if(im::IsItemHovered()) Tooltip("X/Y are ignored if this is not the first EF14 in the frame");
			}

			// Type 14 specific
			if(effect.type == 14) {
				im::DragInt(EfRow(LBL("Rotation")), &p[2]);
				if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}
				im::SameLine(); im::TextDisabled("(?)");
				if(im::IsItemHovered()) {
					Tooltip("Clockwise rotation: 0=0°, 2500=90°, 5000=180°, 10000=360°");
				}
			} else {
				// Type 4 param3
				im::DragInt(EfRow(LBL("Mirror across opponent")), &p[2]);
				if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}
				im::SameLine(); im::TextDisabled("(?)");
				if(im::IsItemHovered()) {
					Tooltip("Offset self and opponent mirrored across the opponent's position");
				}
			}

			// Flags
			if(im::TreeNode(LBL("Flags"))) {
				unsigned int flagIdx = -1;
				if(BitField("##flags", (unsigned int*)&p[3], &flagIdx, 7)) {
					markModified();
				}

				switch(flagIdx) {
					case 0: Tooltip("Play animation"); break;
					case 1: Tooltip("Reverse vector"); break;
					case 2: Tooltip("Set enemy Y to 0 (requires Play animation)"); break;
					case 3: Tooltip("Make enemy unhittable"); break;
					case 4:
						if(effect.type == 14) {
							Tooltip("Use last position?");
						} else {
							Tooltip("Reset positions (same as X pos, Y pos and Mirror = 0)");
						}
						break;
					case 5: Tooltip("Hard Knockdown"); break;
					case 6: Tooltip("No KO"); break;
				}

				im::Text(TXT("Raw value: %d"), p[3]);
				im::TreePop();
			}

			// EF14 param5/param6 depend on the Play animation flag (bit 0).
			const bool ef14NoAnim = effect.type == 14 && !(p[3] & 1);

			// Vector ID
			if(ef14NoAnim) {
				if(im::InputInt(EfRow(LBL("Grab location")), &p[4], 0, 0)) markModified();
				im::SameLine(); im::TextDisabled("(?)");
				if(im::IsItemHovered()) Tooltip("Play animation off: grab location (see pattern 350, special boxes 10-12)");
			} else {
				if(EfComboManual("Vector ID", &p[4], hitVectorList, IM_ARRAYSIZE(hitVectorList), width*2, width)) {
					markModified();
				}
				im::SameLine(); im::TextDisabled("(?)");
				if(im::IsItemHovered()) {
					Tooltip("Works only if animation plays");
				}
			}

			// Untech time
			im::DragInt(EfRow(ef14NoAnim ? LBL("Opponent frame") : LBL("Untech time")), &p[5]);
			if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}
			im::SameLine(); im::TextDisabled("(?)");
			if(im::IsItemHovered()) {
				Tooltip(ef14NoAnim ? "Play animation off: opponent's state (frame) in the pattern" :
					effect.type == 14 ? "Used with vectors" :
					"0 = infinite, only works for airstun vectors");
			}

			// Type 14 specific param7
			if(effect.type == 14) {
				if(i18n::Combo(EfRow(LBL("Char location")), &p[6], "0: Self\0001: Opponent\0002: Self, allow opponent below ground\000")) {
					markModified();
				}
			} else {
				im::DragInt(EfRow(LBL("Unknown")), &p[6]);
				if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}
				im::SameLine(); im::TextDisabled("(?)");
				if(im::IsItemHovered()) {
					Tooltip("Most of the time it's 0");
				}
			}

	if(effect.type == 14) {
		if(im::InputInt(EfRow(LBL("Unknown (p11)")), &p[10], 0, 0)) markModified();
		im::SameLine(); im::TextDisabled("(?)");
		if(im::IsItemHovered()) Tooltip("Something with p12?");
		if(im::InputInt(EfRow(LBL("Follow .pat interpolation?")), &p[11], 0, 0)) markModified();
		im::SameLine(); im::TextDisabled("(?)");
		if(im::IsItemHovered()) Tooltip("Follow .pat char interpolation? (uncertain)");
	}

	// Opponent's frame
	if(effect.type == 4) {
		im::DragInt(EfRow(LBL("Opponent frame")), &p[7]);
		if(im::IsItemEdited()) {
		if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
	}
	if(im::IsItemDeactivatedAfterEdit()) {
		markModified();
	}
	}
}
#endif /* EFFECT_STATE_H_GUARD */
