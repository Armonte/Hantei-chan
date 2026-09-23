#ifndef FRAME_DISP_IF_H_GUARD
#define FRAME_DISP_IF_H_GUARD

#include "frame_disp_common.h"
#include "bof_extensions.h"
#include "../cg.h"
#include <vector>

// ============================================================================
// Condition (IF) Display - ORGANIZED BY CATEGORY
// ============================================================================
// Displays and edits Frame_IF data (condition system for branching logic)
//
// Frame_IF controls all conditional branching in patterns with 152 types
// organized into 7 logical categories:
//
// 📥 INPUT CHECKS (Types 1, 6, 7, 11, 31, 35)
//    - Directional input, lever/trigger checks, command inputs
//
// 📍 PHYSICS/POSITION (Types 4, 12, 13, 26, 29, 70)
//    - Vector checks, distance, screen position, borders
//
// ⚔️  COMBAT CHECKS (Types 3, 17, 27, 38, 51, 52, 54)
//    - Hit checks, hit count, getting hurt, shield, throw
//
// 📦 COLLISION CHECKS (Types 14, 15, 19, 20)
//    - Box collisions, captures, reflections
//
// 🎭 STATE CHECKS (Types 5, 18, 21, 28, 30, 32, 36, 37, 41)
//    - KO, character checks, facing direction, meter mode
//
// 🔄 CONTROL FLOW (Types 8, 9, 10, 40)
//    - Random, loops, timers
//
// 🔧 MISC/SYSTEM (Types 2, 16, 22-25, 33, 34, 39, 42, 53, 55, 60+)
//    - Effect despawn, scrolling, BG checks, variables, sound, homing
//
// The switch statement below is organized in these categories for clarity.
// Each category is clearly marked with comments for easy navigation.
//
// Parameter meanings: docs/tag_research/MBAA_NAME_AUDIT.md section 3 (IDA-verified
// MBAA Cond_Dispatch 0x4693E0). Docs number params from 1 (p1 = p[0]).
// Every IF "jump" is Character_JumpToFrame: a FRAME of the current pattern.
// Only some types queue a PATTERN instead (IF 1 below 10000; IF 3/14/25 at
// 10000+; IF 7, 8, 12, 13, 27, 37). The helpers below label each case.
// ============================================================================


