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

			if(i18n::Combo(EfRow(LBL("Bank")), &no,
				"0: Universal sounds\000"
				"1: Character specific\000")) {
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
				Tooltip("0 = 100%");
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

			im::DragInt(EfRow(LBL("Unknown (param4)")), &p[3]);
			if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}
			im::SameLine(); im::TextDisabled("(?)");
			if(im::IsItemHovered()) {
				Tooltip("Sometimes 1");
			}

			im::DragInt(EfRow(LBL("Unknown (param6)")), &p[5]);
			if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}

			im::DragInt(EfRow(LBL("Unknown (param7)")), &p[6]);
			if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}
			im::SameLine(); im::TextDisabled("(?)");
			if(im::IsItemHovered()) {
				Tooltip("Either 2 or 0, probably related to param6");
			}

			im::DragInt(EfRow(LBL("Unknown (param12)")), &p[11]);
			if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}
			im::SameLine(); im::TextDisabled("(?)");
			if(im::IsItemHovered()) {
				Tooltip("Sometimes 1, used in time up/intro/win");
			}

}

#endif /* EFFECT_AUDIO_H_GUARD */
