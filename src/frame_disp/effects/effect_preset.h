#ifndef EFFECT_PRESET_H_GUARD
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
	constexpr float width = 75.f;

	const char* const presetEffects[] = {
				"1: Jump looking effect",
				"2: David star (lags)",
				"3: Red hitspark",
				"4: Force field",
				"6: Fire",
				"7: Snow",
				"8: Blue flash",
				"9: Blue hitspark",
				"10: Superflash background",
				"15: Blue horizontal wave",
				"16: Red vertical wave",
				"19: Foggy rays",
				"20: 3D rotating waves",
				"23: Blinding effect",
				"24: Blinding effect 2",
				"27: Dust cloud",
				"28: Dust cloud (large)",
				"29: Dust cloud (rotating)",
				"30: Massive dust cloud",
			};

			if(ShowComboWithManual("Effect Number", &no, presetEffects, IM_ARRAYSIZE(presetEffects), width*2, width)) {
				markModified();
			}

			// Position (common to all)
			im::SetNextItemWidth(width);
			if(im::InputInt("Position X", &p[0], 0, 0)) markModified();
			im::SameLine(0, 20);
			im::SetNextItemWidth(width);
			if(im::InputInt("Position Y", &p[1], 0, 0)) markModified();

			// Type-specific parameters
			if(no == 1) { // Jump effect
				im::Text("--- Jump Effect ---");
				im::SetNextItemWidth(width);
				im::DragInt("Duration", &p[2]);
				if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}
				im::SetNextItemWidth(width);
				im::DragInt("Size", &p[3]);
				if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}
				im::SetNextItemWidth(width);
				im::DragInt("Growth rate", &p[4]);
				if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}
			} else if(no == 3 || no == 9) { // Hitsparks
				im::Text("--- Hitspark ---");
				im::SetNextItemWidth(width);
				im::DragInt("Intensity", &p[2]);
				if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}
			} else if(no >= 27 && no <= 30) { // Dust clouds
				im::Text("--- Dust Cloud ---");
				im::SetNextItemWidth(width);
				im::DragInt("X speed", &p[2]);
				if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}
				im::SetNextItemWidth(width);
				im::DragInt("Y speed", &p[3]);
				if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}
				im::SetNextItemWidth(width);
				im::DragInt("Duration", &p[4]);
				if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}
				im::SetNextItemWidth(width);
				if(im::Combo("Color", &p[5], "0: Brown\0001: Black\0002: Purple\0003: White\000")) {
					markModified();
				}
				im::SetNextItemWidth(width);
				im::DragInt("Flags", &p[6]);
				if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}
				im::SetNextItemWidth(width);
				im::DragInt("Amount", &p[7]);
				if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}
	} else {
		// Generic
		im::Text("Parameters:");
		if(im::InputScalarN("##params", ImGuiDataType_S32, p, 6, NULL, NULL, "%d", 0)) {
			markModified();
		}
	}
}
#endif /* EFFECT_PRESET_H_GUARD */