template<typename GroupClipboardType = std::vector<Frame_IF>>
inline void IfDisplay(std::vector<Frame_IF> *ifList_, Frame_IF *singleClipboard = nullptr, FrameData *frameData = nullptr, int patternIndex = -1, std::function<void()> onModified = nullptr, GroupClipboardType *groupClipboard = nullptr)
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

	std::vector<Frame_IF> & ifList = *ifList_;
	constexpr float width = 75.f;

	auto ShowTooltipMarker = [&](const char* tooltip) {
		if(tooltip) {
			im::SameLine(); im::TextDisabled("(?)");
			if(im::IsItemHovered()) Tooltip(tooltip);
		}
	};

	// Frame in the current pattern (Character_JumpToFrame). No pattern semantics.
	auto ShowFrameField = [&](const char* label, int* value, const char* tooltip = nullptr) {
		im::SetNextItemWidth(width);
		if(im::InputInt(label, value, 0, 0)) markModified();
		ShowTooltipMarker(tooltip ? tooltip : "Frame in the current pattern");
	};

	// Pattern number (queued pattern), shown with its name.
	auto ShowPatternField = [&](const char* label, int* value, const char* tooltip = nullptr) {
		im::SetNextItemWidth(width);
		if(im::InputInt(label, value, 0, 0)) markModified();
		if(frameData && *value >= 0 && *value < frameData->get_sequence_count()) {
			im::SameLine(); im::TextDisabled("[%s]", frameData->GetDecoratedName(*value).c_str());
		}
		ShowTooltipMarker(tooltip);
	};

	// IF 3/14/25 style: < 10000 = jump to frame, >= 10000 = queue pattern (value - 10000).
	auto ShowJumpField = [&](const char* label, int* value, const char* tooltip = nullptr) {
		im::SetNextItemWidth(width);
		if(im::InputInt(label, value, 0, 0)) markModified();
		if(frameData && *value >= 10000) {
			int patternNum = *value - 10000;
			if(patternNum >= 0 && patternNum < frameData->get_sequence_count()) {
				im::SameLine(); im::TextDisabled("[queue %s]", frameData->GetDecoratedName(patternNum).c_str());
			}
		}
		ShowTooltipMarker(tooltip ? tooltip : "Frame in the current pattern.\n10000+N queues pattern N instead");
	};

	// Helper lambda to show command IDs with names
	auto ShowCommandField = [&](const char* label, int* value, const char* tooltip = nullptr) {
		im::SetNextItemWidth(width);
		if(im::InputInt(label, value, 0, 0)) markModified();
		if(frameData) {
			Command* cmd = frameData->get_command(*value);
			if(cmd) {
				im::SameLine();
				if(!cmd->comment.empty())
					im::TextDisabled("[%s - %s]", cmd->input.c_str(), cmd->comment.c_str());
				else
					im::TextDisabled("[%s]", cmd->input.c_str());
			}
		}
		if(tooltip) {
			im::SameLine(); im::TextDisabled("(?)");
			if(im::IsItemHovered()) Tooltip(tooltip);
		}
	};

	// Condition type names, character lists, and other labels are now in framedata_labels.h

	int deleteI = -1;
	static int dragSourceIndex = -1;
	static bool reorderedThisFrame = false;
	reorderedThisFrame = false; // Reset at start of frame
	static std::vector<int> manualEditMode; // Track manual edit mode per condition (int instead of bool for ImGui)
	if(manualEditMode.size() != ifList.size()) {
		manualEditMode.resize(ifList.size(), 0);
	}
	
	// Track collapsed state per item
	static std::vector<bool> collapsedStates;
	if(collapsedStates.size() != ifList.size()) {
		collapsedStates.resize(ifList.size(), false);
	}

	for( int i = 0; i < ifList.size(); i++)
	{
		im::PushID(i);
		
		// Build header label with condition type
		int typeIndex = ifList[i].type;
		if(typeIndex < 0) typeIndex = 0;
		char headerLabel[256];
		const auto& condLabels = ConditionTypeLabels();
		if(typeIndex < (int)condLabels.size()) {
			snprintf(headerLabel, sizeof(headerLabel), "Condition %d: %s", i, condLabels[typeIndex]);
		} else {
			snprintf(headerLabel, sizeof(headerLabel), "Condition %d: Type %d", i, ifList[i].type);
		}
		
		// Track start position of item
		ImVec2 itemStartPos = im::GetCursorScreenPos();
		
		// Drag handle button - use ASCII characters for compatibility
		if(im::Button("=", ImVec2(20, 20))) {
			// Button click does nothing, just for visual
		}
		if(im::BeginDragDropSource(ImGuiDragDropFlags_None)) {
			im::SetDragDropPayload("CONDITION_ITEM", &i, sizeof(int));
			im::Text("Moving condition %d", i);
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
			
			// Type dropdown
			if(typeIndex >= (int)condLabels.size()) {
				// Handle special cases (50+, 100+, 150+)
				im::SetNextItemWidth(width*2);
				if(im::InputInt("Type", &ifList[i].type, 0, 0)) markModified();
			} else {
				im::SetNextItemWidth(width*3);
				if(im::Combo("Type", &typeIndex, condLabels.data(), (int)condLabels.size())) {
					ifList[i].type = typeIndex;
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

		// Smart parameter fields based on type
		int* p = ifList[i].parameters;

		// If manual mode is enabled, show raw parameter editing
		if(manualEditMode[i]) {
			im::Text("Raw parameters:");
			if(im::InputScalarN("##params", ImGuiDataType_S32, p, 6, NULL, NULL, "%d", 0)) markModified();
			if(im::InputScalarN("##params2", ImGuiDataType_S32, p+6, 3, NULL, NULL, "%d", 0)) markModified();
		} else {
		// Otherwise show smart UI based on type
		switch(ifList[i].type) {
			case 1: // Jump on lever input
				im::SetNextItemWidth(width);
				if(im::DragInt("Direction (numpad)", &p[0])) markModified(); im::SameLine();
				im::TextDisabled("(?)"); if(im::IsItemHovered()) Tooltip("0=neutral, 2=down, 4=left, 6=right, 8=up, etc.\n10=both 6 and 3");

				// NOTE: IF 1 is the reverse of IF 3/14/25 (MBAA Cond1_JumpOnLeverInput 0x4694BB).
				im::SetNextItemWidth(width);
				if(im::InputInt("Target", &p[1], 0, 0)) markModified();
				if(p[1] >= 10000) {
					im::SameLine(); im::TextDisabled("[frame %d]", p[1] - 10000);
				} else if(frameData && p[1] >= 0 && p[1] < frameData->get_sequence_count()) {
					im::SameLine(); im::TextDisabled("[queue %s]", frameData->GetDecoratedName(p[1]).c_str());
				}
				ShowTooltipMarker("< 10000: queue this pattern (priority 0)\n10000+N: jump to frame N of the current pattern");

				im::SetNextItemWidth(width);
				if(im::Checkbox("Negate condition", (bool*)&p[2])) markModified();
				break;

			case 2: // Effect despawn conditions
			{
				unsigned int flagIdx = -1;
				if(BitField("Despawn flags", (unsigned int*)&p[0], &flagIdx, 2)) markModified();
				switch(flagIdx) {
					case 0: Tooltip("Despawn outside camera X bounds"); break;
					case 1: Tooltip("Despawn outside camera Y bounds"); break;
				}

				im::SetNextItemWidth(width);
				if(im::Checkbox("Despawn on landing", (bool*)&p[1])) markModified();

				im::SetNextItemWidth(width);
				if(im::Checkbox("Despawn on pattern transition", (bool*)&p[2])) markModified();

				im::SetNextItemWidth(width);
				if(im::DragInt("Projectile var decrease", &p[3])) markModified();
				break;
			}

			case 4: // Vector check
				ShowFrameField("Frame to jump to", &p[0]);

				im::SetNextItemWidth(width);
				if(im::Combo("X velocity", &p[1], "0: No check\0001: Backwards (negative)\0002: Forwards (positive)\0")) markModified();

				im::SetNextItemWidth(width);
				if(im::Combo("Y velocity", &p[2], "0: No check\0001: Down (positive)\0002: Up (negative)\0")) markModified();
				break;

			case 8: // Random check
				if(p[2]) ShowPatternField("Pattern to queue", &p[0], "Queues pattern p1 + random(0, p4)");
				else ShowFrameField("Frame to jump to", &p[0]);

				im::SetNextItemWidth(width);
				if(im::DragInt("Chance", &p[1])) markModified(); im::SameLine();
				im::TextDisabled("(?)"); if(im::IsItemHovered()) Tooltip("Passes if random(512) < chance (max 512)");

				im::SetNextItemWidth(width);
				if(im::Checkbox("Queue pattern instead of frame jump", (bool*)&p[2])) markModified();

				if(p[2]) {
					im::SetNextItemWidth(width);
					if(im::DragInt("Random pattern range", &p[3])) markModified();
					ShowTooltipMarker("Pattern = p1 + random(0, range)");
				}

				im::SetNextItemWidth(width);
				if(im::Checkbox("Deterministic RNG (p8)", (bool*)&p[7])) markModified();
				break;

			case 3: // Branch on hit
				ShowJumpField("Jump to", &p[0]);

				if(ShowComboWithManual("Hit condition", &p[1], hitConditions, IM_ARRAYSIZE(hitConditions), width*2, width)) markModified();

				im::SetNextItemWidth(width*2);
				if(im::Combo("Opponent state", &p[2], opponentStateList, IM_ARRAYSIZE(opponentStateList))) markModified();

				im::SetNextItemWidth(width);
				if(im::DragInt("Projectile var decrease", &p[3])) markModified();
				break;

			case 6: // Lever & Trigger check (Frame)
			case 7: // Lever & Trigger check (Pattern)
				im::SetNextItemWidth(width);
				if(im::DragInt("Lever direction", &p[0])) markModified(); im::SameLine();
				im::TextDisabled("(?)"); if(im::IsItemHovered()) Tooltip("Numpad notation\n0=5(neutral), 5=nothing, 10=6/4, 13=1/2/3, 255=any");

				{
					unsigned int flagIdx = -1;
					if(BitField("Buttons", (unsigned int*)&p[1], &flagIdx, 8)) markModified();
					switch(flagIdx) {
						case 0: Tooltip("A button"); break;
						case 1: Tooltip("B button"); break;
						case 2: Tooltip("C button"); break;
						case 3: Tooltip("D button"); break;
					}
					im::SameLine(); im::TextDisabled("(?)");
					if(im::IsItemHovered()) Tooltip("255 = buttons don't matter");
				}

				im::SetNextItemWidth(width);
								if(ifList[i].type == 6) ShowFrameField("Frame to jump to", &p[2], "Frame in the current pattern (+ current frame if p5 != 0)");
				else ShowPatternField("Pattern to queue", &p[2], "Queued at priority 500");

				{
					unsigned int flagIdx = -1;
					if(BitField("Flags", (unsigned int*)&p[3], &flagIdx, 8)) markModified();
					switch(flagIdx) {
						case 0: Tooltip("Negate the condition"); break;
						case 1: Tooltip("Can be held"); break;
						case 2: Tooltip("Require any button (instead of all)"); break;
					}
				}
				break;

			case 11: // Additional command input check
			{
				ShowCommandField("Command ID", &p[0], "From _c.txt file, column 1");

				const char* const cond11When[] = {
					"0: Always",
					"1: On hit, block, or clash",
					"2: On hit",
					"3: On block or clash",
				};
				im::SetNextItemWidth(width*2);
				if(ShowComboWithManual("When", &p[1], cond11When, IM_ARRAYSIZE(cond11When), width*2, width)) markModified();

				im::SetNextItemWidth(width);
								if(im::DragInt("Target frame (?)", &p[2])) markModified();

				const char* const cond11State[] = {
					"0: Always",
					"1: Grounded hit (according to When)",
					"2: Air hit (according to When)",
				};
				im::SetNextItemWidth(width*2);
				if(ShowComboWithManual("Opponent state", &p[3], cond11State, IM_ARRAYSIZE(cond11State), width*2, width)) markModified();

				im::SetNextItemWidth(width);
				if(im::DragInt("Priority", &p[8])) markModified(); im::SameLine();
				im::TextDisabled("(?)"); if(im::IsItemHovered()) Tooltip("0-5000: priority = 5000 - value\n10000+: priority = value - 10000");
				break;
			}

			case 35: // Custom cancel command check
			{
				const char* const cancelConditions[] = {
					"0: Always",
					"1: On hit, block, or clash",
					"2: On hit",
					"3: On block or clash",
					"256: On grounded hit, block, clash, or shield",
					"257: On grounded hit, block, or clash",
					"258: On grounded hit",
					"259: On grounded block or clash",
					"512: On air hit, block, clash, or shield",
					"513: On air hit, block, or clash",
					"514: On air hit",
					"515: On air block or clash",
				};

				im::SetNextItemWidth(width*3);
				{
					int condIdx = p[0];
					const char* condPreview = "Custom value";
					for(int h = 0; h < IM_ARRAYSIZE(cancelConditions); h++) {
						if(atoi(cancelConditions[h]) == condIdx) {
							condPreview = cancelConditions[h];
							break;
						}
					}
					if(im::BeginCombo("Condition", condPreview)) {
						for(int h = 0; h < IM_ARRAYSIZE(cancelConditions); h++) {
							bool selected = (atoi(cancelConditions[h]) == condIdx);
							if(im::Selectable(cancelConditions[h], selected)) {
								p[0] = atoi(cancelConditions[h]);
								markModified();
							}
							if(selected)
								im::SetItemDefaultFocus();
						}
						im::EndCombo();
					}
				}

				ShowCommandField("Command ID 1", &p[1], "From _c.txt file, column 1");
				ShowCommandField("Command ID 2", &p[2], nullptr);
				ShowCommandField("Command ID 3", &p[3], nullptr);
				ShowCommandField("Command ID 4", &p[4], nullptr);

				im::SetNextItemWidth(width);
				if(im::DragInt("Priority", &p[8])) markModified(); im::SameLine();
				im::TextDisabled("(?)"); if(im::IsItemHovered()) Tooltip("0-5000: priority = 5000 - value\n10000+: priority = value - 10000");
				break;
			}

			case 13: // Screen edge check (MBAA Cond13_ScreenEdgeCheck)
			{
				ShowPatternField("Pattern", &p[0], "Pattern to queue when at the edge (-1 = none)");
				im::SetNextItemWidth(width);
				if(im::Checkbox("Queue now (prio 1000)", (bool*)&p[1])) markModified();
				ShowTooltipMarker("Off: pattern is stored as pending (priority 255)");
				im::SetNextItemWidth(width);
				if(im::InputInt("Stance filter", &p[2], 0, 0)) markModified();
				ShowTooltipMarker("255 = any stance");
				im::SetNextItemWidth(width);
				if(im::InputInt("Distance from edge (px)", &p[3], 0, 0)) markModified();
				im::SetNextItemWidth(width);
				if(im::InputInt("Edge side", &p[4], 0, 0)) markModified();
				ShowTooltipMarker("0 = either, 1/2 = exclude one side (facing-relative)");
				unsigned int flagIdx = -1;
				if(BitField("Actions", (unsigned int*)&p[5], &flagIdx, 2)) markModified();
				switch(flagIdx) {
					case 0: Tooltip("Reverse X velocity"); break;
					case 1: Tooltip("Flip facing"); break;
				}
				break;
			}

			case 14: // Box collision check
			{
				ShowJumpField("Jump to", &p[0]);

				const char* const cond14Target[] = {
					"0: Enemies only",
					"1: Allies only",
					"2: Both",
				};
				im::SetNextItemWidth(width*2);
				if(ShowComboWithManual("Check target", &p[1], cond14Target, IM_ARRAYSIZE(cond14Target), width*2, width)) markModified();
				im::SameLine(); im::TextDisabled("(?)");
				if(im::IsItemHovered()) Tooltip("Add +100X for 'not cornered' check");

				const char* const cond14BoxType[] = {
					"0: Hitbox (no cmd grabs, clashable)",
					"1: Collision Box",
					"2: Hurtbox",
					"3: Special Box 1",
					"4: Special Box 2",
					"5: Special Box 3",
					"6: Special Box 4",
					"7: Special Box 5",
					"8: Special Box 6",
					"9: Special Box 7",
					"10: Special Box 8",
					"11: Special Box 9",
					"12: Special Box 10",
					"13: Special Box 11",
					"14: Special Box 12",
					"15: Special Box 13",
					"16: Special Box 14",
					"17: Special Box 15",
					"18: Special Box 16",
					"1000: No hit consumption, just hitbox",
					"10000: Stand guardable, no hitgrabs",
					"20000: Crouch guardable, no hitgrabs",
					"30000: No hitgrabs",
					"40000: Stand guardable, hitgrabs only",
					"50000: Crouch guardable, hitgrabs only",
					"60000: Hitgrabs only",
					"70000: Stand guardable",
					"80000: Crouch guardable",
				};
				im::SetNextItemWidth(width*3);
				if(ShowComboWithManual("Box type", &p[2], cond14BoxType, IM_ARRAYSIZE(cond14BoxType), width*2, width)) markModified();

				im::SetNextItemWidth(width);
				if(im::DragInt("Hitstop on collision", &p[3])) markModified();

				const char* const cond14Turn[] = {
					"0: No turnaround",
					"2: Turn towards collider",
					"4: Turn away from collider",
				};
				im::SetNextItemWidth(width*2);
				if(ShowComboWithManual("Turn direction", &p[4], cond14Turn, IM_ARRAYSIZE(cond14Turn), width*2, width)) markModified();
				bof::DrawIf14Filters(p, width, markModified); // Extended profile only
				break;
			}

			case 16: // Be affected by scrolling
				im::SetNextItemWidth(width);
				if(im::Checkbox("By X", (bool*)&p[0])) markModified();

				im::SetNextItemWidth(width);
				if(im::Checkbox("By Y", (bool*)&p[1])) markModified();
				break;

			case 17: // Branch according to number of hits
				ShowFrameField("Frame to jump to", &p[0]);

				im::SetNextItemWidth(width);
				if(im::DragInt("Number of hits", &p[1])) markModified();
				break;

			case 24: // Projectile flag/variable check
				ShowFrameField("Frame to jump to", &p[0]);

				im::SetNextItemWidth(width);
				if(im::DragInt("Variable ID and Value", &p[1])) markModified(); im::SameLine();
				im::TextDisabled("(?)"); if(im::IsItemHovered()) Tooltip("1s place: Value\n10s place: Variable ID");
				break;

			case 21: // Enemy character check (off in TAG/TEAM modes)
				ShowFrameField("Frame to jump to", &p[0], "Frame in the current pattern.\nDisabled in TAG/TEAM modes");

				if(ShowComboWithManual("Character", &p[1], characterList, IM_ARRAYSIZE(characterList), width*2, width)) markModified();
				im::SameLine(); im::TextDisabled("(?)");
				if(im::IsItemHovered()) Tooltip("Add 50 for boss version");
				break;

			case 25: // Variable comparison
				ShowJumpField("Jump to", &p[0]);

				im::SetNextItemWidth(width);
				if(im::DragInt("Variable ID", &p[1])) markModified();

				im::SetNextItemWidth(width);
				if(im::DragInt("Compare value", &p[2])) markModified();

				im::SetNextItemWidth(width*2);
				if(im::Combo("Comparison", &p[3], comparisonTypes, IM_ARRAYSIZE(comparisonTypes))) markModified();
				break;

			case 26: // Check lever and change vector
				im::SetNextItemWidth(width);
				if(im::DragInt("Direction", &p[0])) markModified(); im::SameLine();
				im::TextDisabled("(?)"); if(im::IsItemHovered()) Tooltip("0-9: Numpad direction\n10: Only 6/4 (x only)\n11: 4,6,7,8,9 (x+y)");

				im::SetNextItemWidth(width);
				if(im::DragInt("X Speed per frame", &p[1])) markModified();

				im::SetNextItemWidth(width);
				if(im::DragInt("Y Speed per frame", &p[2])) markModified();

				im::SetNextItemWidth(width);
				if(im::DragInt("Max speed", &p[3])) markModified();
				break;

			case 27: // Branch when owner thrown/hurt/blocking
			{
				if(p[1] == 0) ShowPatternField("Pattern to queue", &p[0], "Queued at priority 300");
				else ShowFrameField("Frame to jump to", &p[0]);

				im::SetNextItemWidth(width);
				int jumpKind = p[1] != 0;
				if(im::Combo("Jump type", &jumpKind, "Queue pattern\0Jump to frame\0")) { p[1] = jumpKind; markModified(); }

				unsigned int flagIdx = -1;
				if(BitField("Trigger flags", (unsigned int*)&p[2], &flagIdx, 3)) markModified();
				switch(flagIdx) {
					case 0: Tooltip("When the owner is thrown"); break;
					case 1: Tooltip("When the owner is in hitstun (not blocking)"); break;
					case 2: Tooltip("When the owner is blocking"); break;
				}
				break;
			}

			case 31: // Change variable on command input
				ShowCommandField("Move ID", &p[0], "Command ID from _c.txt");

				im::SetNextItemWidth(width);
				if(im::DragInt("Variable ID", &p[1])) markModified();

				im::SetNextItemWidth(width);
				if(im::InputInt("Value", &p[2], 0, 0)) markModified();
				break;

			case 30: // Facing direction check
				im::SetNextItemWidth(width);
				if(im::DragInt("Frame to jump to", &p[0])) markModified();

				im::SetNextItemWidth(width);
				if(im::Combo("Check facing", &p[1], "Right\0Left\0")) markModified();
				break;

			case 33: // Sound effect state check (medium confidence)
				ShowFrameField("Frame to jump to", &p[0]);
				im::SetNextItemWidth(width);
				if(im::InputInt("Sound ID (?)", &p[1], 0, 0)) markModified();
				ShowTooltipMarker("255 = any character-slot sound (?)");
				im::SetNextItemWidth(width);
				if(im::InputInt("Mode (?)", &p[2], 0, 0)) markModified();
				break;

			case 34: // Homing
				im::SetNextItemWidth(width);
				if(im::Combo("Tracking type", &p[8], "Accelerated\0Instant\0")) markModified();
				im::Text("Other params: See Hime/CMech patterns");
				break;

			case 37: // MBAA: queue pattern unconditionally (was "jump by selected colour" in MBAC/hantei4)
				ShowPatternField("Pattern to queue", &p[0], "Queued at priority 300");
				im::TextDisabled("MBAA queues this pattern with no condition;");
				im::TextDisabled("the MBAC colour check no longer exists.");
				break;

			case 38: // Change variable on hit
			{
				im::SetNextItemWidth(width);
				if(im::DragInt("Value", &p[0])) markModified();

				const char* const cond38When[] = {
					"0: On hit",
					"1: On hit/block",
					"2: On hit/clash",
					"3: On hit/block/clash",
					"5: On block",
					"6: On clash",
					"7: On block/clash",
				};
				im::SetNextItemWidth(width*2);
				if(ShowComboWithManual("When", &p[1], cond38When, IM_ARRAYSIZE(cond38When), width*2, width)) markModified();

				im::SetNextItemWidth(width*2);
				if(im::Combo("Opponent state", &p[2], opponentStateList, IM_ARRAYSIZE(opponentStateList))) markModified();

				im::SetNextItemWidth(width);
				if(im::DragInt("Extra variable ID", &p[3])) markModified();

				const char* const cond38Op[] = {
					"0: Set",
					"1: Add",
					"10: Set owner var",
					"11: Add owner var",
				};
				im::SetNextItemWidth(width*2);
				if(ShowComboWithManual("Operation", &p[4], cond38Op, IM_ARRAYSIZE(cond38Op), width*2, width)) markModified();
				break;
			}

			case 40: // Jump after N frames
				im::SetNextItemWidth(width);
				if(im::DragInt("Frame to jump to", &p[0])) markModified();

				im::SetNextItemWidth(width);
				if(im::DragInt("Number of frames", &p[1])) markModified();
				break;

			case 51: // Check Shield Conditions
				im::SetNextItemWidth(width);
				if(im::DragInt("Frame on D release", &p[0])) markModified();

				im::SetNextItemWidth(width);
				if(im::DragInt("Frame on shield success", &p[1])) markModified();

				im::SetNextItemWidth(width);
				if(im::DragInt("Pattern on EX shield", &p[2])) markModified();
				break;

			case 52: // Throw check
				im::SetNextItemWidth(width);
				if(im::DragInt("Frame on throw success", &p[0])) markModified();

				im::SetNextItemWidth(width);
				if(im::Checkbox("Air throw", (bool*)&p[1])) markModified();

				im::SetNextItemWidth(width);
				if(im::DragInt("Frame if combo air throw", &p[2])) markModified();
				break;

			case 54: // Jump when owner thrown or hit
				ShowFrameField("Frame to jump to", &p[0], "Jumps when the owner is thrown (not in hitstop) or in hitstun");
				break;

			case 70: // Jump on reaching screen border
				im::SetNextItemWidth(width);
				if(im::DragInt("Frame at top", &p[0])) markModified(); im::SameLine();
				im::TextDisabled("(?)"); if(im::IsItemHovered()) Tooltip("-1 for no jump");

				im::SetNextItemWidth(width);
				if(im::DragInt("Frame at bottom", &p[1])) markModified(); im::SameLine();
				im::TextDisabled("(?)"); if(im::IsItemHovered()) Tooltip("-1 for no jump");

				im::SetNextItemWidth(width);
				if(im::DragInt("Frame at left", &p[2])) markModified(); im::SameLine();
				im::TextDisabled("(?)"); if(im::IsItemHovered()) Tooltip("-1 for no jump");

				im::SetNextItemWidth(width);
				if(im::DragInt("Frame at right", &p[3])) markModified(); im::SameLine();
				im::TextDisabled("(?)"); if(im::IsItemHovered()) Tooltip("-1 for no jump");
				break;

			case 5: // Jump if KO flag set
				ShowFrameField("Frame to jump to", &p[0], "Jumps when this actor's KO flag is set");
				break;

			case 9: // Loop counter settings
				im::SetNextItemWidth(width);
				if(im::InputInt("Loop counter value", &p[0], 0, 0)) markModified();
				ShowTooltipMarker("Sets the loop counter (actor +0x21); IF 10 tests it");
				break;

			case 10: // Loop counter check
				ShowFrameField("Frame to jump to", &p[0], "Jumps when the loop counter (set by IF 9) is 0");
				break;

			case 12: // Opponent distance check
			{
				if(p[2] & 2) ShowPatternField("Pattern to queue", &p[0], "Queued at priority 1000 (flag bit 1 set)");
				else ShowFrameField("Frame to jump to", &p[0]);
				im::SetNextItemWidth(width);
				if(im::InputInt("Distance (px)", &p[1], 0, 0)) markModified();
				unsigned int flagIdx = -1;
				if(BitField("Flags", (unsigned int*)&p[2], &flagIdx, 4)) markModified();
				switch(flagIdx) {
					case 0: Tooltip("Target must not be at a wall"); break;
					case 1: Tooltip("Queue pattern p1 (prio 1000) instead of a frame jump"); break;
					case 2: Tooltip("Height test"); break;
					case 3: Tooltip("Needs Character_IsStateValid"); break;
				}
				im::SetNextItemWidth(width);
				if(im::InputInt("Lever filter", &p[3], 0, 0)) markModified();
				im::SetNextItemWidth(width);
				if(im::InputInt("X offset", &p[4], 0, 0)) markModified();
				break;
			}

			case 15: // Box collision check (Capture)
			{
				ShowFrameField("Frame to jump to", &p[0], "Grabs the box target, then jumps to this frame");
				const char* const cond15Target[] = {
					"0: Enemies only",
					"1: Allies only",
					"2: Both",
				};
				if(ShowComboWithManual("Check target", &p[1], cond15Target, IM_ARRAYSIZE(cond15Target), width*2, width)) markModified();
				im::SetNextItemWidth(width);
				if(im::InputInt("Own box", &p[2], 0, 0)) markModified();
				ShowTooltipMarker("Same box numbering as IF 14 (value % 1000)");
				im::SetNextItemWidth(width);
				if(im::InputInt("Hitstop on contact", &p[3], 0, 0)) markModified();
				break;
			}

			case 18: // Check owner pattern
				ShowFrameField("Frame to jump to", &p[0]);
				im::SetNextItemWidth(width);
				if(im::InputInt("Owner pattern", &p[1], 0, 0)) markModified();
				ShowTooltipMarker("Objects only: jumps when the parent/owner is in this pattern");
				break;

			case 19: // Projectile box contact (reflection)
				im::SetNextItemWidth(width*2);
				{
					int selfMode = p[7] != 0;
					if(im::Combo("Mode (p8)", &selfMode, "0: Reflect the box-6 target\0001: Self switches team\000")) { p[7] = selfMode; markModified(); }
				}
				if(p[7]) {
					ShowFrameField("Frame to jump to", &p[0], "Self switches to the attacker's team and jumps here");
				} else {
					im::SetNextItemWidth(width);
					if(im::InputInt("X speed (/256)", &p[0], 0, 0)) markModified();
					ShowTooltipMarker("Reflected target (and its children): team swapped,\nX velocity * -value/256 (0 = plain reverse)");
				}
				break;

			case 20: // Box collision check 2
				ShowFrameField("Frame to jump to", &p[0]);
				im::SetNextItemWidth(width);
				if(im::InputInt("Search kind (?)", &p[1], 0, 0)) markModified();
				im::SetNextItemWidth(width);
				if(im::InputInt("Box", &p[2], 0, 0)) markModified();
				im::SetNextItemWidth(width);
				if(im::InputInt("On-screen test / hitstop (?)", &p[3], 0, 0)) markModified();
				ShowTooltipMarker("1 = target must be 8-312 px on screen");
				break;

			case 22: // BG number check
				ShowFrameField("Frame to jump to", &p[0]);
				im::SetNextItemWidth(width);
				if(im::InputInt("Stage ID", &p[1], 0, 0)) markModified();
				break;

			case 23: // BG type check
				im::Text("Removed in MBAA - this condition does nothing");
				break;

			case 28: // Jump if round-end phase started
				ShowFrameField("Frame to jump to", &p[0], "Jumps once the round-end phase has started\n(g_RoundEndState != 0; EF6 No 253 advances it).\nThis is not the actor's own KO state.");
				break;

			case 29: // Check X pos on screen
				ShowFrameField("Frame to jump to", &p[0]);
				im::SetNextItemWidth(width);
				if(im::InputInt("Screen X", &p[1], 0, 0)) markModified();
				im::SetNextItemWidth(width);
				if(im::InputInt("Direction", &p[2], 0, 0)) markModified();
				ShowTooltipMarker("Jumps and clamps the position when past Screen X");
				break;

			case 32: // Jump if CPU side of CPU battle
				im::Text("Arcade mode feature (used in Hime's intro)");
				ShowFrameField("Frame to jump to", &p[0]);
				im::SetNextItemWidth(width);
				if(im::InputInt("CPU side param 1 (?)", &p[1], 0, 0)) markModified();
				im::SetNextItemWidth(width);
				if(im::InputInt("CPU side param 2 (?)", &p[2], 0, 0)) markModified();
				break;

			case 36: // Circuit (meter) mode check
			{
				ShowFrameField("Frame to jump to", &p[0]);

				const char* const meterModes[] = {
					"0: Normal",
					"1: HEAT",
					"2: MAX",
					"3: BLOOD HEAT",
					"4: (Unused)",
					"5: Unlimited (training only)",
				};
				im::SetNextItemWidth(width*2);
				if(ShowComboWithManual("Meter mode", &p[1], meterModes, IM_ARRAYSIZE(meterModes), width*2, width)) markModified();

				im::SetNextItemWidth(width*2);
				if(im::Combo("Comparison", &p[2], "0: Equals\0001: Not equals\0")) markModified();
				break;
			}

			case 39: // Partner pattern check
				ShowFrameField("Frame to jump to", &p[0]);
				im::SetNextItemWidth(width);
				if(im::InputInt("Partner pattern", &p[1], 0, 0)) markModified();
				ShowTooltipMarker("Jumps when the team's non-point member is in this pattern\n(pattern number in the PARTNER's data, e.g. Hisui/Kohaku 246).\nDisabled in TAG/TEAM modes");
				break;

			case 41: // Jump if not team point
				ShowFrameField("Frame to jump to", &p[0], "Jumps when this character has a partner and is not the point");
				break;

			case 42: // Jump if NOT at match point
				ShowFrameField("Frame to jump to", &p[0], "Jumps when this side is NOT at match point\n(Team_IsAtMatchPoint == 0)");
				break;

			case 53: // Destroy if owner left shield
				im::Text("Destroys this object when the owner's frame has no IF 51");
				im::TextDisabled("(shield condition) and the owner is no longer shielding. No parameters.");
				break;

			case 55: // 1P/2P side branch
				ShowFrameField("Frame if even slot (1P side)", &p[0]);
				ShowFrameField("Frame if odd slot (2P side)", &p[1]);
				break;

			case 60: // Meter charge hold loop
				ShowFrameField("Frame on release", &p[0], "First run starts meter charge. Holds while the charge buttons are held,\nthe character is in circuit state 0 and not hit; otherwise jumps here");
				break;

			case 50: // Attack box touches teammate box 10
				ShowFrameField("Frame to jump to", &p[0], "Jumps when own attack boxes touch a TEAMMATE's box 10\n(hurtbox index 9). Not an opponent check (medium confidence)");
				break;

			case 100: // Partner box contact (tag touch)
				ShowFrameField("Frame to jump to", &p[0], "Point (tagFlag 0): own box #10 touches the partner ->\nBOTH characters jump to this frame and the tag request advances.\nNon-point (tagFlag != 0): copies the partner's hitstop instead.\n(MBAC used box 6; MBAA uses box #10)");
				im::TextDisabled("Used by Hisui/Kohaku pattern 53 (touch), frame 4, p1 = 5");
				break;

			case 150: // KO revive check
				ShowFrameField("Frame to jump to", &p[0], "Jumps when KO'd and revives used < CharaSystemData DestroyNum");
				break;

			case 151: // KO revive
				im::Text("Revive: revives++, health = red health = max, KO state cleared");
				im::TextDisabled("No parameters");
				break;

			case 154: case 155: case 156: case 157: // BOF only (Extended profile)
				if(bof::DrawCondition(ifList[i].type, p, width, frameData, markModified)) break;
				[[fallthrough]];
			default:
				// Generic parameter display for unknown/unimplemented types
				im::Text("Parameters:");
				if(im::InputScalarN("##params", ImGuiDataType_S32, p, 6, NULL, NULL, "%d", 0)) {
		markModified();
	}
				if(im::InputScalarN("##params2", ImGuiDataType_S32, p+6, 3, NULL, NULL, "%d", 0)) {
		markModified();
	}
				break;
		}
		} // End of manual mode else block

			// Copy button for all condition types
			if(singleClipboard && im::Button("Copy")) {
				*singleClipboard = ifList[i];
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
			
			if(const ImGuiPayload* payload = im::AcceptDragDropPayload("CONDITION_ITEM")) {
				if(!reorderedThisFrame) {
					int sourceIdx = *(const int*)payload->Data;
					if(sourceIdx != i && sourceIdx >= 0 && sourceIdx < ifList.size()) {
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
						Frame_IF temp = ifList[sourceIdx];
						int tempManual = manualEditMode[sourceIdx];
						bool tempCollapsed = collapsedStates[sourceIdx];
						
						// Remove from source
						ifList.erase(ifList.begin() + sourceIdx);
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
						if(insertPos > ifList.size()) insertPos = ifList.size();
						
						// Insert at target position
						ifList.insert(ifList.begin() + insertPos, temp);
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


	if(deleteI >= 0) {
		ifList.erase(ifList.begin() + deleteI);
		manualEditMode.erase(manualEditMode.begin() + deleteI);
		collapsedStates.erase(collapsedStates.begin() + deleteI);
		markModified();
	}

	if(im::Button("Add")) {
		ifList.push_back({});
		manualEditMode.push_back(0);
		collapsedStates.push_back(false); // New items start expanded
		markModified();
	}

	if(groupClipboard) {
		im::SameLine(0,20.f);
		if(im::Button("Copy all")) {
			CopyVectorContents<Frame_IF>(*groupClipboard, ifList);
		}
		im::SameLine(0,20.f);
		if(im::Button("Paste all")) {
			CopyVectorContents<Frame_IF>(ifList, *groupClipboard);
			markModified();
		}
		im::SameLine(0,20.f);
		if(im::Button("Add copy")) {
			if(singleClipboard) {
				ifList.push_back(*singleClipboard);
				manualEditMode.push_back(0);
				markModified();
			}
		}
	}
}

// Forward declaration for smart effect UI
static inline void DrawSmartEffectUI(Frame_EF& effect, FrameData* frameData, int patternIndex, std::function<void()> markModified);


#endif /* FRAME_DISP_IF_H_GUARD */
