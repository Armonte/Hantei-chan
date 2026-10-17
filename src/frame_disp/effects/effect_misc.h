#ifndef EFFECT_MISC_H_GUARD
#define EFFECT_MISC_H_GUARD

#include "../bof_extensions.h"
#include "../../i18n.h"
#include "effect_form.h"

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
//
// Sub-No table and parameter meanings: docs/tag_research/MBAA_NAME_AUDIT.md 2.3
// (IDA-verified Effect6_Dispatch 0x45D700). Docs number params from 1 (p1 = p[0]).
// All numbers not listed (25-49, 51-99, 104, 108, 109, 115-149, 154, 156-199,
// 201-251) are no-ops in vanilla MBAA. 154-157 are BOF/Extended Melty only.
// ============================================================================

static inline void DrawEffectMisc_Type6(Frame_EF& effect, FrameData* frameData, int patternIndex, std::function<void()> markModified)
{
	int* p = effect.parameters;
	int& no = effect.number;
	const float width = ui::Px(75.f);   // follows the font size (ui_style.h)

			const char* const effect6Types[] = {
				"0: After-image",
				"1: Screen effects",
				"2: Invulnerability",
				"3: Trailing images",
				"4: Gauges",
				"5: Set facing",
				"6: Set movement vector",
				"7: Spawn after-image sprite",
				"8: Accelerate toward enemy",
				"9: Set No Input flag",
				"10: Status timer (armor/confusion)",
				"11: Super flash stop",
				"12: Various teleports",
				"13: Color flash / tint",
				"14: Gauges of char in special box",
				"15: Start/Stop Music",
				"16: Directional movement",
				"17: Camera focus / zoom",
				"18: Camera follows P1 slot",
				"19: Prorate",
				"20: Modify guard gauge (while guarding)",
				"21: Camera zoom preset",
				"22: Scale and rotation",
				"23: Rotate toward floor point",
				"24: Guard Quality Change",
				"50: Camera zoom override",
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
				"151: Set circuit mode, zero meter",
				"152: BG fade mode",
				"153: Z priority to front (452)",
				"155: Show/hide team partner",
				"200: Object tracking mode",
				"252: Set tag flag",
				"253: Advance round-end phase",
				"254: Set intro done",
				"255: Set KO/win animation done",
				"1100: Reverse velocity",
			};

			const auto effect6List = Effect6TypeLabels(effect6Types, IM_ARRAYSIZE(effect6Types)); // + BOF 154 (Extended)
			if(EfComboManual("Sub-type", &no, effect6List.data(), (int)effect6List.size(), width*2, width)) {
				markModified();
			}

			// Sub-type specific parameters
			if(no == 0 || no == 3) { // After-image or Trailing images
				EfSection(no == 0 ? TXT("--- After-image (one every 7f) ---") : TXT("--- Trailing images ---"));

				if(i18n::Combo(EfRow(LBL("Blending mode")), &p[0],
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
					im::DragInt(EfRow(LBL("Number of images")), &p[1]);
					if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}

					im::DragInt(EfRow(LBL("Frames behind")), &p[2]);
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
				EfSection(TXT("--- Screen Effects ---"));

				im::DragInt(EfRow(LBL("Shake duration")), &p[0]);
				if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}

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

				if(i18n::Combo(EfRow(LBL("Screen effect")), &screenIdx, screenEffectNames, IM_ARRAYSIZE(screenEffectNames))) {
					p[1] = screenEffectTypes[screenIdx];
					markModified();
				}

				im::DragInt(EfRow(LBL("Effect duration")), &p[2]);
				if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}

				im::DragInt(EfRow(LBL("Slowdown duration")), &p[3]);
				if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}

			} else if(no == 2) { // Invulnerability
				EfSection(TXT("--- Invulnerability ---"));

				im::DragInt(EfRow(LBL("Strike invuln")), &p[0]);
				if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}

				im::DragInt(EfRow(LBL("Throw invuln")), &p[1]);
				if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}

			} else if(no == 4) { // Gauges
				EfSection(TXT("--- Gauges ---"));

				im::DragInt(EfRow(LBL("Health change")), &p[0]);
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

				im::DragInt(EfRow(LBL("Meter change")), &p[1]);
				if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}

				im::DragInt(EfRow(LBL("MAX/HEAT time")), &p[2]);
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

				im::DragInt(EfRow(LBL("Red Health change")), &p[3]);
				if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}

			} else if(no == 5) { // Turnaround behavior
				EfSection(TXT("--- Turnaround Behavior ---"));

				if(i18n::Combo(EfRow(LBL("Behavior")), &p[0],
					"0: Turnaround regardless of input\000"
					"1: Turn to face opponent\000"
					"2: Turnaround on 1/4/7 input\000"
					"3: Turnaround on 3/6/9 input\000"
					"4: Always face right\000"
					"5: Always face left\000"
					"6: Random\000"
					"7: Copy owner's facing\000")) {
					markModified();
				}

			} else if(no == 6) { // Set movement vector
				EfSection(TXT("--- Set Movement Vector ---"));

				if(i18n::Combo(EfRow(LBL("Mode (p12)")), &p[11],
					"0: Velocity/accel on one axis\000"
					"1: Angle + speed\000"
					"2: Arc to parent\000")) {
					markModified();
				}
				im::SameLine(); im::TextDisabled("(?)");
				if(im::IsItemHovered()) Tooltip("0: vel = p1+rand(p2-p1), accel = p3+rand(p4-p3) on axis p5\n"
					"1: angle p5 (+rand p6) /10000, speed p1 (+rand p2), accel p3; p7=1 decelerates to a stop\n"
					"2: arc to parent (0,p3) in p1 frames with gravity p2");

				im::DragInt(EfRow(LBL("Min speed")), &p[0]);
				if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}

				im::DragInt(EfRow(LBL("Max speed")), &p[1]);
				if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}

				im::DragInt(EfRow(LBL("Min accel")), &p[2]);
				if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}

				im::DragInt(EfRow(LBL("Max accel")), &p[3]);
				if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}

				if(p[11] == 1) {
					if(im::InputInt(EfRow(LBL("Angle (p5)")), &p[4], 0, 0)) markModified();
					if(im::InputInt(EfRow(LBL("Random angle (p6)")), &p[5], 0, 0)) markModified();
					if(im::InputInt(EfRow(LBL("Decelerate to stop (p7)")), &p[6], 0, 0)) markModified();
				} else if(p[11] == 0) {
					if(i18n::Combo(EfRow(LBL("Axis")), &p[4], "0: X\0001: Y\000")) {
						markModified();
					}
				}

			} else if(no == 9) { // Set No Input flag
				im::DragInt(EfRow(LBL("Value")), &p[0]);
				if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}

			} else if(no == 10) { // Status timer (armor/confusion)
				EfSection(TXT("--- Status Timer ---"));

				if(i18n::Combo(EfRow(LBL("Effect")), &p[0], "0: Armor\0001: Confusion\000")) {
					markModified();
				}

				im::DragInt(EfRow(LBL("Duration")), &p[1]);
				if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}

			} else if(no == 11) { // Super flash
				EfSection(TXT("--- Super Flash ---"));

								im::DragInt(EfRow(LBL("Flash/freeze duration")), &p[0]);
				if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}
				im::SameLine(); im::TextDisabled("(?)");
				if(im::IsItemHovered()) {
					Tooltip("Characters only.\np2 != 0: global freeze timer = p1\np2 == 0: per-slot super flash timer = p1");
				}

								im::DragInt(EfRow(LBL("Global freeze")), &p[1]);
				if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}
				im::SameLine(); im::TextDisabled("(?)");
				if(im::IsItemHovered()) {
					Tooltip("Non-zero: p1 sets the global freeze timer instead of the per-slot flash");
				}

			} else if(no == 12) { // Various teleports
				EfSection(TXT("--- Teleports ---"));

				if(i18n::Combo(EfRow(LBL("Param12 mode")), &p[11],
					"0: Move self/parent relative\000"
					"1: Reset camera and absolute pos\000"
					"2: Move relative to parent/grabbed\000"
					"3: Keep in bounds\000")) {
					markModified();
				}

				if(p[11] == 0) {
					if(i18n::Combo(EfRow(LBL("Param5 mode")), &p[4],
						"0: Move relative to itself/camera\000"
						"1: Move parent relative to opponent\000"
						"2: Move parent to self\000")) {
						markModified();
					}

					if(p[4] == 0) {
						if(i18n::Combo(EfRow(LBL("Position relative to")), &p[2],
							"0: param4\0001: Camera edge and floor\000")) {
							markModified();
						}

						if(i18n::Combo(EfRow(LBL("Target")), &p[3], "0: Parent\0001: Self\000")) {
							markModified();
						}
					}

					im::DragInt(EfRow(LBL("X offset")), &p[0]);
					if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}
					im::DragInt(EfRow(LBL("Y offset")), &p[1]);
					if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}

				} else if(p[11] == 1) {
					im::DragInt(EfRow(LBL("X position")), &p[0]);
					if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}
					im::DragInt(EfRow(LBL("Y position")), &p[1]);
					if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}

					if(i18n::Combo(EfRow(LBL("X mode")), &p[2],
						"0: Absolute\0001: Relative to facing\000")) {
						markModified();
					}

				} else if(p[11] == 2) {
					im::DragInt(EfRow(LBL("X offset")), &p[0]);
					if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}
					im::DragInt(EfRow(LBL("Y offset")), &p[1]);
					if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}
				}

			} else if(no == 14) { // Gauges of character in special box
				EfSection(TXT("--- Gauges (Special Box) ---"));

				if(i18n::Combo(EfRow(LBL("Target")), &p[0],
					"0: Enemies only\000"
					"1: Allies only\000"
					"2: Both\000")) {
					markModified();
				}

				if(im::InputInt(EfRow(LBL("Health change")), &p[1], 0, 0)) markModified();

				if(im::InputInt(EfRow(LBL("Meter change")), &p[2], 0, 0)) markModified();

				if(im::InputInt(EfRow(LBL("MAX/HEAT time")), &p[3], 0, 0)) markModified();

			} else if(no == 15) { // Start/Stop Music
				if(i18n::Combo(EfRow(LBL("Music")), &p[0], "0: Stop\0001: Start\000")) {
					markModified();
				}

			} else if(no == 16) { // Directional movement
				EfSection(TXT("--- Directional Movement ---"));

				if(i18n::Combo(EfRow(LBL("Param12 mode")), &p[11],
					"0: Directional Movement\000"
					"1: Go to camera relative position\000")) {
					markModified();
				}

				if(p[11] == 0) {
					im::DragInt(EfRow(LBL("Base angle")), &p[0]);
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

					im::DragInt(EfRow(LBL("Random angle range")), &p[1]);
					if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}

					im::DragInt(EfRow(LBL("Base speed")), &p[2]);
					if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}

					im::DragInt(EfRow(LBL("Random speed range")), &p[3]);
					if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}

				} else {
					im::DragInt(EfRow("X"), &p[0]);
					if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}

					im::DragInt(EfRow("Y"), &p[1]);
					if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}

					im::DragInt(EfRow(LBL("Velocity divisor")), &p[2]);
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
				EfSection(TXT("--- Prorate ---"));

				im::DragInt(EfRow(LBL("Proration value")), &p[0]);
				if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}

				int prorateType = p[1];
				// Clamp to valid range for combo
				if(prorateType < 0 || prorateType > 2) prorateType = 0;
				if(i18n::Combo(EfRow(LBL("Type")), &prorateType,
					"0: Set (keeps the lower)\0001: Multiplicative\0002: Subtractive\000")) {
					p[1] = prorateType;
					markModified();
				}

			} else if(no == 22) { // Scale and rotation
				EfSection(TXT("--- Scale and Rotation ---"));

				if(im::InputInt(EfRow(LBL("Value")), &p[0], 0, 0)) markModified();
				im::SameLine(); im::TextDisabled("(?)");
				if(im::IsItemHovered()) Tooltip("Scale: value/10000. Rotation: 10000 = 360 degrees");
				if(im::InputInt(EfRow(LBL("Random add")), &p[1], 0, 0)) markModified();
				int scaleRotTypes[] = {0, 1, 2, 10};
				const char* scaleRotNames[] = {
					"0: Scale one axis (Y?)",
					"1: Scale other axis (X?)",
					"2: Scale X and Y",
					"10: Rotation",
				};

				// Find current index
				int scaleRotIdx = 0;
				for(int i = 0; i < IM_ARRAYSIZE(scaleRotTypes); i++) {
					if(p[5] == scaleRotTypes[i]) {
						scaleRotIdx = i;
						break;
					}
				}

								if(i18n::Combo(EfRow(LBL("Target (p6)")), &scaleRotIdx, scaleRotNames, IM_ARRAYSIZE(scaleRotNames))) {
					p[5] = scaleRotTypes[scaleRotIdx];
					markModified();
				}

			} else if(no == 24) { // Guard Quality Change
				EfSection(TXT("--- Guard Quality Change ---"));

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

				if(i18n::Combo(EfRow(LBL("Guard action")), &guardIdx, guardActionNames, IM_ARRAYSIZE(guardActionNames))) {
					p[0] = guardActionTypes[guardIdx];
					markModified();
				}

			} else if(no == 100 || no == 101) { // Increase/Decrease Projectile Variable
				EfSection(no == 100 ? TXT("--- Increase Proj Var ---") : TXT("--- Decrease Proj Var ---"));

				im::DragInt(EfRow(LBL("Param1")), &p[0]);
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
				EfSection(no == 102 ? TXT("--- Increase Dash Var ---") : TXT("--- Decrease Dash Var ---"));

				im::DragInt(EfRow(LBL("Amount")), &p[0]);
				if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}

			} else if(no == 105) { // Change variable
				EfSection(TXT("--- Change Variable ---"));
				bof::DrawVar6Presets(p, markModified); // Extended profile only

				im::DragInt(EfRow(LBL("Variable ID")), &p[0]);
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

				im::DragInt(EfRow(LBL("Value")), &p[1]);
				if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}

				const char* const ef6_105Modes[] = {
					"0: Set (team point var)",
					"1: Add (team point var)",
					"10: Set (self)",
					"11: Add (self)",
				};
				if(EfComboManual("Mode", &p[2], ef6_105Modes, IM_ARRAYSIZE(ef6_105Modes), width*2, width)) {
					markModified();
				}

			} else if(no == 154 && bof::DrawEffect154(p, width, markModified)) { // BOF only (Extended profile)
			} else if(no == 106) { // Make projectile no longer despawn on hit
				im::Text(TXT("Deactivates EFTP1 P3 bit0"));

			} else if(no == 107) { // Change frames into pattern
				EfSection(TXT("--- Change Frames ---"));

				im::DragInt(EfRow(LBL("Value")), &p[0]);
				if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}

				if(i18n::Combo(EfRow(LBL("Mode")), &p[1], "0: Set\0001: Add\000")) {
					markModified();
				}

			} else if(no == 110) { // Count normal as used
				im::DragInt(EfRow(LBL("Pattern - 1")), &p[0]);
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
				EfSection(TXT("--- Store/Load Movement ---"));

				if(i18n::Combo(EfRow(LBL("Mode")), &p[0], "0: Save\0001: Load\000")) {
					markModified();
				}

				im::DragInt(EfRow(LBL("Clear movement")), &p[1]);
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

				im::DragInt(EfRow(LBL("Set stored Y accel")), &p[2]);
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
				EfSection(TXT("--- Change Proration ---"));

				im::DragInt(EfRow(LBL("Proration")), &p[0]);
				if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}

				if(i18n::Combo(EfRow(LBL("Type")), &p[1],
										"0: Set (keeps the lower)\0001: Multiply\0002: Subtract\000")) {
					markModified();
				}

			} else if(no == 113) { // Rebeat Penalty
				im::Text(TXT("Rebeat Penalty (22.5%)"));

			} else if(no == 114) { // Circuit Break
				im::Text(TXT("Circuit Break"));

			} else if(no == 150) { // Command partner
				EfSection(TXT("--- Command Partner ---"));

				im::DragInt(EfRow(LBL("Partner pattern")), &p[0]);
				if(im::IsItemEdited()) {
				if (frameData && patternIndex >= 0) frameData->mark_modified(patternIndex);
			}
			if(im::IsItemDeactivatedAfterEdit()) {
				markModified();
			}

				if(i18n::Combo(EfRow(LBL("Condition")), &p[1],
					"0: Always\000"
					"1: If partner is grounded\000"
					"2: If partner is airborne\000"
					"3: If partner is standing\000"
					"4: If partner is crouching\000")) {
					markModified();
				}
				im::TextDisabled(TXT("Queues the pattern on the non-point member (prio 300) if it can act."));
				im::TextDisabled(TXT("Disabled in TAG/TEAM modes."));

			} else if(no == 252) { // Set tag flag
				const char* const tagFlagValues[] = {
					"0: Point (active)",
					"1: Resting (unhittable, untargetable)",
					"2: Resting but acting (hittable, not throwable)",
				};
				if(EfComboManual("Tag flag", &p[0], tagFlagValues, IM_ARRAYSIZE(tagFlagValues), width*3, width)) {
					markModified();
				}
				im::SameLine(); im::TextDisabled("(?)");
				if(im::IsItemHovered()) Tooltip("Owner tagFlag (actor+0x174) = value");

			} else if(no == 253) { // Advance round-end phase
				im::Text(TXT("++g_RoundEndState (advance the round-end phase; IF 28 tests it)"));

			} else if(no == 254) { // Set intro done
				im::Text(TXT("Owner +0x1B0 = 1 (intro finished)"));

			} else if(no == 255) { // KO/win animation done
				im::Text(TXT("Owner koFlags |= 0x10 (down/win animation finished)"));

			} else if(no == 7) { // Spawn after-image sprite
				EfSection(TXT("--- Spawn After-image Sprite ---"));
				im::TextDisabled(TXT("Effect_CreateAfterImage(p1..p5): a timed copy of the current sprite"));
				if(im::InputScalarN("p1..p5 (?)", ImGuiDataType_S32, p, 5, NULL, NULL, "%d", 0)) markModified();
				im::SameLine(); im::TextDisabled("(?)");
				if(im::IsItemHovered()) Tooltip("Exact meaning of each param not confirmed.\nLifetime is probably p4 (?)");

			} else if(no == 8) { // Accelerate toward enemy
				EfSection(TXT("--- Accelerate Toward Enemy ---"));
				if(im::InputInt(EfRow(LBL("Base frame")), &p[0], 0, 0)) markModified();
				im::SameLine(); im::TextDisabled("(?)");
				if(im::IsItemHovered()) Tooltip("Used only if Angle sectors != 0:\njumps to frame = base + sector of the angle to the enemy");
				if(im::InputInt(EfRow(LBL("Speed")), &p[1], 0, 0)) markModified();
				im::SameLine(); im::TextDisabled("(?)");
				if(im::IsItemHovered()) Tooltip("velocity += speed toward the opponent (or nearest enemy point); accel cleared");
				if(im::InputInt(EfRow(LBL("Angle sectors")), &p[2], 0, 0)) markModified();

			} else if(no == 13) { // Color flash / tint
				EfSection(TXT("--- Color Flash / Tint ---"));
				if(im::InputInt(EfRow(LBL("Tint mode")), &p[0], 0, 0)) markModified();
				im::SameLine(); im::TextDisabled("(?)");
				if(im::IsItemHovered()) Tooltip("Same fields the hit flash uses (+0x1F2)");
				if(im::InputInt(EfRow(LBL("Duration")), &p[1], 0, 0)) markModified();

			} else if(no == 17) { // Camera focus / zoom
				EfSection(TXT("--- Camera Focus / Zoom ---"));
				if(i18n::Combo(EfRow(LBL("Mode (p12)")), &p[11], "0: Camera focus\0001: Zoom target\000")) markModified();
				if(p[11] == 0) {
					if(i18n::Combo(EfRow(LBL("Focus")), &p[0], "0: Clear focus\0001: Focus on own slot\000")) markModified();
				} else {
					if(im::InputInt(EfRow(LBL("Flag (?)")), &p[0], 0, 0)) markModified();
					im::SameLine(); im::TextDisabled("(?)");
					if(im::IsItemHovered()) Tooltip("Writes byte 0x557D2B (0/1); meaning unknown");
					if(i18n::Combo(EfRow(LBL("Zoom")), &p[1], "0: (none)\0001: 1.0\0002: 0.84\000")) markModified();
				}

			} else if(no == 18) { // Camera follow slot 0
				if(i18n::Combo(EfRow(LBL("Camera follows P1 slot")), &p[0], "0: Yes\0001: No\000")) markModified();
				im::SameLine(); im::TextDisabled("(?)");
				if(im::IsItemHovered()) Tooltip("Writes g_CameraFollowSlot[0] only (medium confidence)");

			} else if(no == 20) { // Modify guard gauge
				EfSection(TXT("--- Modify Guard Gauge (while guarding) ---"));
				if(im::InputInt(EfRow(LBL("Value 1")), &p[0], 0, 0)) markModified();
				if(im::InputInt(EfRow(LBL("Value 2")), &p[1], 0, 0)) markModified();
				im::SameLine(); im::TextDisabled("(?)");
				if(im::IsItemHovered()) Tooltip("Character_ModifyGuardGaugeClamped(owner, p2, p1)\nOnly while the owner's guard state is 1");
				if(im::Checkbox(EfRow(LBL("Raw values")), (bool*)&p[2])) markModified();
				im::SameLine(); im::TextDisabled("(?)");
				if(im::IsItemHovered()) Tooltip("Off: values are TypeConfig system-value ids");

			} else if(no == 21) { // Camera zoom preset
				if(i18n::Combo(EfRow(LBL("Zoom preset")), &p[0], "0: Zoom 1.0\0001: Zoom 0.84 (snap)\0002: Freeze\000")) markModified();

			} else if(no == 23) { // Rotate toward floor point
				if(im::InputInt(EfRow(LBL("Radius")), &p[0], 0, 0)) markModified();
				im::SameLine(); im::TextDisabled("(?)");
				if(im::IsItemHovered()) Tooltip("Circle of this radius vs the ground line -> rotation (atan2 * 10000)");

			} else if(no == 50) { // Camera zoom override
				EfSection(TXT("--- Camera Zoom Override ---"));
				if(im::InputInt(EfRow("X"), &p[0], 0, 0)) markModified();
				if(im::InputInt(EfRow("Y"), &p[1], 0, 0)) markModified();
				if(im::InputInt(EfRow(LBL("Out frames")), &p[2], 0, 0)) markModified();
				if(im::InputInt(EfRow(LBL("Zoom (/10000)")), &p[3], 0, 0)) markModified();
				im::TextDisabled(TXT("X, Y and Out frames all 0 = override off"));

			} else if(no == 151) { // Set circuit mode, zero meter
				EfSection(TXT("--- Set Circuit Mode (meter = 0) ---"));
				if(i18n::Combo(EfRow(LBL("Circuit state")), &p[0], "0: Normal\0001: HEAT\0002: MAX\0003: BLOOD HEAT\000")) markModified();
				if(im::InputInt(EfRow(LBL("Value")), &p[1], 0, 0)) markModified();
				im::SameLine(); im::TextDisabled("(?)");
				if(im::IsItemHovered()) Tooltip("State 0: meter = value. States 1-3: heat time = value");
				if(im::InputInt(EfRow(LBL("Max duration")), &p[2], 0, 0)) markModified();

			} else if(no == 152) { // BG fade mode
				if(im::InputInt(EfRow(LBL("BG fade state")), &p[0], 0, 0)) markModified();
				im::SameLine(); im::TextDisabled("(?)");
				if(im::IsItemHovered()) Tooltip("1/2 -> BG alpha 0, 3 -> 255");

			} else if(no == 153) { // Z priority front
				im::Text(TXT("Z priority (+0x160) = 452. No parameters"));

			} else if(no == 155) { // Show/hide team partner
				EfSection(TXT("--- Show/Hide Team Partner ---"));
				if(i18n::Combo(EfRow(LBL("Team")), &p[0], "0: Own team\0001: Enemy team\000")) markModified();
				if(i18n::Combo(EfRow(LBL("Action")), &p[1], "0: Hide\0001: Show\0002: Toggle\000")) markModified();

			} else if(no == 200) { // Object tracking mode
				EfSection(TXT("--- Object Tracking Mode ---"));
				const char* const trackModes[] = {
					"0: None",
					"1: Face parent",
					"2: Chase parent",
					"3: Chase parent (variant)",
					"10: Attach to parent sprite point",
				};
				if(EfComboManual("Mode", &p[0], trackModes, IM_ARRAYSIZE(trackModes), width*2, width)) markModified();
				if(im::InputScalarN("Params p2..p6", ImGuiDataType_S32, p+1, 5, NULL, NULL, "%d", 0)) markModified();
				im::SameLine(); im::TextDisabled("(?)");
				if(im::IsItemHovered()) Tooltip("Stored in words +0x36..+0x3E; per-mode meaning not mapped (?)\nEF30 No 1 edits the same words");

			} else if(no == 1100) { // Reverse velocity
				if(i18n::Combo(EfRow(LBL("Axis")), &p[0], "0: X\0001: Y\000")) markModified();
				im::SameLine(); im::TextDisabled("(?)");
				if(im::IsItemHovered()) Tooltip("Negates velocity and acceleration on that axis");

	} else {
		// Generic parameters
		im::Text(TXT("Parameters:"));
		if(im::InputScalarN("##params", ImGuiDataType_S32, p, 6, NULL, NULL, "%d", 0)) {
			markModified();
		}
		if(im::InputScalarN("##params2", ImGuiDataType_S32, p+6, 6, NULL, NULL, "%d", 0)) {
			markModified();
		}
	}
}
#endif /* EFFECT_MISC_H_GUARD */
