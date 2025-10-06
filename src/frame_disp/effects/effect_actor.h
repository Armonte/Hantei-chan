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

			// Flagset 1
			if(im::TreeNode("Flagset 1 (Spawn Behavior)")) {
				unsigned int flagIdx = -1;
				if(BitField("##flags1", (unsigned int*)&p[2], &flagIdx, 13)) {
					markModified();
				}

				// Tooltips based on hovered flag
				switch(flagIdx) {
					case 0: Tooltip("Remove when parent gets hit (includes shield, throw, not clash/armor)"); break;
					case 1: Tooltip("Always face right"); break;
					case 2: Tooltip("Follow parent"); break;
					case 3: Tooltip("Affected by hitstop of parent"); break;
					case 4: Tooltip("Coordinates relative to camera"); break;
					case 5: Tooltip("Remove when parent changes pattern"); break;
					case 6: Tooltip("Can go below floor level"); break;
					case 7: Tooltip("Always on floor"); break;
					case 8: Tooltip("Inherit parent's rotation"); break;
					case 9: Tooltip("Position relative to screen border in the back"); break;
					case 10: Tooltip("Unknown effect"); break;
					case 11: Tooltip("Flip facing"); break;
					case 12: Tooltip("Unknown (Akiha's 217 only)"); break;
				}

				im::Text("Raw value: %d", p[2]);
				im::TreePop();
			}

			// Flagset 2
			if(im::TreeNode("Flagset 2 (Child Properties)")) {
				unsigned int flagIdx = -1;
				if(BitField("##flags2", (unsigned int*)&p[3], &flagIdx, 13)) {
					markModified();
				}

				// Tooltips based on hovered flag
				switch(flagIdx) {
					case 0: Tooltip("Child has shadow"); break;
					case 1: Tooltip("Advance child with parent (also activates Flagset 1 bit 5)"); break;
					case 2: Tooltip("Remove when parent is thrown"); break;
					case 3: Tooltip("Parent is affected by hitstop"); break;
					case 4: case 5: Tooltip("Unknown effect"); break;
					case 6: Tooltip("Unaffected by parent's Type 2 superflash if spawned during it"); break;
					case 7: Tooltip("Doesn't move with camera"); break;
					case 8: Tooltip("Position is relative to opponent"); break;
					case 9: Tooltip("Position is relative to (-32768, 0)"); break;
					case 10: Tooltip("Top Z Priority (overridden if projectile sets own priority)"); break;
					case 11: Tooltip("Face according to player slot"); break;
					case 12: Tooltip("Unaffected by any superflash"); break;
				}

				im::Text("Raw value: %d", p[3]);
				im::TreePop();
			}


}
#endif /* EFFECT_ACTOR_H_GUARD */
