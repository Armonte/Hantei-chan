#ifndef EFFECT_AUDIO_H_GUARD
#include "../../i18n.h"
#include "effect_form.h"
#define EFFECT_AUDIO_H_GUARD

// ============================================================================
// Effect Type 9: Play Audio
// ============================================================================
// Plays sound effects
// Includes sound ID, volume, pitch, pan, etc.
// ============================================================================

static inline void DrawEffectAudio_Type9(Frame_EF& effect, FrameData* frameData, int patternIndex, std::function<void()> markModified)
{
	int* p = effect.parameters;
	int& no = effect.number;

			// Param names: gonp's HA6 doc, 2026-11.
			if(i18n::Combo(EfRow(LBL("Bank")), &no,
				"0: Universal sounds\000"
				"1: Character specific\000"
				"2: ?\000"
				"3: ?\000"
				"4: Cancel all of the character's sounds\000")) {
				markModified();
			}

			im::DragInt(EfRow(LBL("Sound ID")), &p[0]);
			if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}

			im::DragInt(EfRow(LBL("Probability")), &p[1]);
			if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}
			im::SameLine(); im::TextDisabled("(?)");
			if(im::IsItemHovered()) {
				Tooltip("0 = always plays; otherwise chance = value/512 (max 512)");
			}

			im::DragInt(EfRow(LBL("Different sounds")), &p[2]);
			if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}
			im::SameLine(); im::TextDisabled("(?)");
			if(im::IsItemHovered()) {
				Tooltip("1 and 0 equivalent, IDs adjacent");
			}

			if(im::Checkbox(EfRow(LBL("Cancel other sounds")), (bool*)&p[3])) markModified();
			im::SameLine(); im::TextDisabled("(?)");
			if(im::IsItemHovered()) {
				Tooltip("Cancel other audio from the same character and bank");
			}

			if(im::InputInt(EfRow(LBL("Character ID filter")), &p[5], 0, 0)) markModified();
			im::SameLine(); im::TextDisabled("(?)");
			if(im::IsItemHovered()) {
				Tooltip("Character ID to play the sound against (255 to disable, with filter mode 2)");
			}

			const char* const filterModes[] = {
				"0: Ignore character ID",
				"1: Use character ID",
				"2: Block later sounds this frame if this one plays",
			};
			if(EfComboManual("Filter mode", &p[6], filterModes, IM_ARRAYSIZE(filterModes))) markModified();
			im::SameLine(); im::TextDisabled("(?)");
			if(im::IsItemHovered()) {
				Tooltip("2: disables sounds on the same frame after this one, if this one plays");
			}

			const char* const rngPools[] = {
				"0: RNG pool 10 (gameplay default)",
				"1: RNG pool 2",
			};
			if(EfComboManual("RNG pool", &p[11], rngPools, IM_ARRAYSIZE(rngPools))) markModified();
}

#endif /* EFFECT_AUDIO_H_GUARD */
