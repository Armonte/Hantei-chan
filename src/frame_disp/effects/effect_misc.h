#ifndef EFFECT_MISC_H_GUARD
#define EFFECT_MISC_H_GUARD

// ============================================================================
// Effect Type 6: Various Effects 2 (Miscellaneous)
// ============================================================================
// THE BIG ONE! This type has 30+ sub-types including:
// - Invincibility/armor
// - Teleportation
// - Screen effects (shake, freeze, slowmo)
// - Movement (push, pull, gravity)
// - State changes (crouch, stand, air)
// - Meter/health modifications
// - Counter states
// - And many more...
//
// This is the most complex and feature-rich effect type.
// ============================================================================

static inline void DrawEffectMisc_Type6(Frame_EF& effect, FrameData* frameData, int patternIndex, std::function<void()> markModified)
{
	int* p = effect.parameters;
	int& no = effect.number;
	constexpr float width = 75.f;

	im::SetNextItemWidth(width*2);
			const char* const effect6Types[] = {
				"0: After-image",
				"1: Screen effects",
				"2: Invulnerability",
				"3: Trailing images",
				"4: Gauges",
				"5: Turnaround behavior",
				"6: Set movement vector",
				"9: Set No Input flag",
				"10: Various effects",
				"11: Super flash",
				"12: Various teleports",
				"14: Gauges of char in special box",
				"15: Start/Stop Music",
				"16: Directional movement",
				"19: Prorate",
				"22: Scale and rotation",
				"24: Guard Quality Change",
				"100: Increase Projectile Variable",
				"101: Decrease Projectile Variable",
				"102: Increase Dash variable",
				"103: Decrease Dash variable",
				"105: Change variable",
				"106: Make projectile no despawn on hit",
				"107: Change frames into pattern",
				"110: Count normal as used",
				"111: Store/Load Movement",
				"112: Change proration",
				"113: Rebeat Penalty (22.5%)",
				"114: Circuit Break",
				"150: Command partner",
				"252: Set tag flag",
				"253: Hide chars and delay hit effects",
				"254: Intro check",
				"255: Char + 0x1b1 | 0x10",
			};

			if(ShowComboWithManual("Sub-type", &no, effect6Types, IM_ARRAYSIZE(effect6Types), width*2, width)) {
				markModified();
			}

			// Sub-type specific parameters
			if(no == 0 || no == 3) { // After-image or Trailing images
				im::Text(no == 0 ? "--- After-image (one every 7f) ---" : "--- Trailing images ---");

				im::SetNextItemWidth(width*2);
				if(im::Combo("Blending mode", &p[0],
					"0: Blue (Normal)\000"
					"1: Red (Normal)\000"
					"2: Green (Normal)\000"
					"3: Yellow (Normal)\000"
					"4: Normal (Normal)\000"
					"5: Black (Normal)\000"
					"6: Blue (Additive)\000"
					"7: Red (Additive)\000"
					"8: Green (Additive)\000"
					"9: Yellow (Additive)\000"
					"10: Normal (Additive)\000"
					"11: Normal (Full opacity)\000")) {
					markModified();
				}

				if(no == 3) { // Trailing images only
					im::SetNextItemWidth(width);
					im::DragInt("Number of images", &p[1]);
					if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}

					im::SetNextItemWidth(width);
					im::DragInt("Frames behind", &p[2]);
					if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}
					im::SameLine(); im::TextDisabled("(?)");
					if(im::IsItemHovered()) {
						Tooltip("Frames each image is behind previous (max 32)");
					}
				}

			} else if(no == 1) { // Screen effects
				im::Text("--- Screen Effects ---");

				im::SetNextItemWidth(width);
				im::DragInt("Shake duration", &p[0]);
				if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}

				im::SetNextItemWidth(width*2);
				int screenEffectTypes[] = {0, 1, 2, 3, 4, 5, 31, 32};
				const char* screenEffectNames[] = {
					"0: Screen dim",
					"1: Black bg",
					"2: Blue bg",
					"3: Red bg",
					"4: White bg",
					"5: Red bg with silhouette",
					"31: EX flash facing right",
					"32: EX flash facing left",
				};

				// Find current index
				int screenIdx = 0;
				for(int i = 0; i < IM_ARRAYSIZE(screenEffectTypes); i++) {
					if(p[1] == screenEffectTypes[i]) {
						screenIdx = i;
						break;
					}
				}

				if(im::Combo("Screen effect", &screenIdx, screenEffectNames, IM_ARRAYSIZE(screenEffectNames))) {
					p[1] = screenEffectTypes[screenIdx];
					markModified();
				}

				im::SetNextItemWidth(width);
				im::DragInt("Effect duration", &p[2]);
				if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}

				im::SetNextItemWidth(width);
				im::DragInt("Slowdown duration", &p[3]);
				if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}

			} else if(no == 2) { // Invulnerability
				im::Text("--- Invulnerability ---");

				im::SetNextItemWidth(width);
				im::DragInt("Strike invuln", &p[0]);
				if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}

				im::SetNextItemWidth(width);
				im::DragInt("Throw invuln", &p[1]);
				if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}

			} else if(no == 4) { // Gauges
				im::Text("--- Gauges ---");

				im::SetNextItemWidth(width);
				im::DragInt("Health change", &p[0]);
				if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}
				im::SameLine(); im::TextDisabled("(?)");
				if(im::IsItemHovered()) {
					Tooltip("May not exceed Red Health");
				}

				im::SetNextItemWidth(width);
				im::DragInt("Meter change", &p[1]);
				if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}

				im::SetNextItemWidth(width);
				im::DragInt("MAX/HEAT time", &p[2]);
				if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}
				im::SameLine(); im::TextDisabled("(?)");
				if(im::IsItemHovered()) {
					Tooltip("Only works if owner is in MAX/HEAT/BLOOD HEAT");
				}

				im::SetNextItemWidth(width);
				im::DragInt("Red Health change", &p[3]);
				if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}

			} else if(no == 5) { // Turnaround behavior
				im::Text("--- Turnaround Behavior ---");

				im::SetNextItemWidth(width*2);
				if(im::Combo("Behavior", &p[0],
					"0: Turnaround regardless of input\000"
					"1: Turn to face opponent\000"
					"2: Turnaround on 1/4/7 input\000"
					"3: Turnaround on 3/6/9 input\000"
					"4: Always face right\000"
					"5: Always face left\000"
					"6: Random\000")) {
					markModified();
				}

			} else if(no == 6) { // Set movement vector
				im::Text("--- Set Movement Vector ---");

				im::SetNextItemWidth(width);
				im::DragInt("Min speed", &p[0]);
				if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}

				im::SetNextItemWidth(width);
				im::DragInt("Max speed", &p[1]);
				if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}

				im::SetNextItemWidth(width);
				im::DragInt("Min accel", &p[2]);
				if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}

				im::SetNextItemWidth(width);
				im::DragInt("Max accel", &p[3]);
				if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}

				im::SetNextItemWidth(width);
				if(im::Combo("Axis", &p[4], "0: X\0001: Y\000")) {
					markModified();
				}

				im::SetNextItemWidth(width);
				if(im::Checkbox("Param12=1: param4=angle", (bool*)&p[11])) {
					markModified();
				}

			} else if(no == 9) { // Set No Input flag
				im::SetNextItemWidth(width);
				im::DragInt("Value", &p[0]);
				if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}

			} else if(no == 10) { // Various effects (armor/confusion)
				im::Text("--- Various Effects ---");

				im::SetNextItemWidth(width);
				if(im::Combo("Effect", &p[0], "0: Armor\0001: Confusion\000")) {
					markModified();
				}

				im::SetNextItemWidth(width);
				im::DragInt("Duration", &p[1]);
				if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}

			} else if(no == 11) { // Super flash
				im::Text("--- Super Flash ---");

				im::SetNextItemWidth(width);
				im::DragInt("Global flash dur", &p[0]);
				if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}
				im::SameLine(); im::TextDisabled("(?)");
				if(im::IsItemHovered()) {
					Tooltip("Only if param2 != 0");
				}

				im::SetNextItemWidth(width);
				im::DragInt("Player flash", &p[1]);
				if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}
				im::SameLine(); im::TextDisabled("(?)");
				if(im::IsItemHovered()) {
					Tooltip("Set to 0 if 0");
				}

			} else if(no == 12) { // Various teleports
				im::Text("--- Teleports ---");

				im::SetNextItemWidth(width);
				if(im::Combo("Param12 mode", &p[11],
					"0: Move self/parent relative\000"
					"1: Reset camera and absolute pos\000"
					"2: Move relative to parent/grabbed\000"
					"3: Keep in bounds\000")) {
					markModified();
				}

				if(p[11] == 0) {
					im::SetNextItemWidth(width);
					if(im::Combo("Param5 mode", &p[4],
						"0: Move relative to itself/camera\000"
						"1: Move parent relative to opponent\000"
						"2: Move parent to self\000")) {
						markModified();
					}

					if(p[4] == 0) {
						im::SetNextItemWidth(width);
						if(im::Combo("Position relative to", &p[2],
							"0: param4\0001: Camera edge and floor\000")) {
							markModified();
						}

						im::SetNextItemWidth(width);
						if(im::Combo("Target", &p[3], "0: Parent\0001: Self\000")) {
							markModified();
						}
					}

					im::SetNextItemWidth(width);
					im::DragInt("X offset", &p[0]);
					if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}
					im::SetNextItemWidth(width);
					im::DragInt("Y offset", &p[1]);
					if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}

				} else if(p[11] == 1) {
					im::SetNextItemWidth(width);
					im::DragInt("X position", &p[0]);
					if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}
					im::SetNextItemWidth(width);
					im::DragInt("Y position", &p[1]);
					if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}

					im::SetNextItemWidth(width);
					if(im::Combo("X mode", &p[2],
						"0: Absolute\0001: Relative to facing\000")) {
						markModified();
					}

				} else if(p[11] == 2) {
					im::SetNextItemWidth(width);
					im::DragInt("X offset", &p[0]);
					if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}
					im::SetNextItemWidth(width);
					im::DragInt("Y offset", &p[1]);
					if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}
				}

			} else if(no == 14) { // Gauges of character in special box
				im::Text("--- Gauges (Special Box) ---");

				im::SetNextItemWidth(width);
				if(im::Combo("Target", &p[0],
					"0: Enemies only\000"
					"1: Allies only\000"
					"2: Both\000")) {
					markModified();
				}

				im::SetNextItemWidth(width);
				if(im::InputInt("Health change", &p[1], 0, 0)) markModified();

				im::SetNextItemWidth(width);
				if(im::InputInt("Meter change", &p[2], 0, 0)) markModified();

				im::SetNextItemWidth(width);
				if(im::InputInt("MAX/HEAT time", &p[3], 0, 0)) markModified();

			} else if(no == 15) { // Start/Stop Music
				im::SetNextItemWidth(width);
				if(im::Combo("Music", &p[0], "0: Stop\0001: Start\000")) {
					markModified();
				}

			} else if(no == 16) { // Directional movement
				im::Text("--- Directional Movement ---");

				im::SetNextItemWidth(width);
				if(im::Combo("Param12 mode", &p[11],
					"0: Directional Movement\000"
					"1: Go to camera relative position\000")) {
					markModified();
				}

				if(p[11] == 0) {
					im::SetNextItemWidth(width);
					im::DragInt("Base angle", &p[0]);
					if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}
					im::SameLine(); im::TextDisabled("(?)");
					if(im::IsItemHovered()) {
						Tooltip("Degrees clockwise");
					}

					im::SetNextItemWidth(width);
					im::DragInt("Random angle range", &p[1]);
					if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}

					im::SetNextItemWidth(width);
					im::DragInt("Base speed", &p[2]);
					if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}

					im::SetNextItemWidth(width);
					im::DragInt("Random speed range", &p[3]);
					if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}

				} else {
					im::SetNextItemWidth(width);
					im::DragInt("X", &p[0]);
					if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}

					im::SetNextItemWidth(width);
					im::DragInt("Y", &p[1]);
					if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}

					im::SetNextItemWidth(width);
					im::DragInt("Velocity divisor", &p[2]);
					if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}
					im::SameLine(); im::TextDisabled("(?)");
					if(im::IsItemHovered()) {
						Tooltip("If 1 or 0, travels entire distance in 1 frame");
					}
				}

			} else if(no == 19) { // Prorate
				im::Text("--- Prorate ---");

				im::SetNextItemWidth(width);
				im::DragInt("Proration value", &p[0]);
				if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}

				im::SetNextItemWidth(width);
				int prorateType = p[1];
				// Clamp to valid range for combo
				if(prorateType < 0 || prorateType > 2) prorateType = 0;
				if(im::Combo("Type", &prorateType,
					"0: Absolute\0001: Multiplicative\0002: Subtractive\000")) {
					p[1] = prorateType;
					markModified();
				}

			} else if(no == 22) { // Scale and rotation
				im::Text("--- Scale and Rotation ---");

				im::SetNextItemWidth(width);
				int scaleRotTypes[] = {0, 1, 2, 10};
				const char* scaleRotNames[] = {
					"0: Something with X scale",
					"1: Something with Y scale",
					"2: Something with X and Y scale",
					"10: Something with rotation",
				};

				// Find current index
				int scaleRotIdx = 0;
				for(int i = 0; i < IM_ARRAYSIZE(scaleRotTypes); i++) {
					if(p[5] == scaleRotTypes[i]) {
						scaleRotIdx = i;
						break;
					}
				}

				if(im::Combo("Param6", &scaleRotIdx, scaleRotNames, IM_ARRAYSIZE(scaleRotNames))) {
					p[5] = scaleRotTypes[scaleRotIdx];
					markModified();
				}

			} else if(no == 24) { // Guard Quality Change
				im::Text("--- Guard Quality Change ---");

				im::SetNextItemWidth(width*2);
				int guardActionTypes[] = {0, 1, 5, 6, 7, 10, 11, 15, 20, 21, 22, 23, 25, 30, 31, 32, 35, 42, 43, 44};
				const char* guardActionNames[] = {
					"0: Guard Reset (-50000)",
					"1: Super jump (0)",
					"5: Stand shield (10000)",
					"6: Crouch shield (10000)",
					"7: Air shield (10000)",
					"10: Ground dodge (4000)",
					"11: Air dodge (7500)",
					"15: Backdash (0)",
					"20: Heat/Blood Heat Activation (-50000)",
					"21: Meter Charge (-500, unused)",
					"22: Ground burst (-50000)",
					"23: Air burst (-50000)",
					"25: Throw escape (-5000, unused)",
					"30: Successful stand shield (-15000)",
					"31: Successful crouch shield (-15000)",
					"32: Successful air shield (-5000)",
					"35: Throw escape/Guard Crush (-50000)",
					"42: Meter charge 2F (-500)",
					"43: Meter charge 3F (-333)",
					"44: Meter charge 4F (-250)",
				};

				// Find current index
				int guardIdx = 0;
				for(int i = 0; i < IM_ARRAYSIZE(guardActionTypes); i++) {
					if(p[0] == guardActionTypes[i]) {
						guardIdx = i;
						break;
					}
				}

				if(im::Combo("Guard action", &guardIdx, guardActionNames, IM_ARRAYSIZE(guardActionNames))) {
					p[0] = guardActionTypes[guardIdx];
					markModified();
				}

			} else if(no == 100 || no == 101) { // Increase/Decrease Projectile Variable
				im::Text(no == 100 ? "--- Increase Proj Var ---" : "--- Decrease Proj Var ---");

				im::SetNextItemWidth(width);
				im::DragInt("Param1", &p[0]);
				if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}
				im::SameLine(); im::TextDisabled("(?)");
				if(im::IsItemHovered()) {
					Tooltip("1s place: Amount\n10s place: Variable ID");
				}

			} else if(no == 102 || no == 103) { // Increase/Decrease Dash variable
				im::Text(no == 102 ? "--- Increase Dash Var ---" : "--- Decrease Dash Var ---");

				im::SetNextItemWidth(width);
				im::DragInt("Amount", &p[0]);
				if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}

			} else if(no == 105) { // Change variable
				im::Text("--- Change Variable ---");

				im::SetNextItemWidth(width);
				im::DragInt("Variable ID", &p[0]);
				if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}
				im::SameLine(); im::TextDisabled("(?)");
				if(im::IsItemHovered()) {
					Tooltip("0-9, can overflow with <0 and >9");
				}

				im::SetNextItemWidth(width);
				im::DragInt("Value", &p[1]);
				if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}

				im::SetNextItemWidth(width);
				if(im::Combo("Mode", &p[2], "0: Set\0001: Add\000")) {
					markModified();
				}

			} else if(no == 106) { // Make projectile no longer despawn on hit
				im::Text("Deactivates EFTP1 P3 bit0");

			} else if(no == 107) { // Change frames into pattern
				im::Text("--- Change Frames ---");

				im::SetNextItemWidth(width);
				im::DragInt("Value", &p[0]);
				if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}

				im::SetNextItemWidth(width);
				if(im::Combo("Mode", &p[1], "0: Set\0001: Add\000")) {
					markModified();
				}

			} else if(no == 110) { // Count normal as used
				im::SetNextItemWidth(width);
				im::DragInt("Pattern - 1", &p[0]);
				if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}
				im::SameLine(); im::TextDisabled("(?)");
				if(im::IsItemHovered()) {
					Tooltip("Pattern of normal - 1 (0-8)");
				}

			} else if(no == 111) { // Store/Load Movement
				im::Text("--- Store/Load Movement ---");

				im::SetNextItemWidth(width);
				if(im::Combo("Mode", &p[0], "0: Save\0001: Load\000")) {
					markModified();
				}

				im::SetNextItemWidth(width);
				im::DragInt("Clear movement", &p[1]);
				if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}
				im::SameLine(); im::TextDisabled("(?)");
				if(im::IsItemHovered()) {
					Tooltip("Clear current movement if not 0");
				}

				im::SetNextItemWidth(width);
				im::DragInt("Set stored Y accel", &p[2]);
				if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}
				im::SameLine(); im::TextDisabled("(?)");
				if(im::IsItemHovered()) {
					Tooltip("If not 0 and current Y velocity/accel < 1");
				}

			} else if(no == 112) { // Change proration
				im::Text("--- Change Proration ---");

				im::SetNextItemWidth(width);
				im::DragInt("Proration", &p[0]);
				if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}

				im::SetNextItemWidth(width);
				if(im::Combo("Type", &p[1],
					"0: Override\0001: Multiply\0002: Subtract\000")) {
					markModified();
				}

			} else if(no == 113) { // Rebeat Penalty
				im::Text("Rebeat Penalty (22.5%)");

			} else if(no == 114) { // Circuit Break
				im::Text("Circuit Break");

			} else if(no == 150) { // Command partner
				im::Text("--- Command Partner ---");

				im::SetNextItemWidth(width);
				im::DragInt("Partner pattern", &p[0]);
				if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}

				im::SetNextItemWidth(width);
				if(im::Combo("Condition", &p[1],
					"0: Always\000"
					"1: If assist is grounded\000"
					"2: If assist is airborne\000"
					"3: If assist is standing\000"
					"4: If assist is crouching\000")) {
					markModified();
				}

			} else if(no == 252) { // Set tag flag
				im::SetNextItemWidth(width);
				im::DragInt("Value", &p[0]);
				if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}

			} else if(no == 253) { // Hide chars
				im::Text("Hide chars and delay hit effects");

			} else if(no == 254) { // Intro check
				im::Text("Intro check");

			} else if(no == 255) { // Char + 0x1b1
				im::Text("Char + 0x1b1 | 0x10");

	} else {
		// Generic parameters
		im::Text("Parameters:");
		if(im::InputScalarN("##params", ImGuiDataType_S32, p, 6, NULL, NULL, "%d", 0)) {
			markModified();
		}
		if(im::InputScalarN("##params2", ImGuiDataType_S32, p+6, 6, NULL, NULL, "%d", 0)) {
			markModified();
		}
	}
}
#endif /* EFFECT_MISC_H_GUARD */
