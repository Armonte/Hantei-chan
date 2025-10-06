#ifndef EFFECT_STATE_H_GUARD
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
	constexpr float width = 75.f;

	// Pattern dropdown for opponent state
			if(frameData) {
				im::SetNextItemWidth(width*3);
				std::string currentName = frameData->GetDecoratedName(no);
				if(im::BeginCombo("Opponent Pattern", currentName.c_str())) {
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
				im::SetNextItemWidth(width);
				if(im::InputInt("Opponent Pattern", &no, 0, 0)) {
					markModified();
				}
			}
			im::SameLine(); im::TextDisabled("(?)");
			if(im::IsItemHovered()) {
				Tooltip("Usually only 23, 24, 26, 30, 350, 354");
			}

			// Position
			im::SetNextItemWidth(width);
			im::DragInt("X pos", &p[0]);
			if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}
			im::SameLine(0, 20);
			im::SetNextItemWidth(width);
			im::DragInt("Y pos", &p[1]);
			if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}

			// Type 14 specific
			if(effect.type == 14) {
				im::SetNextItemWidth(width);
				im::DragInt("Rotation", &p[2]);
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
				im::SetNextItemWidth(width);
				im::DragInt("Unknown", &p[2]);
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
			if(im::TreeNode("Flags")) {
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

				im::Text("Raw value: %d", p[3]);
				im::TreePop();
			}

			// Vector ID
			im::SetNextItemWidth(width*2);
			if(ShowComboWithManual("Vector ID", &p[4], hitVectorList, IM_ARRAYSIZE(hitVectorList), width*2, width)) {
				markModified();
			}
			im::SameLine(); im::TextDisabled("(?)");
			if(im::IsItemHovered()) {
				Tooltip("Works only if animation plays");
			}

			// Untech time
			im::SetNextItemWidth(width);
			im::DragInt("Untech time", &p[5]);
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
				im::SetNextItemWidth(width);
				if(im::Combo("Char location", &p[6], "0: Self\0001: Opponent\000")) {
					markModified();
				}
			} else {
				im::SetNextItemWidth(width);
				im::DragInt("Unknown", &p[6]);
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
		im::SetNextItemWidth(width);
		im::DragInt("Opponent frame", &p[7]);
		if(im::IsItemEdited()) {
		if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
	}
	if(im::IsItemDeactivatedAfterEdit()) {
		markModified();
	}
	}
}
#endif /* EFFECT_STATE_H_GUARD */
