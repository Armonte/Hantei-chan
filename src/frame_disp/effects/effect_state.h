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
					Tooltip("Rotation unit (large values like 1000, 4000 used)");
				}
			} else {
				// Type 4 has unknown param3
				im::DragInt(EfRow(LBL("Unknown")), &p[2]);
				if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}
				im::SameLine(); im::TextDisabled("(?)");
				if(im::IsItemHovered()) {
					Tooltip("Found in old airthrows, probably leftover");
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
					case 2: Tooltip("Can't OTG"); break;
					case 3:
						if(effect.type == 14) {
							Tooltip("Make enemy unhittable");
						} else {
							Tooltip("Unknown");
						}
						break;
					case 4:
						if(effect.type == 14) {
							Tooltip("Use last position?");
						} else {
							Tooltip("Unknown");
						}
						break;
					case 5: Tooltip("Hard Knockdown"); break;
				}

				im::Text(TXT("Raw value: %d"), p[3]);
				im::TreePop();
			}

			// Vector ID
			if(EfComboManual("Vector ID", &p[4], hitVectorList, IM_ARRAYSIZE(hitVectorList), width*2, width)) {
				markModified();
			}
			im::SameLine(); im::TextDisabled("(?)");
			if(im::IsItemHovered()) {
				Tooltip("Works only if animation plays");
			}

			// Untech time
			im::DragInt(EfRow(LBL("Untech time")), &p[5]);
			if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}
			im::SameLine(); im::TextDisabled("(?)");
			if(im::IsItemHovered()) {
				Tooltip(effect.type == 14 ?
					"Used with vectors" :
					"0 = infinite, only works for airstun vectors");
			}

			// Type 14 specific param7
			if(effect.type == 14) {
				if(i18n::Combo(EfRow(LBL("Char location")), &p[6], "0: Self\0001: Opponent\000")) {
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
