#ifndef FRAME_DISP_EF_H_GUARD
#define FRAME_DISP_EF_H_GUARD

#include "frame_disp_common.h"
#include "../cg.h"
#include <vector>

// ============================================================================
// Effect (EF) Display - MODULARIZED
// ============================================================================
// Displays and edits Frame_EF data (effect/action system)
//
// Frame_EF controls all effects and actions with 16+ types split into categories:
// - Pattern spawning (types 1, 11, 101, 111)
// - Visual effects (type 2)
// - Preset effects (type 3)
// - Opponent state (types 4, 14)
// - Damage (type 5)
// - Misc effects (type 6) - THE BIG ONE with 30+ sub-types!
// - Actors (type 8)
// - Audio (type 9)
// - Special/unknown (types 257, 1000, 10002)
//
// Each category is in its own file in effects/ directory for better organization
// ============================================================================

// Include all effect category handlers
#include "effects/effect_spawn.h"
#include "effects/effect_visual.h"
#include "effects/effect_preset.h"
#include "effects/effect_state.h"
#include "effects/effect_damage.h"
#include "effects/effect_misc.h"
#include "effects/effect_actor.h"
#include "effects/effect_audio.h"
#include "effects/effect_unknown.h"

// Forward declaration
static inline void DrawSmartEffectUI(Frame_EF& effect, FrameData* frameData, int patternIndex, std::function<void()> markModified);

inline void EfDisplay(std::vector<Frame_EF> *efList_, Frame_EF *singleClipboard = nullptr, FrameData *frameData = nullptr, int patternIndex = -1, std::function<void()> onModified = nullptr)
{
	// Helper lambda to mark both frameData and character as modified
	auto markModified = [&]() {
		if (frameData && patternIndex >= 0) {
			frameData->mark_modified(patternIndex);
		}
		if (onModified) {
			onModified();
		}
	};

	std::vector<Frame_EF> & efList = *efList_;
	constexpr float width = 75.f;

	// Manual edit mode tracking
	static std::vector<int> manualEditMode;
	if(manualEditMode.size() != efList.size()) {
		manualEditMode.resize(efList.size(), 0);
	}

	int deleteI = -1;
	for(int i = 0; i < efList.size(); i++)
	{
		if(i>0) im::Separator();
		im::PushID(i);

		// Type dropdown (with fallback for unlisted types)
		int typeValue = efList[i].type;
		int typeIndex = -1;
		int knownTypes[] = {0, 1, 2, 3, 4, 5, 6, 8, 9, 11, 14, 101, 111, 257, 1000, 10002};
		for(int j = 0; j < IM_ARRAYSIZE(knownTypes); j++) {
			if(typeValue == knownTypes[j]) {
				typeIndex = j;
				break;
			}
		}

		if(typeIndex >= 0) {
			im::SetNextItemWidth(width*3);
			if(im::Combo("Type", &typeIndex, effectTypes, IM_ARRAYSIZE(effectTypes))) {
				efList[i].type = knownTypes[typeIndex];
				markModified();
			}
		} else {
			im::SetNextItemWidth(width);
			if(im::InputInt("Type", &efList[i].type, 0, 0)) {
				markModified();
			}
		}

		im::SameLine(0.f, 20);
		bool manualMode = manualEditMode[i] != 0;
		if(im::Checkbox("Manual", &manualMode)) {
			manualEditMode[i] = manualMode ? 1 : 0;
		}
		if(im::IsItemHovered()) {
			Tooltip("Enable raw parameter editing for undocumented values");
		}

		im::SameLine(0.f, 20);
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1,0,0,0.4));
		if(im::Button("Delete"))
			deleteI = i;
		ImGui::PopStyleColor();

		// Parameters
		int* p = efList[i].parameters;
		int& no = efList[i].number;

		if(manualEditMode[i]) {
			// Raw parameter editing
			im::SetNextItemWidth(width);
			if(im::InputInt("Number", &no, 0, 0)) {
				markModified();
			}
			im::Text("Raw parameters:");
			if(im::InputScalarN("##params", ImGuiDataType_S32, p, 6, NULL, NULL, "%d", 0)) {
				markModified();
			}
			if(im::InputScalarN("##params2", ImGuiDataType_S32, p+6, 6, NULL, NULL, "%d", 0)) {
				markModified();
			}
		} else {
			// Smart UI based on effect type
			DrawSmartEffectUI(efList[i], frameData, patternIndex, markModified);
		}

		im::SameLine();
		if(singleClipboard && im::Button("Copy")) {
			*singleClipboard = efList[i];
		}

		im::PopID();
	}

	// Handle deletion
	if(deleteI >= 0) {
		efList.erase(efList.begin() + deleteI);
		manualEditMode.erase(manualEditMode.begin() + deleteI);
		markModified();
	}

	if(im::Button("Add effect")) {
		efList.push_back({});
		manualEditMode.push_back(0);
		markModified();
	}
}

// Smart Effect UI dispatcher - calls appropriate handler based on effect type
static inline void DrawSmartEffectUI(Frame_EF& effect, FrameData* frameData, int patternIndex, std::function<void()> markModified)
{
	switch(effect.type) {
		case 1:   // Spawn Pattern
		case 101: // Spawn Relative Pattern
			DrawEffectSpawn_Type1_101(effect, frameData, patternIndex, markModified);
			break;

		case 2: // Various Effects
			DrawEffectVisual_Type2(effect, frameData, patternIndex, markModified);
			break;

		case 3: // Spawn Preset Effect
			DrawEffectPreset_Type3(effect, frameData, patternIndex, markModified);
			break;

		case 11:  // Spawn Random Pattern
		case 111: // Spawn Random Relative Pattern
			DrawEffectSpawn_Type11_111(effect, frameData, patternIndex, markModified);
			break;

		case 4:  // Set Opponent State (no bounce reset)
		case 14: // Set Opponent State (reset bounces)
			DrawEffectState_Type4_14(effect, frameData, patternIndex, markModified);
			break;

		case 5: // Damage
			DrawEffectDamage_Type5(effect, frameData, patternIndex, markModified);
			break;

		case 6: // Various Effects 2 (THE BIG ONE!)
			DrawEffectMisc_Type6(effect, frameData, patternIndex, markModified);
			break;

		case 8: // Spawn Actor (effect.ha6)
			DrawEffectActor_Type8(effect, frameData, patternIndex, markModified);
			break;

		case 9: // Play Audio
			DrawEffectAudio_Type9(effect, frameData, patternIndex, markModified);
			break;

		case 257:  // Arc typo
		case 1000: // Spawn and follow
		case 10002: // Unknown
			DrawEffectUnknown(effect, frameData, patternIndex, markModified);
			break;

		default:
			// Fallback for completely unknown types
			int* p = effect.parameters;
			int& no = effect.number;
			constexpr float width = 75.f;
			
			im::SetNextItemWidth(width);
			if(im::InputInt("Number", &no, 0, 0)) {
				markModified();
			}
			im::Text("Unknown effect type - raw parameters:");
			if(im::InputScalarN("##params", ImGuiDataType_S32, p, 6, NULL, NULL, "%d", 0)) {
				markModified();
			}
			if(im::InputScalarN("##params2", ImGuiDataType_S32, p+6, 6, NULL, NULL, "%d", 0)) {
				markModified();
			}
			break;
	}
}

#endif /* FRAME_DISP_EF_H_GUARD */
