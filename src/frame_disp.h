#include <imgui.h>
#include "imgui_utils.h"
#include "framedata.h"
#include "cg.h"

namespace im = ImGui;

// Hit vector list - from MBAACC Hit Vectors CSV
// TODO: Translate Japanese names for vectors 80-99 (currently showing JP text)
static const char* const hitVectorList[] = {
	"0: Head damage (weak)",
	"1: Head damage (medium)",
	"2: Head damage (strong)",
	"3: Gut damage (weak)",
	"4: Gut damage (medium)",
	"5: Gut damage (strong)",
	"6: Sweep",
	"7: Down",
	"8: Airborne damage",
	"9: Sent flying horizontal",
	"10: Sent flying upwards",
	"11: Sent flying up and away",
	"12: Spiked straight down",
	"13: Spiked down and away",
	"14: Stagger (unused?)",
	"15: Sent flying very far upwards",
	"16: Guard (unused?)",
	"17: No hitstun",
	"30: Standing crush (unused?)",
	"31: Crouching crush (unused?)",
	"32: Airborne crush (unused?)",
	"40: Floating guard? (unused?)",
	"50: Crouching damage (Weak)",
	"51: Crouching damage (Medium)",
	"52: Crouching damage (Strong)",
	"54: Jumping attack gut damage",
	"55: Jumping attack standing damage",
	"56: Jumping attack crouching damage",
	"57: Airborne down",
	"58: Soft launch",
	"59: Soft launch 2",
	"60: Air guard",
	"61: Spiked straight down (explode)",
	"62: Spiked down and away (bounce)",
	"63: Spike (bounce)",
	"64: Sent flying up 2",
	"65: Head damage (strong) 2",
	"66: Crouching damage (strong) 2",
	"67: Down 2",
	"68: Floating low down",
	"69: 7th HS sent flying horizontal",
	"70: Sent flying up (low grav)",
	"71: Home run",
	"72: Sliding down",
	"73: In-place head damage",
	"74: In-place gut damage",
	"75: Ryougi AT down",
	"76: Ries j.A head damage (weak)",
	"77: Sent flying up",
	"78: Sent flying up and away",
	"79: Low knockback gut damage",
	"80: 離れにくい屈中",
	"81: 離れにくい腹強",
	"82: 離れにくい屈強",
	"83: さつき用斜め叩きつけバウンド",
	"85: 特大のけぞり立",
	"86: 特大のけぞり屈",
	"87: 頭やられちょい中",
	"88: 屈やられちょい中",
	"90: 地上受け身＿短",
	"91: 地上受け身＿長",
	"92: 地上受け身＿前",
	"93: 空中受け身＿前",
	"94: 空中受け身＿中",
	"95: 空中受け身＿後",
	"98: ダウン追い討ち",
	"99: KOダウン",
	"152: 屈やられ中＋１",
	"153: 腹やられ中＋１",
	"161: 叩きつけ小バウンド",
	"162: 斜め叩きつけ小バウンド",
	"163: 叩きつけ中バウンド",
	"164: 斜め叩きつけ中バウンド",
	"171: 叩きつけ小バウンド",
	"172: 斜め叩きつけ小バウンド",
	"173: 叩きつけ中バウンド",
	"174: 斜め叩きつけ中バウンド",
	"200: 空中ガード2",
};

// Helper function for combo with manual entry support
static inline void ShowComboWithManual(const char* label, int* value, const char* const* items, int itemCount, float comboWidth, float defaultWidth = 75.f) {
	if(comboWidth < 0) comboWidth = defaultWidth*2;
	// Find if current value is in the list
	int selectedIndex = -1;
	int parsedValue;
	for(int i = 0; i < itemCount; i++) {
		// Parse "N: Description" format
		if(sscanf(items[i], "%d:", &parsedValue) == 1) {
			if(parsedValue == *value) {
				selectedIndex = i;
				break;
			}
		}
	}

	// Determine preview text
	const char* preview;
	char customBuffer[64];
	if(selectedIndex >= 0) {
		preview = items[selectedIndex];
	} else {
		snprintf(customBuffer, sizeof(customBuffer), "Custom: %d", *value);
		preview = customBuffer;
	}

	im::SetNextItemWidth(comboWidth);
	if(im::BeginCombo(label, preview)) {
		// Show all predefined items
		for(int i = 0; i < itemCount; i++) {
			bool selected = (i == selectedIndex);
			if(im::Selectable(items[i], selected)) {
				// Parse and set value
				if(sscanf(items[i], "%d:", &parsedValue) == 1) {
					*value = parsedValue;
				}
			}
			if(selected)
				im::SetItemDefaultFocus();
		}

		// Add manual entry option
		im::Separator();
		im::SetNextItemWidth(defaultWidth);
		im::InputInt("Custom value", value, 0, 0);

		im::EndCombo();
	}

	// Show visual indicator for custom values
	if(selectedIndex < 0) {
		im::SameLine();
		im::TextColored(ImVec4(1.0f, 0.7f, 0.0f, 1.0f), "⚠");
		if(im::IsItemHovered()) {
			Tooltip("Using undocumented value");
		}
	}
}

inline void HitVectorDisplay()
{
	if (im::BeginTable("HitVectors", 17,
		ImGuiTableFlags_RowBg |
		ImGuiTableFlags_SizingFixedFit))
	{
		im::TableSetupColumn("Description");
		im::TableSetupColumn("VecCnt");
		im::TableSetupColumn("UkemiTime");
		im::TableSetupColumn("Prio");
		im::TableSetupColumn("PrioAni");
		im::TableSetupColumn("KoCheck");
		im::TableSetupColumn("VecNum");
		im::TableSetupColumn("HitAni");
		im::TableSetupColumn("GuardAni");
		im::TableSetupColumn("Time");
		im::TableSetupColumn("VecTime");
		im::TableSetupColumn("flag");
		im::TableSetupColumn("VecNum");
		im::TableSetupColumn("HitAni");
		im::TableSetupColumn("GuardAni");
		im::TableSetupColumn("Time");
		im::TableSetupColumn("VecTime");
		im::TableHeadersRow();
		im::EndTable();
	}
}

