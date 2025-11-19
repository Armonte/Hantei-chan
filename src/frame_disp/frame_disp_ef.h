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

template<typename GroupClipboardType = std::vector<Frame_EF>>
inline void EfDisplay(std::vector<Frame_EF> *efList_, Frame_EF *singleClipboard = nullptr, FrameData *frameData = nullptr, int patternIndex = -1, std::function<void()> onModified = nullptr, GroupClipboardType *groupClipboard = nullptr)
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
	
	// Track collapsed state per item
	static std::vector<bool> collapsedStates;
	if(collapsedStates.size() != efList.size()) {
		collapsedStates.resize(efList.size(), false);
	}

	int deleteI = -1;
	static int dragSourceIndex = -1;
	static bool reorderedThisFrame = false;
	reorderedThisFrame = false; // Reset at start of frame
	
	for(int i = 0; i < efList.size(); i++)
	{
		im::PushID(i);
		
		// Build header label with effect type
		int typeValue = efList[i].type;
		int typeIndex = -1;
		int knownTypes[] = {0, 1, 2, 3, 4, 5, 6, 8, 9, 11, 14, 101, 111, 257, 1000, 10002};
		for(int j = 0; j < IM_ARRAYSIZE(knownTypes); j++) {
			if(typeValue == knownTypes[j]) {
				typeIndex = j;
				break;
			}
		}
		
		char headerLabel[256];
		if(typeIndex >= 0 && typeIndex < IM_ARRAYSIZE(effectTypes)) {
			snprintf(headerLabel, sizeof(headerLabel), "Effect %d: %s", i, effectTypes[typeIndex]);
		} else {
			snprintf(headerLabel, sizeof(headerLabel), "Effect %d: Type %d", i, typeValue);
		}
		
		// Track start position of item
		ImVec2 itemStartPos = im::GetCursorScreenPos();
		
		// Drag handle button - use ASCII characters for compatibility
		if(im::Button("=", ImVec2(20, 20))) {
			// Button click does nothing, just for visual
		}
		if(im::BeginDragDropSource(ImGuiDragDropFlags_None)) {
			im::SetDragDropPayload("EFFECT_ITEM", &i, sizeof(int));
			im::Text("Moving effect %d", i);
			im::EndDragDropSource();
			dragSourceIndex = i;
		}
		im::SameLine();
		
		// Collapsible header - use stored collapsed state
		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_None;
		if(!collapsedStates[i]) {
			flags |= ImGuiTreeNodeFlags_DefaultOpen;
		}
		bool isOpen = im::CollapsingHeader(headerLabel, flags);
		collapsedStates[i] = !isOpen; // Update stored state
		
		if(isOpen) {
			im::Indent();
			
			// Type dropdown (with fallback for unlisted types)
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
			
			im::Unindent();
		}
		
		// Track end position and create drop target covering entire item
		ImVec2 itemEndPos = im::GetCursorScreenPos();
		float itemHeight = itemEndPos.y - itemStartPos.y;
		if(itemHeight < 20.0f) itemHeight = 20.0f; // Minimum height
		
		// Create invisible button covering entire item area
		im::SetCursorScreenPos(itemStartPos);
		im::InvisibleButton("##item_dropzone", ImVec2(-1, itemHeight));
		
		// Make the entire item area a drop target with top/bottom split
		if(im::BeginDragDropTarget()) {
			ImVec2 mousePos = im::GetMousePos();
			float midPoint = itemStartPos.y + itemHeight * 0.5f;
			bool isTopHalf = mousePos.y < midPoint;
			
			// Visual feedback: highlight the half we're hovering over
			ImU32 highlightColor = im::GetColorU32(ImGuiCol_DragDropTarget);
			ImVec2 windowPos = im::GetWindowPos();
			ImVec2 windowSize = im::GetWindowSize();
			float drawMidPoint = itemStartPos.y + itemHeight * 0.5f;
			float itemWidth = windowSize.x - (itemStartPos.x - windowPos.x);
			
			if(isTopHalf) {
				// Highlight top half
				im::GetWindowDrawList()->AddRectFilled(
					ImVec2(itemStartPos.x, itemStartPos.y),
					ImVec2(itemStartPos.x + itemWidth, drawMidPoint),
					(highlightColor & 0x00FFFFFF) | 0x20000000); // Semi-transparent
				im::GetWindowDrawList()->AddLine(
					ImVec2(itemStartPos.x, drawMidPoint),
					ImVec2(itemStartPos.x + itemWidth, drawMidPoint),
					highlightColor, 3.0f);
			} else {
				// Highlight bottom half
				im::GetWindowDrawList()->AddRectFilled(
					ImVec2(itemStartPos.x, drawMidPoint),
					ImVec2(itemStartPos.x + itemWidth, itemEndPos.y),
					(highlightColor & 0x00FFFFFF) | 0x20000000); // Semi-transparent
				im::GetWindowDrawList()->AddLine(
					ImVec2(itemStartPos.x, drawMidPoint),
					ImVec2(itemStartPos.x + itemWidth, drawMidPoint),
					highlightColor, 3.0f);
			}
			
			if(const ImGuiPayload* payload = im::AcceptDragDropPayload("EFFECT_ITEM")) {
				if(!reorderedThisFrame) {
					int sourceIdx = *(const int*)payload->Data;
					if(sourceIdx != i && sourceIdx >= 0 && sourceIdx < efList.size()) {
						// Determine desired final position in the ORIGINAL array
						int desiredPos;
						if(isTopHalf) {
							// Insert above this item
							desiredPos = i;
						} else {
							// Insert below this item
							desiredPos = i + 1;
						}
						
						// Reorder: move source to target position
						Frame_EF temp = efList[sourceIdx];
						int tempManual = manualEditMode[sourceIdx];
						bool tempCollapsed = collapsedStates[sourceIdx];
						
						// Remove from source
						efList.erase(efList.begin() + sourceIdx);
						manualEditMode.erase(manualEditMode.begin() + sourceIdx);
						collapsedStates.erase(collapsedStates.begin() + sourceIdx);
						
						// Calculate insert position in the NEW array (after removal)
						int insertPos = desiredPos;
						if(sourceIdx < desiredPos) {
							// Source was removed before the desired position, so adjust
							insertPos = desiredPos - 1;
						}
						
						// Clamp insert position
						if(insertPos < 0) insertPos = 0;
						if(insertPos > efList.size()) insertPos = efList.size();
						
						// Insert at target position
						efList.insert(efList.begin() + insertPos, temp);
						manualEditMode.insert(manualEditMode.begin() + insertPos, tempManual);
						collapsedStates.insert(collapsedStates.begin() + insertPos, tempCollapsed);
						markModified();
						reorderedThisFrame = true;
					}
				}
			}
			im::EndDragDropTarget();
		}
		
		// Restore cursor position
		im::SetCursorScreenPos(itemEndPos);

		im::PopID();
	}


	// Handle deletion
	if(deleteI >= 0) {
		efList.erase(efList.begin() + deleteI);
		manualEditMode.erase(manualEditMode.begin() + deleteI);
		collapsedStates.erase(collapsedStates.begin() + deleteI);
		markModified();
	}

	if(im::Button("Add effect")) {
		efList.push_back({});
		manualEditMode.push_back(0);
		collapsedStates.push_back(false); // New items start expanded
		markModified();
	}

	if(groupClipboard) {
		im::SameLine(0,20.f);
		if(im::Button("Copy all")) {
			CopyVectorContents<Frame_EF>(*groupClipboard, efList);
		}
		im::SameLine(0,20.f);
		if(im::Button("Paste all")) {
			CopyVectorContents<Frame_EF>(efList, *groupClipboard);
			markModified();
		}
		im::SameLine(0,20.f);
		if(im::Button("Add copy")) {
			if(singleClipboard) {
				efList.push_back(*singleClipboard);
				manualEditMode.push_back(0);
				markModified();
			}
		}
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
