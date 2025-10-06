#ifndef EFFECT_DAMAGE_H_GUARD
#define EFFECT_DAMAGE_H_GUARD

// ============================================================================
// Effect Type 5: Damage
// ============================================================================
// Direct damage to opponent
// Includes damage value, hit effects, meter gain, scaling, etc.
// ============================================================================

static inline void DrawEffectDamage_Type5(Frame_EF& effect, FrameData* frameData, int patternIndex, std::function<void()> markModified)
{
	int* p = effect.parameters;
	int& no = effect.number;
	constexpr float width = 75.f;

			const char* const damageTypes[] = {
				"0: Set added effect",
				"1: Damage opponent",
				"4: Unknown (obsolete?)",
			};

			int typeIndex = -1;
			int knownTypes[] = {0, 1, 4};
			for(int j = 0; j < IM_ARRAYSIZE(knownTypes); j++) {
				if(no == knownTypes[j]) {
					typeIndex = j;
					break;
				}
			}

			if(typeIndex >= 0) {
				im::SetNextItemWidth(width*2);
				if(im::Combo("Damage Type", &typeIndex, damageTypes, IM_ARRAYSIZE(damageTypes))) {
					no = knownTypes[typeIndex];
					markModified();
				}
			} else {
				im::SetNextItemWidth(width);
				if(im::InputInt("Type", &no, 0, 0)) {
					markModified();
				}
			}

			if(no == 0) { // Set added effect
				im::SetNextItemWidth(width*2);
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

				if(im::Combo("Effect", &currentIdx, effectNames, IM_ARRAYSIZE(effectNames))) {
					p[0] = effectTypes[currentIdx];
					markModified();
				}
			} else if(no == 1) { // Damage opponent
				im::SetNextItemWidth(width);
				im::DragInt("Damage", &p[0]);
				if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}

				im::SetNextItemWidth(width);
				if(im::Combo("Add to hit count", &p[1], "0\0001\0002\000")) {
					markModified();
				}

				im::SetNextItemWidth(width);
				im::DragInt("Hitstop", &p[2]);
				if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}

				im::SetNextItemWidth(width);
				im::DragInt("Hit sound", &p[3]);
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

				im::SetNextItemWidth(width);
				if(im::Combo("Hit scaling", &p[4], "0\0001\000")) {
					markModified();
				}

				im::SetNextItemWidth(width);
				im::DragInt("VS damage", &p[5]);
				if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}
			} else if(no == 4) { // Unknown/obsolete
				im::SetNextItemWidth(width);
				im::DragInt("Unknown", &p[0]);
				if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}
				im::SameLine(); im::TextDisabled("(?)");
				if(im::IsItemHovered()) {
					Tooltip("Always -10, probably obsolete");
				}
			} else {
				// Unknown damage type
				im::Text("Parameters:");
				if(im::InputScalarN("##params", ImGuiDataType_S32, p, 6, NULL, NULL, "%d", 0)) {
					markModified();
				}
			}

}
#endif /* EFFECT_DAMAGE_H_GUARD */