inline void IfDisplay(std::vector<Frame_IF> *ifList_, FrameData *frameData = nullptr)
{
	std::vector<Frame_IF> & ifList = *ifList_;
	constexpr float width = 75.f;

	// Helper lambda to show pattern/frame jumps with names
	auto ShowJumpField = [&](const char* label, int* value, const char* tooltip = nullptr) {
		im::SetNextItemWidth(width);
		im::InputInt(label, value, 0, 0);
		if(frameData && *value >= 10000) {
			int patternNum = *value - 10000;
			if(patternNum >= 0 && patternNum < frameData->get_sequence_count()) {
				im::SameLine(); im::TextDisabled("[%s]", frameData->GetDecoratedName(patternNum).c_str());
			}
		}
		if(tooltip) {
			im::SameLine(); im::TextDisabled("(?)");
			if(im::IsItemHovered()) Tooltip(tooltip);
		}
	};

	// Helper lambda to show command IDs with names
	auto ShowCommandField = [&](const char* label, int* value, const char* tooltip = nullptr) {
		im::SetNextItemWidth(width);
		im::InputInt(label, value, 0, 0);
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

	// Condition type names
	const char* const conditionTypes[] = {
		"0: (None)",
		"1: Jump on directional input",
		"2: Effect despawn conditions",
		"3: Branch on hit (all hits used)",
		"4: Vector check",
		"5: KO flag check",
		"6: Lever & Trigger check (Frame)",
		"7: Lever & Trigger check (Pattern)",
		"8: Random check",
		"9: Loop counter settings",
		"10: Loop counter check",
		"11: Additional command input check",
		"12: Opponent distance check",
		"13: Screen corner check",
		"14: Box collision check",
		"15: Box collision check (Capture)",
		"16: Be affected by scrolling",
		"17: Branch on number of hits",
		"18: Check main/trunk animation",
		"19: Projectile box col check",
		"20: Box collision check 2",
		"21: Opponent's character check",
		"22: BG number check",
		"23: BG type check",
		"24: Projectile variable check",
		"25: Variable comparison",
		"26: Check lever and change vector",
		"27: Branch when parent gets hurt",
		"28: Jump if knocked out",
		"29: Check X pos on screen",
		"30: Facing direction check",
		"31: Change variable on command input",
		"32: Jump if CPU side of CPU battle",
		"33: If sound effect is playing",
		"34: Homing",
		"35: Custom cancel command check",
		"36: Meter bar mode check",
		"37: Jump according to color",
		"38: Change variable on hit",
		"39: (Unknown)",
		"40: Jump after N frames",
		"41: Jump if controlled char mismatch",
		"42: (Unknown)",
		"43: (Unknown)",
		"44: (Unknown)",
		"45: (Unknown)",
		"46: (Unknown)",
		"47: (Unknown)",
		"48: (Unknown)",
		"49: (Unknown)",
		"50: Effect reflection box",
		"51: Check Shield Conditions",
		"52: Throw check",
		"53: (Unknown)",
		"54: Jump on hit or block",
		"55: (Unknown)",
		"56: (Unknown)",
		"57: (Unknown)",
		"58: (Unknown)",
		"59: (Unknown)",
		"60: (Unknown)",
		"61: (Unknown)",
		"62: (Unknown)",
		"63: (Unknown)",
		"64: (Unknown)",
		"65: (Unknown)",
		"66: (Unknown)",
		"67: (Unknown)",
		"68: (Unknown)",
		"69: (Unknown)",
		"70: Jump on reaching screen border",
		"71: (Unknown)",
		"72: (Unknown)",
		"73: (Unknown)",
		"74: (Unknown)",
		"75: (Unknown)",
		"76: (Unknown)",
		"77: (Unknown)",
		"78: (Unknown)",
		"79: (Unknown)",
		"80: (Unknown)",
		"81: (Unknown)",
		"82: (Unknown)",
		"83: (Unknown)",
		"84: (Unknown)",
		"85: (Unknown)",
		"86: (Unknown)",
		"87: (Unknown)",
		"88: (Unknown)",
		"89: (Unknown)",
		"90: (Unknown)",
		"91: (Unknown)",
		"92: (Unknown)",
		"93: (Unknown)",
		"94: (Unknown)",
		"95: (Unknown)",
		"96: (Unknown)",
		"97: (Unknown)",
		"98: (Unknown)",
		"99: (Unknown)",
		"100: Box collision with partner",
		"101: (Unknown)",
		"102: (Unknown)",
		"103: (Unknown)",
		"104: (Unknown)",
		"105: (Unknown)",
		"106: (Unknown)",
		"107: (Unknown)",
		"108: (Unknown)",
		"109: (Unknown)",
		"110: (Unknown)",
		"111: (Unknown)",
		"112: (Unknown)",
		"113: (Unknown)",
		"114: (Unknown)",
		"115: (Unknown)",
		"116: (Unknown)",
		"117: (Unknown)",
		"118: (Unknown)",
		"119: (Unknown)",
		"120: (Unknown)",
		"121: (Unknown)",
		"122: (Unknown)",
		"123: (Unknown)",
		"124: (Unknown)",
		"125: (Unknown)",
		"126: (Unknown)",
		"127: (Unknown)",
		"128: (Unknown)",
		"129: (Unknown)",
		"130: (Unknown)",
		"131: (Unknown)",
		"132: (Unknown)",
		"133: (Unknown)",
		"134: (Unknown)",
		"135: (Unknown)",
		"136: (Unknown)",
		"137: (Unknown)",
		"138: (Unknown)",
		"139: (Unknown)",
		"140: (Unknown)",
		"141: (Unknown)",
		"142: (Unknown)",
		"143: (Unknown)",
		"144: (Unknown)",
		"145: (Unknown)",
		"146: (Unknown)",
		"147: (Unknown)",
		"148: (Unknown)",
		"149: (Unknown)",
		"150: (Unknown)",
		"151: (Unknown)",
	};

	const char* const characterList[] = {
		"0: Sion", "1: Arc", "2: Ciel", "3: Akiha", "4: Maids",
		"5: Hisui", "6: Kohaku", "7: Tohno", "8: Miyako", "9: Wara",
		"10: Nero", "11: V.Sion", "12: Warc", "13: V.Akiha", "14: Mech",
		"15: Nanaya", "16: G.Akiha", "17: Satsuki", "18: Len", "19: P.Ciel",
		"20: Neco", "21: (None)", "22: Aoko", "23: W.Len", "24: (None)",
		"25: NAC", "26: (None)", "27: G.Chaos", "28: Kouma", "29: Sei",
		"30: Ries", "31: Roa", "32: Hermes", "33: Ryougi", "34: NecoMech",
		"35: KohaMech", "36: (None)", "37: (None)", "38: (None)", "39: (None)",
		"40: (None)", "41: (None)", "42: (None)", "43: (None)", "44: (None)",
		"45: (None)", "46: (None)", "47: (None)", "48: (None)", "49: (None)",
		"50: (None)", "51: B.Hime", "52: (None)", "53: (None)", "54: (None)",
		"55: (None)", "56: (None)", "57: (None)", "58: B.Miyako", "59: B.Wara",
		"60: (None)", "61: (None)", "62: (None)", "63: (None)", "64: (None)",
		"65: (None)", "66: (None)", "67: (None)", "68: (None)", "69: (None)",
		"70: (None)", "71: (None)", "72: B.Aoko", "73: B.W.Len", "74: (None)",
		"75: (None)", "76: (None)", "77: (None)", "78: (None)", "79: (None)",
		"80: (None)", "81: (None)", "82: (None)", "83: (None)", "84: (None)",
		"85: B.KohaMech",
	};

	const char* const hitConditions[] = {
		"0: On hit",
		"1: On hit or block",
		"2: On hit, clash, or shield",
		"3: On hit, block, clash, or shield",
		"5: On block",
		"6: On clash or shield",
		"7: On block, clash, or shield",
		"100: On shield only",
	};

	const char* const opponentStateList[] = {
		"0: Grounded or airborne",
		"1: Only grounded",
		"2: Only airborne",
	};

	const char* const comparisonTypes[] = {
		"0: Greater than",
		"1: Less than",
		"2: Equal to",
	};

	int deleteI = -1;
	static std::vector<int> manualEditMode; // Track manual edit mode per condition (int instead of bool for ImGui)
	if(manualEditMode.size() != ifList.size()) {
		manualEditMode.resize(ifList.size(), 0);
	}

	for( int i = 0; i < ifList.size(); i++)
	{
		if(i>0)
			im::Separator();
		im::PushID(i);

		// Type dropdown
		int typeIndex = ifList[i].type;
		if(typeIndex < 0) typeIndex = 0;
		if(typeIndex >= IM_ARRAYSIZE(conditionTypes)) {
			// Handle special cases (50+, 100+, 150+)
			im::SetNextItemWidth(width*2);
			im::InputInt("Type", &ifList[i].type, 0, 0);
		} else {
			im::SetNextItemWidth(width*3);
			if(im::Combo("Type", &typeIndex, conditionTypes, IM_ARRAYSIZE(conditionTypes))) {
				ifList[i].type = typeIndex;
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
			im::InputScalarN("##params", ImGuiDataType_S32, p, 6, NULL, NULL, "%d", 0);
			im::InputScalarN("##params2", ImGuiDataType_S32, p+6, 3, NULL, NULL, "%d", 0);
		} else {
		// Otherwise show smart UI based on type
		switch(ifList[i].type) {
			case 1: // Jump on directional input
				im::SetNextItemWidth(width);
				im::InputInt("Direction (numpad)", &p[0], 0, 0); im::SameLine();
				im::TextDisabled("(?)"); if(im::IsItemHovered()) Tooltip("0=neutral, 2=down, 4=left, 6=right, 8=up, etc.\n10=both 6 and 3");

				ShowJumpField("Jump to", &p[1], "Frame number, or add 10000 for pattern");

				im::SetNextItemWidth(width);
				im::Checkbox("Negate condition", (bool*)&p[2]);
				break;

			case 2: // Effect despawn conditions
			{
				unsigned int flagIdx = -1;
				BitField("Despawn flags", (unsigned int*)&p[0], &flagIdx, 2);
				switch(flagIdx) {
					case 0: Tooltip("Despawn outside camera X bounds"); break;
					case 1: Tooltip("Despawn outside camera Y bounds"); break;
				}

				im::SetNextItemWidth(width);
				im::Checkbox("Despawn on landing", (bool*)&p[1]);

				im::SetNextItemWidth(width);
				im::Checkbox("Despawn on pattern transition", (bool*)&p[2]);

				im::SetNextItemWidth(width);
				im::InputInt("Projectile var decrease", &p[3], 0, 0);
				break;
			}

			case 4: // Vector check
				ShowJumpField("Frame to jump to", &p[0], nullptr);

				im::SetNextItemWidth(width);
				im::Combo("X velocity", &p[1], "0: No check\0001: Backwards (negative)\0002: Forwards (positive)\0");

				im::SetNextItemWidth(width);
				im::Combo("Y velocity", &p[2], "0: No check\0001: Down (positive)\0002: Up (negative)\0");
				break;

			case 8: // Random check
				ShowJumpField("Frame to jump to", &p[0], nullptr);

				im::SetNextItemWidth(width);
				im::InputInt("Chance", &p[1], 0, 0); im::SameLine();
				im::TextDisabled("(?)"); if(im::IsItemHovered()) Tooltip("Max 512");

				im::SetNextItemWidth(width);
				im::InputInt("Jump pattern?", &p[2], 0, 0);

				im::SetNextItemWidth(width);
				im::InputInt("Random choose N adjacent?", &p[3], 0, 0);
				break;

			case 3: // Branch on hit
				ShowJumpField("Jump to", &p[0], "Frame number, or add 10000 for pattern");

				ShowComboWithManual("Hit condition", &p[1], hitConditions, IM_ARRAYSIZE(hitConditions), width*2, width);

				im::SetNextItemWidth(width*2);
				im::Combo("Opponent state", &p[2], opponentStateList, IM_ARRAYSIZE(opponentStateList));

				im::SetNextItemWidth(width);
				im::InputInt("Projectile var decrease", &p[3], 0, 0);
				break;

			case 6: // Lever & Trigger check (Frame)
			case 7: // Lever & Trigger check (Pattern)
				im::SetNextItemWidth(width);
				im::InputInt("Lever direction", &p[0], 0, 0); im::SameLine();
				im::TextDisabled("(?)"); if(im::IsItemHovered()) Tooltip("Numpad notation\n0=5(neutral), 5=nothing, 10=6/4, 13=1/2/3, 255=any");

				{
					unsigned int flagIdx = -1;
					BitField("Buttons", (unsigned int*)&p[1], &flagIdx, 8);
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
				im::InputInt(ifList[i].type == 6 ? "Frame to jump to" : "Pattern to jump to", &p[2], 0, 0);

				{
					unsigned int flagIdx = -1;
					BitField("Flags", (unsigned int*)&p[3], &flagIdx, 8);
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
				ShowComboWithManual("When", &p[1], cond11When, IM_ARRAYSIZE(cond11When), width*2, width);

				im::SetNextItemWidth(width);
				im::InputInt("State to transition to", &p[2], 0, 0);

				const char* const cond11State[] = {
					"0: Always",
					"1: Grounded hit (according to When)",
					"2: Air hit (according to When)",
				};
				im::SetNextItemWidth(width*2);
				ShowComboWithManual("Opponent state", &p[3], cond11State, IM_ARRAYSIZE(cond11State), width*2, width);

				im::SetNextItemWidth(width);
				im::InputInt("Priority", &p[8], 0, 0); im::SameLine();
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
							if(im::Selectable(cancelConditions[h], selected))
								p[0] = atoi(cancelConditions[h]);
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
				im::InputInt("Priority", &p[8], 0, 0); im::SameLine();
				im::TextDisabled("(?)"); if(im::IsItemHovered()) Tooltip("0-5000: priority = 5000 - value\n10000+: priority = value - 10000");
				break;
			}

			case 13: // Screen corner check
				ShowJumpField("Jump to", &p[0], "Frame number, or add 10000 for pattern");
				im::SetNextItemWidth(width);
				im::InputInt("Param2", &p[1], 0, 0);
				im::SetNextItemWidth(width);
				im::InputInt("Param3", &p[2], 0, 0);
				break;

			case 14: // Box collision check
			{
				ShowJumpField("Jump to", &p[0], "Frame number, or add 10000 for pattern");

				const char* const cond14Target[] = {
					"0: Enemies only",
					"1: Allies only",
					"2: Both",
				};
				im::SetNextItemWidth(width*2);
				ShowComboWithManual("Check target", &p[1], cond14Target, IM_ARRAYSIZE(cond14Target), width*2, width);
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
				ShowComboWithManual("Box type", &p[2], cond14BoxType, IM_ARRAYSIZE(cond14BoxType), width*2, width);

				im::SetNextItemWidth(width);
				im::InputInt("Hitstop on collision", &p[3], 0, 0);

				const char* const cond14Turn[] = {
					"0: No turnaround",
					"2: Turn towards collider",
					"4: Turn away from collider",
				};
				im::SetNextItemWidth(width*2);
				ShowComboWithManual("Turn direction", &p[4], cond14Turn, IM_ARRAYSIZE(cond14Turn), width*2, width);
				break;
			}

			case 16: // Be affected by scrolling
				im::SetNextItemWidth(width);
				im::Checkbox("By X", (bool*)&p[0]);

				im::SetNextItemWidth(width);
				im::Checkbox("By Y", (bool*)&p[1]);
				break;

			case 17: // Branch according to number of hits
				ShowJumpField("Frame to jump to", &p[0], nullptr);

				im::SetNextItemWidth(width);
				im::InputInt("Number of hits", &p[1], 0, 0);
				break;

			case 24: // Projectile variable check
				ShowJumpField("Frame to jump to", &p[0], nullptr);

				im::SetNextItemWidth(width);
				im::InputInt("Variable ID and Value", &p[1], 0, 0); im::SameLine();
				im::TextDisabled("(?)"); if(im::IsItemHovered()) Tooltip("1s place: Value\n10s place: Variable ID");
				break;

			case 21: // Opponent's character check
				ShowJumpField("Jump to", &p[0], "Frame number, or add 10000 for pattern");

				ShowComboWithManual("Character", &p[1], characterList, IM_ARRAYSIZE(characterList), width*2, width);
				im::SameLine(); im::TextDisabled("(?)");
				if(im::IsItemHovered()) Tooltip("Add 50 for boss version");
				break;

			case 25: // Variable comparison
				ShowJumpField("Jump to", &p[0], "Frame number, or add 10000 for pattern");

				im::SetNextItemWidth(width);
				im::InputInt("Variable ID", &p[1], 0, 0);

				im::SetNextItemWidth(width);
				im::InputInt("Compare value", &p[2], 0, 0);

				im::SetNextItemWidth(width*2);
				im::Combo("Comparison", &p[3], comparisonTypes, IM_ARRAYSIZE(comparisonTypes));
				break;

			case 26: // Check lever and change vector
				im::SetNextItemWidth(width);
				im::InputInt("Direction", &p[0], 0, 0); im::SameLine();
				im::TextDisabled("(?)"); if(im::IsItemHovered()) Tooltip("0-9: Numpad direction\n10: Only 6/4 (x only)\n11: 4,6,7,8,9 (x+y)");

				im::SetNextItemWidth(width);
				im::InputInt("X Speed per frame", &p[1], 0, 0);

				im::SetNextItemWidth(width);
				im::InputInt("Y Speed per frame", &p[2], 0, 0);

				im::SetNextItemWidth(width);
				im::InputInt("Max speed", &p[3], 0, 0);
				break;

			case 27: // Branch when parent gets hurt
			{
				ShowJumpField("Frame/Pattern to jump to", &p[0], "Frame number, or add 10000 for pattern");

				im::SetNextItemWidth(width);
				im::Combo("Jump type", &p[1], "Pattern\0Frame\0");

				unsigned int flagIdx = -1;
				BitField("Trigger flags", (unsigned int*)&p[2], &flagIdx, 3);
				switch(flagIdx) {
					case 0: Tooltip("When getting thrown"); break;
					case 1: Tooltip("When getting hurt"); break;
					case 2: Tooltip("When blocking"); break;
				}
				break;
			}

			case 31: // Change variable on command input
				ShowCommandField("Move ID", &p[0], "Command ID from _c.txt");

				im::SetNextItemWidth(width);
				im::InputInt("Variable ID", &p[1], 0, 0);

				im::SetNextItemWidth(width);
				im::InputInt("Value", &p[2], 0, 0);
				break;

			case 30: // Facing direction check
				im::SetNextItemWidth(width);
				im::InputInt("Frame to jump to", &p[0], 0, 0);

				im::SetNextItemWidth(width);
				im::Combo("Check facing", &p[1], "Right\0Left\0");
				break;

			case 33: // If sound effect is playing
				im::Text("Parameters unknown - see docs");
				im::InputScalarN("##params", ImGuiDataType_S32, p, 6, NULL, NULL, "%d", 0);
				break;

			case 34: // Homing
				im::SetNextItemWidth(width);
				im::Combo("Tracking type", &p[8], "Accelerated\0Instant\0");
				im::Text("Other params: See Hime/CMech patterns");
				break;

			case 37: // Jump according to color selected
				ShowJumpField("Pattern to jump to", &p[0], "Add 10000 for pattern");
				im::Text("(Legacy Re-ACT feature, unused)");
				break;

			case 38: // Change variable on hit
			{
				im::SetNextItemWidth(width);
				im::InputInt("Value", &p[0], 0, 0);

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
				ShowComboWithManual("When", &p[1], cond38When, IM_ARRAYSIZE(cond38When), width*2, width);

				im::SetNextItemWidth(width*2);
				im::Combo("Opponent state", &p[2], opponentStateList, IM_ARRAYSIZE(opponentStateList));

				im::SetNextItemWidth(width);
				im::InputInt("Extra variable ID", &p[3], 0, 0);

				const char* const cond38Op[] = {
					"0: Set",
					"1: Add",
					"10: Set owner var",
					"11: Add owner var",
				};
				im::SetNextItemWidth(width*2);
				ShowComboWithManual("Operation", &p[4], cond38Op, IM_ARRAYSIZE(cond38Op), width*2, width);
				break;
			}

			case 40: // Jump after N frames
				im::SetNextItemWidth(width);
				im::InputInt("Frame to jump to", &p[0], 0, 0);

				im::SetNextItemWidth(width);
				im::InputInt("Number of frames", &p[1], 0, 0);
				break;

			case 51: // Check Shield Conditions
				im::SetNextItemWidth(width);
				im::InputInt("Frame on D release", &p[0], 0, 0);

				im::SetNextItemWidth(width);
				im::InputInt("Frame on shield success", &p[1], 0, 0);

				im::SetNextItemWidth(width);
				im::InputInt("Pattern on EX shield", &p[2], 0, 0);
				break;

			case 52: // Throw check
				im::SetNextItemWidth(width);
				im::InputInt("Frame on throw success", &p[0], 0, 0);

				im::SetNextItemWidth(width);
				im::Checkbox("Air throw", (bool*)&p[1]);

				im::SetNextItemWidth(width);
				im::InputInt("Frame if combo air throw", &p[2], 0, 0);
				break;

			case 54: // Jump on hit or block
				ShowJumpField("Frame to jump to", &p[0], nullptr);
				break;

			case 70: // Jump on reaching screen border
				im::SetNextItemWidth(width);
				im::InputInt("Frame at top", &p[0], 0, 0); im::SameLine();
				im::TextDisabled("(?)"); if(im::IsItemHovered()) Tooltip("-1 for no jump");

				im::SetNextItemWidth(width);
				im::InputInt("Frame at bottom", &p[1], 0, 0); im::SameLine();
				im::TextDisabled("(?)"); if(im::IsItemHovered()) Tooltip("-1 for no jump");

				im::SetNextItemWidth(width);
				im::InputInt("Frame at left", &p[2], 0, 0); im::SameLine();
				im::TextDisabled("(?)"); if(im::IsItemHovered()) Tooltip("-1 for no jump");

				im::SetNextItemWidth(width);
				im::InputInt("Frame at right", &p[3], 0, 0); im::SameLine();
				im::TextDisabled("(?)"); if(im::IsItemHovered()) Tooltip("-1 for no jump");
				break;

			case 5: // KO flag check
				im::Text("Automatic check - no parameters needed");
				im::TextDisabled("Triggers when opponent is KO'd");
				break;

			case 9: // Loop counter settings
				im::Text("Unused feature - parameters unknown");
				im::TextDisabled("Enable Manual mode to edit raw values if needed");
				break;

			case 10: // Loop counter check
				im::Text("Unused feature - parameters unknown");
				im::TextDisabled("Enable Manual mode to edit raw values if needed");
				break;

			case 12: // Opponent distance check
				im::Text("Not yet documented - parameters unknown");
				im::TextDisabled("Enable Manual mode to experiment with values");
				break;

			case 15: // Box collision check (Capture)
				im::Text("Not yet documented - parameters unknown");
				im::TextDisabled("See condition 14 for similar functionality");
				im::TextDisabled("Enable Manual mode to experiment with values");
				break;

			case 18: // Check main/trunk animation
				im::Text("Not yet documented - parameters unknown");
				im::TextDisabled("Enable Manual mode to experiment with values");
				break;

			case 19: // Projectile box collision check (Reflection)
				im::Text("Not yet documented - parameters unknown");
				im::TextDisabled("Enable Manual mode to experiment with values");
				break;

			case 20: // Box collision check 2
				im::Text("Not yet documented - parameters unknown");
				im::TextDisabled("See condition 14 for similar functionality");
				im::TextDisabled("Enable Manual mode to experiment with values");
				break;

			case 22: // BG number check
				im::Text("Unused feature - parameters unknown");
				im::TextDisabled("Enable Manual mode to edit raw values if needed");
				break;

			case 23: // BG type check
				im::Text("Unused feature - parameters unknown");
				im::TextDisabled("Enable Manual mode to edit raw values if needed");
				break;

			case 28: // Jump if knocked out
				im::Text("Automatic check - no parameters needed");
				im::TextDisabled("Triggers when this character is KO'd");
				break;

			case 29: // Check X pos on screen
				im::Text("Unused feature - parameters unknown");
				im::TextDisabled("Enable Manual mode to edit raw values if needed");
				break;

			case 32: // Jump if CPU side of CPU battle
				im::Text("Arcade mode feature (used in Hime's intro)");
				im::SetNextItemWidth(width);
				im::InputInt("Param1", &p[0], 0, 0); im::SameLine();
				im::TextDisabled("(?)"); if(im::IsItemHovered()) Tooltip("Purpose unknown");
				break;

			case 36: // Meter bar mode check
				im::Text("Automatic check - no parameters needed");
				im::TextDisabled("Checks current meter/moon mode");
				break;

			case 39: // Unknown
			case 41: // Jump if controlled char mismatch
			case 42: // Unknown
			case 53: // Unknown
			case 55: // Unknown
			case 60: // Unknown
				im::Text("Not yet documented - parameters unknown");
				im::TextDisabled("Enable Manual mode to experiment with values");
				break;

			case 50: // Effect reflection box
				im::Text("Unused feature - parameters unknown");
				im::TextDisabled("Enable Manual mode to edit raw values if needed");
				break;

			case 100: // Box collision with partner
				im::Text("Partner-specific collision check");
				im::TextDisabled("Not yet documented - Enable Manual mode");
				break;

			case 150: // Unknown
			case 151: // Unknown
				im::Text("Not yet documented - parameters unknown");
				im::TextDisabled("Enable Manual mode to experiment with values");
				break;

			default:
				// Generic parameter display for unknown/unimplemented types
				im::Text("Parameters:");
				im::InputScalarN("##params", ImGuiDataType_S32, p, 6, NULL, NULL, "%d", 0);
				im::InputScalarN("##params2", ImGuiDataType_S32, p+6, 3, NULL, NULL, "%d", 0);
				break;
		}
		} // End of manual mode else block

		im::PopID();
	}

	if(deleteI >= 0) {
		ifList.erase(ifList.begin() + deleteI);
		manualEditMode.erase(manualEditMode.begin() + deleteI);
	}

	if(im::Button("Add")) {
		ifList.push_back({});
		manualEditMode.push_back(0);
	}
}

inline void EfDisplay(std::vector<Frame_EF> *efList_)
{
	std::vector<Frame_EF> & efList = *efList_;
	constexpr float width = 50.f;
	int deleteI = -1;
	for( int i = 0; i < efList.size(); i++)
	{
		if(i>0)
			im::Separator();
		im::PushID(i); 

		im::SetNextItemWidth(width); 
		im::InputInt("Type", &efList[i].type, 0, 0); im::SameLine(0.f, 30);
		im::SetNextItemWidth(width); 
		im::InputInt("Number", &efList[i].number, 0, 0); im::SameLine(0.f, 30);

		im::SetNextItemWidth(width);
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1,0,0,0.4));
		if(im::Button("Delete"))
		{
			deleteI = i;
		}
		ImGui::PopStyleColor();
		
		im::InputScalarN("##params", ImGuiDataType_S32, efList[i].parameters, 6, NULL, NULL, "%d", 0);
		im::InputScalarN("##params2", ImGuiDataType_S32, efList[i].parameters+6, 6, NULL, NULL, "%d", 0);

		im::PopID();
	};

	if(deleteI >= 0)
		efList.erase(efList.begin() + deleteI);
	
	if(im::Button("Add"))
		efList.push_back({});
}

inline void AsDisplay(Frame_AS *as)
{
	const char* const stateList[] = {
		"Standing",
		"Airborne",
		"Crouching"
	};

	const char* const cancelList[] = {
		"Never",
		"On hit",
		"Always",
		"On successful hit"
	};

	const char* const counterList[] = {
		"No change",
		"High counter",
		"Low counter",
		"Clear"
	};

	const char* const invulList[] = {
		"None",
		"High and mid",
		"Low and mid",
		"All but throw",
		"Throw only"
	};

	constexpr float width = 103.f;

	unsigned int flagIndex = -1;
	BitField("Movement Flags", &as->movementFlags, &flagIndex, 8);
	switch (flagIndex)
	{
		case 0: Tooltip("Set Y"); break;
		case 4: Tooltip("Set X"); break;
		case 1: Tooltip("Add Y"); break;
		case 5: Tooltip("Add X"); break;
	}

	im::SetNextItemWidth(width*2);
	im::InputInt2("Speed", as->speed); im::SameLine(0.f, 20); im::SetNextItemWidth(width);
	im::InputInt("Max X speed", &as->maxSpeedX, 0, 0);
	im::SetNextItemWidth(width*2);
	im::InputInt2("Accel", as->accel);
	
	im::Separator();
	flagIndex = -1;
	BitField("Flagset 1", &as->statusFlags[0], &flagIndex);
	switch (flagIndex)
	{
		case 0: Tooltip("Vector influences other animations (dash momentum)"); break;
		case 1: Tooltip("Force clean vector (kill dash momentum)"); break;
		case 2: Tooltip("Don't transition to walking"); break;
		case 4: Tooltip("Can ground tech"); break;
		case 5: Tooltip("Unknown"); break;
		case 8: Tooltip("Guard point high and mid"); break;
		case 9: Tooltip("Guard point air blockable"); break;
		case 12: Tooltip("Unknown"); break;
		case 31: Tooltip("Vector initialization only at the beginning (?)"); break;
	}
	
	flagIndex = -1;
	BitField("Flagset 2", &as->statusFlags[1], &flagIndex);
	switch (flagIndex)
	{
		case 0: Tooltip("Can always EX cancel"); break;
		case 2: Tooltip("Can only jump cancel"); break;
		case 31: Tooltip("Can't block"); break;
	}

	im::SetNextItemWidth(width);
	im::InputInt("Number of hits", &as->hitsNumber, 0, 0); im::SameLine(0,20.f);
	im::Checkbox("Player can move", &as->canMove); //
	im::Combo("State", &as->stanceState, stateList, IM_ARRAYSIZE(stateList));
	im::Combo("Invincibility", &as->invincibility, invulList, IM_ARRAYSIZE(invulList));
	im::Combo("Counterhit", &as->counterType, counterList, IM_ARRAYSIZE(counterList)); 
	im::Combo("Cancel normal", &as->cancelNormal, cancelList, IM_ARRAYSIZE(cancelList));
	im::Combo("Cancel special", &as->cancelSpecial, cancelList, IM_ARRAYSIZE(cancelList));
	

	im::Separator();
	flagIndex = -1;
	BitField("Sine flags", &as->sineFlags, &flagIndex, 8);
	switch (flagIndex)
	{
		case 0: Tooltip("Use Y"); break;
		case 4: Tooltip("Use X"); break;
	}
	im::InputInt4("Sinewave", as->sineParameters); im::SameLine();
	im::TextDisabled("(?)");
	if(im::IsItemHovered())
		Tooltip("Sine parameters:\nX dist, Y dist\nX frequency, Y frequency");
	im::InputFloat2("Phases", as->sinePhases);
}

inline void AtDisplay(Frame_AT *at)
{
	const char* const hitEffectList[] = {
		"Weak punch",
		"Medium punch",
		"Strong punch",
		"Weak kick",
		"Medium kick",
		"Strong kick",
		"Super punch",
		"Super kick",
		"Slash (sparks)",
		"Burn",
		"Freeze",
		"Shock",
		"Big flash (SE)",
		"Small flash (SE)",
		"None",
		"Strong hit",
		"Double slash",
		"Super slash",
		"Weak cut",
		"Medium cut",
		"Strong cut",
		"Faint wave",
		//Are there more is it OOB?
	};

	const char* const addedEffectList[] = {
		"None",
		"Burn",
		"Freeze",
		"Shock",
		"Confuse"
	};

	const char* const vectorFlags[] = {
		"Default",
		"Reverse",
		"Nullify",
		"Both?"
	};

	const char* const hitStopList[] = {
		"Weak (6f)",
		"Medium (8f)",
		"Strong (10f)",
		"None (0f)",
		"Stronger (15f)",
		"Strongest (29f)",
		"Weakest (3f)"
	};
	
	constexpr float width = 75.f;
	unsigned int flagIndex = -1;

	BitField("Guard Flags", &at->guard_flags, &flagIndex);
	switch (flagIndex)
	{
		case 0: Tooltip("Stand blockable"); break;
		case 1: Tooltip("Air blockable"); break;
		case 2: Tooltip("Crouch blockable"); break;
		case 8: Tooltip("Miss if enemy is standing"); break;
		case 9: Tooltip("Miss if enemy is airborne"); break;
		case 10: Tooltip("Miss if enemy is crouching"); break;
		case 11: Tooltip("Miss if enemy is in hitstun (includes blockstun)"); break;
		case 12: Tooltip("Miss if enemy is in blockstun"); break;
		case 13: Tooltip("Miss if OTG"); break;
		case 14: Tooltip("Hit only in hitstun"); break;
		case 15: Tooltip("Can't hit playable character"); break;
	}

	flagIndex = -1;
	BitField("Hit Flags", &at->otherFlags, &flagIndex);
	switch (flagIndex)
	{
		case 0: Tooltip("Chip health instead of red health"); break;
		case 1: Tooltip("Can't KO"); break;
		case 2: Tooltip("Make enemy unhittable"); break;
		case 3: Tooltip("Can't be clashed with"); break;
		case 4: Tooltip("Auto super jump cancel"); break;
		case 5: Tooltip("Don't increase combo counter"); break;
		case 6: Tooltip("Shake the screen on hit"); break;
		case 7: Tooltip("Not air techable"); break;
		case 8: Tooltip("Not ground techable (HKD)"); break;
		case 9: Tooltip("Friendly fire"); break;
		case 10: Tooltip("No self hitstop"); break;

		case 12: Tooltip("Lock burst"); break;
		case 13: Tooltip("Can't be shielded"); break;
		case 14: Tooltip("Can't critical"); break;
		
		case 16: Tooltip("Use custom blockstop"); break;
		case 17: Tooltip("OTG Relaunch"); break;
		case 18: Tooltip("Can't counterhit"); break;
		case 19: Tooltip("Unknown"); break;
		case 20: Tooltip("Use Type 2 Circuit Break"); break;
		case 21: Tooltip("Unknown"); break;
		case 22: Tooltip("Remove 1f of untech"); break;

		//Unused or don't exist in melty.
		//case 25: Tooltip("No hitstop on multihit?"); break; 
		//case 29: Tooltip("Block enemy blast during Stun?"); break;
	}

	im::SetNextItemWidth(width * 2);
	im::InputInt("Custom Blockstop", &at->blockStopTime, 0,0);

	im::SetNextItemWidth(width*2);
	im::Combo("Hitstop", &at->hitStop, hitStopList, IM_ARRAYSIZE(hitStopList)); im::SameLine(0.f, 20);
	im::SetNextItemWidth(width);
	im::InputInt("Custom##Hitstop", &at->hitStopTime, 0,0);


	im::SetNextItemWidth(width);
	im::InputInt("Untech time", &at->untechTime, 0,0);  im::SameLine(0.f, 20); im::SetNextItemWidth(width);
	im::InputInt("Circuit break time", &at->breakTime, 0,0);

	im::SetNextItemWidth(width);
	im::InputFloat("Extra gravity", &at->extraGravity, 0,0); im::SameLine(0.f, 20);
	im::Checkbox("Hitgrab", &at->hitgrab);
	

	im::SetNextItemWidth(width);
	im::InputInt("Correction %", &at->correction, 0, 0); im::SameLine(0.f, 20); im::SetNextItemWidth(width*2);
	im::Combo("Type##Correction", &at->correction_type, "Normal\0Multiplicative\0Subtractive\0");

	im::SetNextItemWidth(width);
	im::InputInt("VS damage", &at->red_damage, 0, 0); im::SameLine(0.f, 20); im::SetNextItemWidth(width);
	im::InputInt("Damage", &at->damage, 0, 0);

	im::SetNextItemWidth(width);
	im::InputInt("Guard damage", &at->guard_damage, 0, 0); im::SameLine(0.f, 20); im::SetNextItemWidth(width);
	im::InputInt("Meter gain", &at->meter_gain, 0, 0);

	im::Separator();
	auto comboWidth = (im::GetWindowWidth())/4.f;

	// Guard Vector - Stand, Air, Crouch with dropdowns
	im::Text("Guard Vector:");
	const char* const vectorLabels[] = {"Stand", "Air", "Crouch"};
	for(int i = 0; i < 3; i++)
	{
		im::PushID(100+i);
		ShowComboWithManual(vectorLabels[i], &at->guardVector[i], hitVectorList, IM_ARRAYSIZE(hitVectorList), width*6, width);
		im::PopID();
	}

	// Guard Vector Flags
	for(int i = 0; i < 3; i++)
	{
		im::SetNextItemWidth(comboWidth);
		if(i > 0)
			im::SameLine();
		im::PushID(i);
		im::Combo("##GFLAG", &at->gVFlags[i], vectorFlags, IM_ARRAYSIZE(vectorFlags));
		im::PopID();
	}

	im::Separator();

	// Hit Vector - Stand, Air, Crouch with dropdowns
	im::Text("Hit Vector:");
	for(int i = 0; i < 3; i++)
	{
		im::PushID(200+i);
		ShowComboWithManual(vectorLabels[i], &at->hitVector[i], hitVectorList, IM_ARRAYSIZE(hitVectorList), width*6, width);
		im::PopID();
	}

	// Hit Vector Flags
	for(int i = 0; i < 3; i++)
	{
		im::SetNextItemWidth(comboWidth);
		if(i > 0)
			im::SameLine();
		im::PushID(i);
		im::Combo("##HFLAG", &at->hVFlags[i], vectorFlags, IM_ARRAYSIZE(vectorFlags));
		im::PopID();
	}
	im::Separator();
	
	im::SetNextItemWidth(150);
	im::Combo("Hit effect", &at->hitEffect, hitEffectList, IM_ARRAYSIZE(hitEffectList)); im::SameLine(0, 20.f);
	im::SetNextItemWidth(70);
	im::InputInt("ID##Hit effect", &at->hitEffect, 0, 0); 
	
	im::SetNextItemWidth(70);
	im::InputInt("Sound effect", &at->soundEffect, 0, 0); im::SameLine(0, 20.f); im::SetNextItemWidth(120);

	im::Combo("Added effect", &at->addedEffect, addedEffectList, IM_ARRAYSIZE(addedEffectList));



}

inline void AfDisplay(Frame_AF *af)
{
	const char* const interpolationList[] = {
		"None",
		"Linear",
		"Slow->Fast",
		"Fast->Slow",
		"Fast middle",
		"Slow middle", //Never used, but it works.
	};

	const char* const animationList[] = {
		"Go to pattern",
		"Next frame",
		"Go to frame"
	};

	constexpr float width = 50.f;

	im::SetNextItemWidth(width*3);
	im::InputInt("Sprite", &af->spriteId); im::SameLine(0, 20.f);
	im::Checkbox("Use .pat", &af->usePat);

	im::Separator();

	unsigned int flagIndex = -1;
	BitField("Animation flags", &af->aniFlag, &flagIndex, 4);
	switch (flagIndex)
	{
		case 0: Tooltip("Landing frame: Land to pattern"); break;
		case 1: Tooltip("Loop: Decrement loops and go to end if 0"); break;
		case 2: Tooltip("Go to: Use relative offset"); break;
		case 3: Tooltip("End of loop: Use relative offset"); break;
	}

	im::Combo("Animation", &af->aniType, animationList, IM_ARRAYSIZE(animationList));

	im::SetNextItemWidth(width);
	im::InputInt("Go to", &af->jump, 0, 0); im::SameLine(0.f, 20); im::SetNextItemWidth(width);
	im::InputInt("Landing frame", &af->landJump, 0, 0);

	im::SetNextItemWidth(width);
	im::InputInt("Z-Priority", &af->priority, 0, 0); im::SetNextItemWidth(width);
	im::InputInt("Loop N times", &af->loopCount, 0, 0); im::SameLine(0,20); im::SetNextItemWidth(width);
	im::InputInt("End of loop", &af->loopEnd, 0, 0);
	im::InputInt("Duration", &af->duration, 1, 0);

	im::Separator();
	im::Combo("Interpolation", &af->interpolationType, interpolationList, IM_ARRAYSIZE(interpolationList));

	im::SetNextItemWidth(width);
	im::DragInt("X", &af->offset_x);
	im::SameLine();
	im::SetNextItemWidth(width);
	im::DragInt("Y", &af->offset_y);

	int mode = af->blend_mode-1;
	if(mode < 1)
		mode = 0;
	if (im::Combo("Blend Mode", &mode, "Normal\0Additive\0Subtractive\0"))
	{
		af->blend_mode=mode+1;
	}
	im::ColorEdit4("Color", af->rgba);

	im::DragFloat3("Rot XYZ", af->rotation, 0.005); 
	im::DragFloat2("Scale", af->scale, 0.1);
	im::Checkbox("Rotation keeps scale set by EF", &af->AFRT);

}