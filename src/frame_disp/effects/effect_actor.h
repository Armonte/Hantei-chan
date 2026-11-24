#ifndef EFFECT_ACTOR_H_GUARD
#include "../../i18n.h"
#include "effect_form.h"
#include "effect_spawn.h"
#include "../../ui/ui_style.h"
#define EFFECT_ACTOR_H_GUARD

// ============================================================================
// Effect Types 8/108: Spawn Actor
// ============================================================================
// Spawns actors from effect.ha6 file
// Used for special visual effects, particles, overlays, etc.
// ============================================================================

static inline void DrawEffectActor_Type8(Frame_EF& effect, FrameData* frameData, int patternIndex, std::function<void()> markModified)
{
	int* p = effect.parameters;
	int& no = effect.number;
	const float width = ui::Px(75.f);   // follows the font size (ui_style.h)


			const char* const actorTypes[] = {
				"0: Airdash forward rings",
				"1: Airdash back rings",
				"5: Clash",
				"26: Charge [X]",
			};

			if(EfComboManual("Actor", &no, actorTypes, IM_ARRAYSIZE(actorTypes), width*2, width)) {
				markModified();
			}

			// Pattern dropdown (same as type 1)
			im::DragInt(EfRow(LBL("Offset X")), &p[0]);
			if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}
			im::DragInt(EfRow(LBL("Offset Y")), &p[1]);
			if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}

			// Flagset 1
			if(im::TreeNode(LBL("Flagset 1 (Spawn Behavior)"))) {
				unsigned int flagIdx = -1;
				if(BitField("##flags1", (unsigned int*)&p[2], &flagIdx, 13)) {
					markModified();
				}

				// Tooltips based on hovered flag
				EfSpawnFlagset1Tooltip(flagIdx);

				im::Text(TXT("Raw value: %d"), p[2]);
				im::TreePop();
			}

			// Flagset 2
			if(im::TreeNode(LBL("Flagset 2 (Child Properties)"))) {
				unsigned int flagIdx = -1;
				if(BitField("##flags2", (unsigned int*)&p[3], &flagIdx, 13)) {
					markModified();
				}

				// Tooltips based on hovered flag
				EfSpawnFlagset2Tooltip(flagIdx);

				im::Text(TXT("Raw value: %d"), p[3]);
				im::TreePop();
			}

			// Angle: EF8/108 take the same parameters as EF1 (MBAA_NAME_AUDIT.md 2.1); layout as in the Gonptechan EX fork.
			if(im::DragInt(EfRow(LBL("Rotation")), &p[7])) markModified();
			im::SameLine(); im::TextDisabled("(?)");
			if(im::IsItemHovered()) Tooltip("Clockwise rotation: 0=0°, 2500=90°, 5000=180°, 10000=360°");

}
#endif /* EFFECT_ACTOR_H_GUARD */
