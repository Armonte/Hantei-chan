#ifndef EFFECT_UNKNOWN_H_GUARD
#define EFFECT_UNKNOWN_H_GUARD

// ============================================================================
// Effect Types 257, 1000, 10002: Special/Unknown
// ============================================================================
// Type 257:  Arc typo (legacy/unused?)
// Type 1000: Spawn and follow
// Type 10002: Unknown
//
// These are rare, undocumented, or legacy effect types
// ============================================================================

static inline void DrawEffectUnknown(Frame_EF& effect, FrameData* frameData, int patternIndex, std::function<void()> markModified)
{
	int* p = effect.parameters;
	int& no = effect.number;
	constexpr float width = 75.f;

	switch(effect.type) {
		case 257: // Arc typo
		{
			im::Text("Arc pattern 124 typo - no effect");
			im::Text("Parameters:");
			if(im::InputScalarN("##params", ImGuiDataType_S32, p, 6, NULL, NULL, "%d", 0)) {
				markModified();
			}
			break;
		}

		case 1000: // Spawn and follow
		{
			im::Text("Spawns and follows (Dust of Osiris, Sion)");
			im::Text("Probably interacts with pattern data");

			im::SetNextItemWidth(width);
			if(im::InputInt("Pattern", &no, 0, 0)) {
				markModified();
			}

			im::Text("Parameters:");
			if(im::InputScalarN("##params", ImGuiDataType_S32, p, 6, NULL, NULL, "%d", 0)) {
				markModified();
			}
			break;
		}

		case 10002: // Unknown
		{
			im::Text("Only for projectiles");

			im::SetNextItemWidth(width);
			im::DragInt("Param1 (*256)", &p[0]);
			if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}
			im::SameLine(); im::TextDisabled("(?)");
			if(im::IsItemHovered()) {
				Tooltip("Calculates X + (Param1 * 256)");
			}
			break;
		}
	}
}

#endif /* EFFECT_UNKNOWN_H_GUARD */
