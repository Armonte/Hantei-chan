#ifndef EFFECT_VISUAL_H_GUARD
#define EFFECT_VISUAL_H_GUARD

// ============================================================================
// Effect Type 2: Visual Effects
// ============================================================================
// Superflash, heat distortion, speed lines, screen effects, etc.
// Various visual-only effects that don't spawn patterns or cause damage
// ============================================================================

static inline void DrawEffectVisual_Type2(Frame_EF& effect, FrameData* frameData, int patternIndex, std::function<void()> markModified)
{
	int* p = effect.parameters;
	int& no = effect.number;
	constexpr float width = 75.f;

	// Sub-type dropdown
			const char* const subTypes[] = {
				"4: Random sparkle/speed line",
				"26: (Blood) Heat",
				"50: Superflash",
				"210: Unknown (Ciel)",
				"260: Unknown (Arc/Aoko)",
				"261: Trailing effect",
				"1260: Fading circle (Boss Aoko)",
			};

			// Find index or use custom
			int knownTypes[] = {4, 26, 50, 210, 260, 261, 1260};
			if(ShowComboWithManual("Effect Number", &no, subTypes, IM_ARRAYSIZE(subTypes), width*2, width)) {
				markModified();
			}

			// Sub-type specific parameters
			if(no == 50) { // Superflash
				im::Text("--- Superflash Parameters ---");

				im::SetNextItemWidth(width);
				im::DragInt("Position X", &p[0]);
				if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}
				im::SameLine(0, 20);
				im::SetNextItemWidth(width);
				im::DragInt("Position Y", &p[1]);
				if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}

				im::SetNextItemWidth(width*2);
				if(im::Combo("Freeze Mode", &p[2],
					"0: Freeze self\000"
					"1: Don't freeze self (freezes projectiles)\000"
					"2: Don't freeze self or opponent\000"
					"3: No flash\000")) {
					markModified();
				}

				im::SetNextItemWidth(width);
				im::DragInt("Duration", &p[3]);
				if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}
				im::SameLine(); im::TextDisabled("(?)");
				if(im::IsItemHovered()) Tooltip("0 = default 30f");

				im::SetNextItemWidth(width*2);
				if(im::Combo("Portrait", &p[4],
					"0: EX portrait\000"
					"1: AD portrait\000"
					"255: No portrait\000")) {
					markModified();
				}

				im::SetNextItemWidth(width);
				im::DragInt("Don't spend meter", &p[8]);
				if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}

				im::SetNextItemWidth(width);
				im::DragInt("Meter gain mult", &p[9]);
				if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}
				im::SameLine(); im::TextDisabled("(?)");
				if(im::IsItemHovered()) Tooltip("256 = 1.0x, requires param9 != 0");

				im::SetNextItemWidth(width);
				im::DragInt("Mult duration", &p[10]);
				if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}
				im::SameLine(); im::TextDisabled("(?)");
				if(im::IsItemHovered()) Tooltip("Frames, requires param9 != 0");

	} else {
		// Generic parameters for other sub-types
		im::Text("Parameters:");
		if(im::InputScalarN("##params", ImGuiDataType_S32, p, 6, NULL, NULL, "%d", 0)) {
			markModified();
		}
		if(im::InputScalarN("##params2", ImGuiDataType_S32, p+6, 6, NULL, NULL, "%d", 0)) {
			markModified();
		}
	}
}

#endif /* EFFECT_VISUAL_H_GUARD */
