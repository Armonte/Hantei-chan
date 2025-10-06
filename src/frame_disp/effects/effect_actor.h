#ifndef EFFECT_ACTOR_H_GUARD
#define EFFECT_ACTOR_H_GUARD

// ============================================================================
// Effect Type 8: Spawn Actor
// ============================================================================
// Spawns actors from effect.ha6 file
// Used for special visual effects, particles, overlays, etc.
// ============================================================================

static inline void DrawEffectActor_Type8(Frame_EF& effect, FrameData* frameData, int patternIndex, std::function<void()> markModified)
{
	int* p = effect.parameters;
	int& no = effect.number;
	constexpr float width = 75.f;


			const char* const actorTypes[] = {
				"5: Clash",
				"26: Charge [X]",
			};

			if(ShowComboWithManual("Actor", &no, actorTypes, IM_ARRAYSIZE(actorTypes), width*2, width)) {
				markModified();
			}

			// Pattern dropdown (same as type 1)
			im::SetNextItemWidth(width);
			im::DragInt("Offset X", &p[0]);
			if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}
			im::SameLine(0, 20);
			im::SetNextItemWidth(width);
			im::DragInt("Offset Y", &p[1]);
			if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}

			// Show flagsets collapsed
			if(im::TreeNode("Flagset 1 (same as Type 1)")) {
				unsigned int flagIdx = -1;
				if(BitField("##flags1", (unsigned int*)&p[2], &flagIdx, 13)) {
					markModified();
				}
				im::Text("Raw value: %d", p[2]);
				im::TreePop();
			}

			if(im::TreeNode("Flagset 2 (same as Type 1)")) {
				unsigned int flagIdx = -1;
				if(BitField("##flags2", (unsigned int*)&p[3], &flagIdx, 13)) {
					markModified();
				}
				im::Text("Raw value: %d", p[3]);
				im::TreePop();
			}


}
#endif /* EFFECT_ACTOR_H_GUARD */
