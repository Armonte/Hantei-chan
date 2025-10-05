#include <imgui.h>
#include "imgui_utils.h"
#include "framedata.h"
#include "cg.h"
#include <functional>

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
static inline bool ShowComboWithManual(const char* label, int* value, const char* const* items, int itemCount, float comboWidth, float defaultWidth = 75.f) {
	bool changed = false;
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
					changed = true;
				}
			}
			if(selected)
				im::SetItemDefaultFocus();
		}

		// Add manual entry option
		im::Separator();
		im::SetNextItemWidth(defaultWidth);
		if(im::InputInt("Custom value", value, 0, 0)) {
			changed = true;
		}

		im::EndCombo();
	}
	return changed;

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

inline void IfDisplay(std::vector<Frame_IF> *ifList_, Frame_IF *singleClipboard = nullptr, FrameData *frameData = nullptr, int patternIndex = -1, std::function<void()> onModified = nullptr)
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

				if(ShowComboWithManual("Hit condition", &p[1], hitConditions, IM_ARRAYSIZE(hitConditions), width*2, width)) {
		markModified();
	}

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
				if(im::InputScalarN("##params", ImGuiDataType_S32, p, 6, NULL, NULL, "%d", 0)) {
		markModified();
	}
				im::SameLine();
				if(singleClipboard && im::Button("Copy")) {
					*singleClipboard = ifList[i];
				}
				if(im::InputScalarN("##params2", ImGuiDataType_S32, p+6, 3, NULL, NULL, "%d", 0)) {
		markModified();
	}
				break;
		}
		} // End of manual mode else block

		im::PopID();
	}

	if(deleteI >= 0) {
		ifList.erase(ifList.begin() + deleteI);
		manualEditMode.erase(manualEditMode.begin() + deleteI);
		markModified();
	}

	if(im::Button("Add")) {
		ifList.push_back({});
		manualEditMode.push_back(0);
		markModified();
	}
}

// Forward declaration for smart effect UI
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

	// Effect type list
	const char* const effectTypes[] = {
		"0: (Unknown)",
		"1: Spawn Pattern",
		"2: Various Effects",
		"3: Spawn Preset Effect",
		"4: Set Opponent State (no bounce reset)",
		"5: Damage",
		"6: Various Effects 2",
		"8: Spawn Actor (effect.ha6)",
		"9: Play Audio",
		"11: Spawn Random Pattern",
		"14: Set Opponent State (reset bounces)",
		"101: Spawn Relative Pattern",
		"111: Spawn Random Relative Pattern",
		"257: (Arc typo?)",
		"1000: Spawn and Follow",
		"10002: Unknown",
	};

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

// Smart Effect UI implementation
static inline void DrawSmartEffectUI(Frame_EF& effect, FrameData* frameData, int patternIndex, std::function<void()> markModified)
{
	int* p = effect.parameters;
	int& no = effect.number;
	constexpr float width = 75.f;

	switch(effect.type) {
		case 1:   // Spawn Pattern
		case 101: // Spawn Relative Pattern
		{
			// Pattern dropdown with names
			if(frameData) {
				im::SetNextItemWidth(width*3);
				std::string currentName = frameData->GetDecoratedName(no);
				if(im::BeginCombo("Pattern", currentName.c_str())) {
					for(int i = 0; i < frameData->get_sequence_count(); i++) {
						bool selected = (no == i);
						std::string name = frameData->GetDecoratedName(i);
						if(im::Selectable(name.c_str(), selected)) {
							no = i;
							markModified();
						}
						if(selected)
							im::SetItemDefaultFocus();
					}
					im::EndCombo();
				}
			} else {
				im::SetNextItemWidth(width);
				if(im::InputInt("Pattern", &no, 0, 0)) {
					markModified();
				}
			}

			// Offset
			im::SetNextItemWidth(width);
			if(im::InputInt("Offset X", &p[0], 0, 0)) markModified();
			im::SameLine(0, 20);
			im::SetNextItemWidth(width);
			if(im::InputInt("Offset Y", &p[1], 0, 0)) markModified();

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

			// Angle
			im::SetNextItemWidth(width);
			if(im::InputInt("Angle", &p[7], 0, 0)) markModified();
			im::SameLine(); im::TextDisabled("(?)");
			if(im::IsItemHovered()) {
				Tooltip("Clockwise rotation: 0=0°, 2500=90°, 5000=180°, 10000=360°");
			}

			// Projectile var decrease
			im::SetNextItemWidth(width);
			if(im::InputInt("Proj var decrease", &p[8], 0, 0)) markModified();

			break;
		}

		case 2: // Various Effects
		{
			// Sub-type dropdown
			const char* const subTypes[] = {
				"4: Random sparkle/speed line",
				"26: (Blood) Heat",
				"50: Superflash",
				"210: Unknown (Ciel)",
				"260: Unknown (Arc/Aoko)",
				"261: Trailing effect",
				"1260: Fading circle (Boss Aoko)",
			};

			// Find index or use custom
			int knownTypes[] = {4, 26, 50, 210, 260, 261, 1260};
			if(ShowComboWithManual("Effect Number", &no, subTypes, IM_ARRAYSIZE(subTypes), width*2, width)) {
				markModified();
			}

			// Sub-type specific parameters
			if(no == 50) { // Superflash
				im::Text("--- Superflash Parameters ---");

				im::SetNextItemWidth(width);
				if(im::InputInt("Position X", &p[0], 0, 0)) markModified();
				im::SameLine(0, 20);
				im::SetNextItemWidth(width);
				if(im::InputInt("Position Y", &p[1], 0, 0)) markModified();

				im::SetNextItemWidth(width*2);
				if(im::Combo("Freeze Mode", &p[2],
					"0: Freeze self\000"
					"1: Don't freeze self (freezes projectiles)\000"
					"2: Don't freeze self or opponent\000"
					"3: No flash\000")) {
					markModified();
				}

				im::SetNextItemWidth(width);
				if(im::InputInt("Duration", &p[3], 0, 0)) markModified();
				im::SameLine(); im::TextDisabled("(?)");
				if(im::IsItemHovered()) Tooltip("0 = default 30f");

				im::SetNextItemWidth(width*2);
				if(im::Combo("Portrait", &p[4],
					"0: EX portrait\000"
					"1: AD portrait\000"
					"255: No portrait\000")) {
					markModified();
				}

				im::SetNextItemWidth(width);
				if(im::InputInt("Don't spend meter", &p[8], 0, 0)) markModified();

				im::SetNextItemWidth(width);
				if(im::InputInt("Meter gain mult", &p[9], 0, 0)) markModified();
				im::SameLine(); im::TextDisabled("(?)");
				if(im::IsItemHovered()) Tooltip("256 = 1.0x, requires param9 != 0");

				im::SetNextItemWidth(width);
				if(im::InputInt("Mult duration", &p[10], 0, 0)) markModified();
				im::SameLine(); im::TextDisabled("(?)");
				if(im::IsItemHovered()) Tooltip("Frames, requires param9 != 0");

			} else {
				// Generic parameters for other sub-types
				im::Text("Parameters:");
				if(im::InputScalarN("##params", ImGuiDataType_S32, p, 6, NULL, NULL, "%d", 0)) {
					markModified();
				}
				if(im::InputScalarN("##params2", ImGuiDataType_S32, p+6, 6, NULL, NULL, "%d", 0)) {
					markModified();
				}
			}

			break;
		}

		case 3: // Spawn Preset Effect
		{
			const char* const presetEffects[] = {
				"1: Jump looking effect",
				"2: David star (lags)",
				"3: Red hitspark",
				"4: Force field",
				"6: Fire",
				"7: Snow",
				"8: Blue flash",
				"9: Blue hitspark",
				"10: Superflash background",
				"15: Blue horizontal wave",
				"16: Red vertical wave",
				"19: Foggy rays",
				"20: 3D rotating waves",
				"23: Blinding effect",
				"24: Blinding effect 2",
				"27: Dust cloud",
				"28: Dust cloud (large)",
				"29: Dust cloud (rotating)",
				"30: Massive dust cloud",
			};

			if(ShowComboWithManual("Effect Number", &no, presetEffects, IM_ARRAYSIZE(presetEffects), width*2, width)) {
				markModified();
			}

			// Position (common to all)
			im::SetNextItemWidth(width);
			if(im::InputInt("Position X", &p[0], 0, 0)) markModified();
			im::SameLine(0, 20);
			im::SetNextItemWidth(width);
			if(im::InputInt("Position Y", &p[1], 0, 0)) markModified();

			// Type-specific parameters
			if(no == 1) { // Jump effect
				im::Text("--- Jump Effect ---");
				im::SetNextItemWidth(width);
				if(im::InputInt("Duration", &p[2], 0, 0)) markModified();
				im::SetNextItemWidth(width);
				if(im::InputInt("Size", &p[3], 0, 0)) markModified();
				im::SetNextItemWidth(width);
				if(im::InputInt("Growth rate", &p[4], 0, 0)) markModified();
			} else if(no == 3 || no == 9) { // Hitsparks
				im::Text("--- Hitspark ---");
				im::SetNextItemWidth(width);
				if(im::InputInt("Intensity", &p[2], 0, 0)) markModified();
			} else if(no >= 27 && no <= 30) { // Dust clouds
				im::Text("--- Dust Cloud ---");
				im::SetNextItemWidth(width);
				if(im::InputInt("X speed", &p[2], 0, 0)) markModified();
				im::SetNextItemWidth(width);
				if(im::InputInt("Y speed", &p[3], 0, 0)) markModified();
				im::SetNextItemWidth(width);
				if(im::InputInt("Duration", &p[4], 0, 0)) markModified();
				im::SetNextItemWidth(width);
				if(im::Combo("Color", &p[5], "0: Brown\0001: Black\0002: Purple\0003: White\000")) {
					markModified();
				}
				im::SetNextItemWidth(width);
				if(im::InputInt("Flags", &p[6], 0, 0)) markModified();
				im::SetNextItemWidth(width);
				if(im::InputInt("Amount", &p[7], 0, 0)) markModified();
			} else {
				// Generic
				im::Text("Parameters:");
				if(im::InputScalarN("##params", ImGuiDataType_S32, p, 6, NULL, NULL, "%d", 0)) {
					markModified();
				}
			}

			break;
		}

		case 11:  // Spawn Random Pattern
		case 111: // Spawn Random Relative Pattern
		{
			// Similar to type 1, but for random patterns
			if(frameData) {
				im::SetNextItemWidth(width*3);
				std::string currentName = frameData->GetDecoratedName(no);
				if(im::BeginCombo("Pattern", currentName.c_str())) {
					for(int i = 0; i < frameData->get_sequence_count(); i++) {
						bool selected = (no == i);
						std::string name = frameData->GetDecoratedName(i);
						if(im::Selectable(name.c_str(), selected)) {
							no = i;
							markModified();
						}
						if(selected)
							im::SetItemDefaultFocus();
					}
					im::EndCombo();
				}
			} else {
				im::SetNextItemWidth(width);
				if(im::InputInt("Pattern", &no, 0, 0)) {
					markModified();
				}
			}

			// Random range
			im::SetNextItemWidth(width);
			if(im::InputInt("Random range", &p[0], 0, 0)) markModified();
			im::SameLine(); im::TextDisabled("(?)");
			if(im::IsItemHovered()) {
				Tooltip("Spawns pattern (Pattern + random(0, range))");
			}

			// Offset
			im::SetNextItemWidth(width);
			if(im::InputInt("Offset X", &p[1], 0, 0)) markModified();
			im::SameLine(0, 20);
			im::SetNextItemWidth(width);
			if(im::InputInt("Offset Y", &p[2], 0, 0)) markModified();

			// Same flagsets as type 1
			if(im::TreeNode("Flagset 1 (Spawn Behavior)")) {
				unsigned int flagIdx = -1;
				if(BitField("##flags1", (unsigned int*)&p[3], &flagIdx, 13)) {
					markModified();
				}

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

				im::Text("Raw value: %d", p[3]);
				im::TreePop();
			}

			if(im::TreeNode("Flagset 2 (Child Properties)")) {
				unsigned int flagIdx = -1;
				if(BitField("##flags2", (unsigned int*)&p[4], &flagIdx, 13)) {
					markModified();
				}

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

				im::Text("Raw value: %d", p[4]);
				im::TreePop();
			}

			// Angle
			im::SetNextItemWidth(width);
			if(im::InputInt("Angle", &p[8], 0, 0)) markModified();
			im::SameLine(); im::TextDisabled("(?)");
			if(im::IsItemHovered()) {
				Tooltip("Clockwise rotation: 0=0°, 2500=90°, 5000=180°, 10000=360°");
			}

			// Projectile var decrease
			im::SetNextItemWidth(width);
			if(im::InputInt("Proj var decrease", &p[9], 0, 0)) markModified();

			break;
		}

		case 4:  // Set Opponent State (no bounce reset)
		case 14: // Set Opponent State (reset bounces)
		{
			// Pattern dropdown for opponent state
			if(frameData) {
				im::SetNextItemWidth(width*3);
				std::string currentName = frameData->GetDecoratedName(no);
				if(im::BeginCombo("Opponent Pattern", currentName.c_str())) {
					for(int i = 0; i < frameData->get_sequence_count(); i++) {
						bool selected = (no == i);
						std::string name = frameData->GetDecoratedName(i);
						if(im::Selectable(name.c_str(), selected)) {
							no = i;
							markModified();
						}
						if(selected)
							im::SetItemDefaultFocus();
					}
					im::EndCombo();
				}
			} else {
				im::SetNextItemWidth(width);
				if(im::InputInt("Opponent Pattern", &no, 0, 0)) {
					markModified();
				}
			}
			im::SameLine(); im::TextDisabled("(?)");
			if(im::IsItemHovered()) {
				Tooltip("Usually only 23, 24, 26, 30, 350, 354");
			}

			// Position
			im::SetNextItemWidth(width);
			if(im::InputInt("X pos", &p[0], 0, 0)) markModified();
			im::SameLine(0, 20);
			im::SetNextItemWidth(width);
			if(im::InputInt("Y pos", &p[1], 0, 0)) markModified();

			// Type 14 specific
			if(effect.type == 14) {
				im::SetNextItemWidth(width);
				if(im::InputInt("Rotation", &p[2], 0, 0)) markModified();
				im::SameLine(); im::TextDisabled("(?)");
				if(im::IsItemHovered()) {
					Tooltip("Rotation unit (large values like 1000, 4000 used)");
				}
			} else {
				// Type 4 has unknown param3
				im::SetNextItemWidth(width);
				if(im::InputInt("Unknown", &p[2], 0, 0)) markModified();
				im::SameLine(); im::TextDisabled("(?)");
				if(im::IsItemHovered()) {
					Tooltip("Found in old airthrows, probably leftover");
				}
			}

			// Flags
			if(im::TreeNode("Flags")) {
				unsigned int flagIdx = -1;
				if(BitField("##flags", (unsigned int*)&p[3], &flagIdx, 7)) {
					markModified();
				}

				switch(flagIdx) {
					case 0: Tooltip("Play animation"); break;
					case 1: Tooltip("Reverse vector"); break;
					case 2: Tooltip("Can't OTG"); break;
					case 3:
						if(effect.type == 14) {
							Tooltip("Make enemy unhittable");
						} else {
							Tooltip("Unknown");
						}
						break;
					case 4:
						if(effect.type == 14) {
							Tooltip("Use last position?");
						} else {
							Tooltip("Unknown");
						}
						break;
					case 5: Tooltip("Hard Knockdown"); break;
				}

				im::Text("Raw value: %d", p[3]);
				im::TreePop();
			}

			// Vector ID
			im::SetNextItemWidth(width*2);
			if(ShowComboWithManual("Vector ID", &p[4], hitVectorList, IM_ARRAYSIZE(hitVectorList), width*2, width)) {
				markModified();
			}
			im::SameLine(); im::TextDisabled("(?)");
			if(im::IsItemHovered()) {
				Tooltip("Works only if animation plays");
			}

			// Untech time
			im::SetNextItemWidth(width);
			if(im::InputInt("Untech time", &p[5], 0, 0)) markModified();
			im::SameLine(); im::TextDisabled("(?)");
			if(im::IsItemHovered()) {
				Tooltip(effect.type == 14 ?
					"Used with vectors" :
					"0 = infinite, only works for airstun vectors");
			}

			// Type 14 specific param7
			if(effect.type == 14) {
				im::SetNextItemWidth(width);
				if(im::Combo("Char location", &p[6], "0: Self\0001: Opponent\000")) {
					markModified();
				}
			} else {
				im::SetNextItemWidth(width);
				if(im::InputInt("Unknown", &p[6], 0, 0)) markModified();
				im::SameLine(); im::TextDisabled("(?)");
				if(im::IsItemHovered()) {
					Tooltip("Most of the time it's 0");
				}
			}

			// Opponent's frame
			if(effect.type == 4) {
				im::SetNextItemWidth(width);
				if(im::InputInt("Opponent frame", &p[7], 0, 0)) markModified();
			}

			break;
		}

		case 5: // Damage
		{
			const char* const damageTypes[] = {
				"0: Set added effect",
				"1: Damage opponent",
				"4: Unknown (obsolete?)",
			};

			int typeIndex = -1;
			int knownTypes[] = {0, 1, 4};
			for(int j = 0; j < IM_ARRAYSIZE(knownTypes); j++) {
				if(no == knownTypes[j]) {
					typeIndex = j;
					break;
				}
			}

			if(typeIndex >= 0) {
				im::SetNextItemWidth(width*2);
				if(im::Combo("Damage Type", &typeIndex, damageTypes, IM_ARRAYSIZE(damageTypes))) {
					no = knownTypes[typeIndex];
					markModified();
				}
			} else {
				im::SetNextItemWidth(width);
				if(im::InputInt("Type", &no, 0, 0)) {
					markModified();
				}
			}

			if(no == 0) { // Set added effect
				im::SetNextItemWidth(width*2);
				int effectTypes[] = {1, 2, 3, 4, 100};
				const char* effectNames[] = {
					"1: Burn",
					"2: Freeze",
					"3: Shock",
					"4: Confuse",
					"100: Black sprite",
				};

				// Find current index
				int currentIdx = 0;
				for(int i = 0; i < IM_ARRAYSIZE(effectTypes); i++) {
					if(p[0] == effectTypes[i]) {
						currentIdx = i;
						break;
					}
				}

				if(im::Combo("Effect", &currentIdx, effectNames, IM_ARRAYSIZE(effectNames))) {
					p[0] = effectTypes[currentIdx];
					markModified();
				}
			} else if(no == 1) { // Damage opponent
				im::SetNextItemWidth(width);
				if(im::InputInt("Damage", &p[0], 0, 0)) markModified();

				im::SetNextItemWidth(width);
				if(im::Combo("Add to hit count", &p[1], "0\0001\0002\000")) {
					markModified();
				}

				im::SetNextItemWidth(width);
				if(im::InputInt("Hitstop", &p[2], 0, 0)) markModified();

				im::SetNextItemWidth(width);
				if(im::InputInt("Hit sound", &p[3], 0, 0)) markModified();
				im::SameLine(); im::TextDisabled("(?)");
				if(im::IsItemHovered()) {
					Tooltip("Same as in AT data");
				}

				im::SetNextItemWidth(width);
				if(im::Combo("Hit scaling", &p[4], "0\0001\000")) {
					markModified();
				}

				im::SetNextItemWidth(width);
				if(im::InputInt("VS damage", &p[5], 0, 0)) markModified();
			} else if(no == 4) { // Unknown/obsolete
				im::SetNextItemWidth(width);
				if(im::InputInt("Unknown", &p[0], 0, 0)) markModified();
				im::SameLine(); im::TextDisabled("(?)");
				if(im::IsItemHovered()) {
					Tooltip("Always -10, probably obsolete");
				}
			} else {
				// Unknown damage type
				im::Text("Parameters:");
				if(im::InputScalarN("##params", ImGuiDataType_S32, p, 6, NULL, NULL, "%d", 0)) {
					markModified();
				}
			}

			break;
		}

		case 6: // Various Effects 2
		{
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
					if(im::InputInt("Number of images", &p[1], 0, 0)) markModified();

					im::SetNextItemWidth(width);
					if(im::InputInt("Frames behind", &p[2], 0, 0)) markModified();
					im::SameLine(); im::TextDisabled("(?)");
					if(im::IsItemHovered()) {
						Tooltip("Frames each image is behind previous (max 32)");
					}
				}

			} else if(no == 1) { // Screen effects
				im::Text("--- Screen Effects ---");

				im::SetNextItemWidth(width);
				if(im::InputInt("Shake duration", &p[0], 0, 0)) markModified();

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
				if(im::InputInt("Effect duration", &p[2], 0, 0)) markModified();

				im::SetNextItemWidth(width);
				if(im::InputInt("Slowdown duration", &p[3], 0, 0)) markModified();

			} else if(no == 2) { // Invulnerability
				im::Text("--- Invulnerability ---");

				im::SetNextItemWidth(width);
				if(im::InputInt("Strike invuln", &p[0], 0, 0)) markModified();

				im::SetNextItemWidth(width);
				if(im::InputInt("Throw invuln", &p[1], 0, 0)) markModified();

			} else if(no == 4) { // Gauges
				im::Text("--- Gauges ---");

				im::SetNextItemWidth(width);
				if(im::InputInt("Health change", &p[0], 0, 0)) markModified();
				im::SameLine(); im::TextDisabled("(?)");
				if(im::IsItemHovered()) {
					Tooltip("May not exceed Red Health");
				}

				im::SetNextItemWidth(width);
				if(im::InputInt("Meter change", &p[1], 0, 0)) markModified();

				im::SetNextItemWidth(width);
				if(im::InputInt("MAX/HEAT time", &p[2], 0, 0)) markModified();
				im::SameLine(); im::TextDisabled("(?)");
				if(im::IsItemHovered()) {
					Tooltip("Only works if owner is in MAX/HEAT/BLOOD HEAT");
				}

				im::SetNextItemWidth(width);
				if(im::InputInt("Red Health change", &p[3], 0, 0)) markModified();

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
				if(im::InputInt("Min speed", &p[0], 0, 0)) markModified();

				im::SetNextItemWidth(width);
				if(im::InputInt("Max speed", &p[1], 0, 0)) markModified();

				im::SetNextItemWidth(width);
				if(im::InputInt("Min accel", &p[2], 0, 0)) markModified();

				im::SetNextItemWidth(width);
				if(im::InputInt("Max accel", &p[3], 0, 0)) markModified();

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
				if(im::InputInt("Value", &p[0], 0, 0)) markModified();

			} else if(no == 10) { // Various effects (armor/confusion)
				im::Text("--- Various Effects ---");

				im::SetNextItemWidth(width);
				if(im::Combo("Effect", &p[0], "0: Armor\0001: Confusion\000")) {
					markModified();
				}

				im::SetNextItemWidth(width);
				if(im::InputInt("Duration", &p[1], 0, 0)) markModified();

			} else if(no == 11) { // Super flash
				im::Text("--- Super Flash ---");

				im::SetNextItemWidth(width);
				if(im::InputInt("Global flash dur", &p[0], 0, 0)) markModified();
				im::SameLine(); im::TextDisabled("(?)");
				if(im::IsItemHovered()) {
					Tooltip("Only if param2 != 0");
				}

				im::SetNextItemWidth(width);
				if(im::InputInt("Player flash", &p[1], 0, 0)) markModified();
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
					if(im::InputInt("X offset", &p[0], 0, 0)) markModified();
					im::SetNextItemWidth(width);
					if(im::InputInt("Y offset", &p[1], 0, 0)) markModified();

				} else if(p[11] == 1) {
					im::SetNextItemWidth(width);
					if(im::InputInt("X position", &p[0], 0, 0)) markModified();
					im::SetNextItemWidth(width);
					if(im::InputInt("Y position", &p[1], 0, 0)) markModified();

					im::SetNextItemWidth(width);
					if(im::Combo("X mode", &p[2],
						"0: Absolute\0001: Relative to facing\000")) {
						markModified();
					}

				} else if(p[11] == 2) {
					im::SetNextItemWidth(width);
					if(im::InputInt("X offset", &p[0], 0, 0)) markModified();
					im::SetNextItemWidth(width);
					if(im::InputInt("Y offset", &p[1], 0, 0)) markModified();
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
					if(im::InputInt("Base angle", &p[0], 0, 0)) markModified();
					im::SameLine(); im::TextDisabled("(?)");
					if(im::IsItemHovered()) {
						Tooltip("Degrees clockwise");
					}

					im::SetNextItemWidth(width);
					if(im::InputInt("Random angle range", &p[1], 0, 0)) markModified();

					im::SetNextItemWidth(width);
					if(im::InputInt("Base speed", &p[2], 0, 0)) markModified();

					im::SetNextItemWidth(width);
					if(im::InputInt("Random speed range", &p[3], 0, 0)) markModified();

				} else {
					im::SetNextItemWidth(width);
					if(im::InputInt("X", &p[0], 0, 0)) markModified();

					im::SetNextItemWidth(width);
					if(im::InputInt("Y", &p[1], 0, 0)) markModified();

					im::SetNextItemWidth(width);
					if(im::InputInt("Velocity divisor", &p[2], 0, 0)) markModified();
					im::SameLine(); im::TextDisabled("(?)");
					if(im::IsItemHovered()) {
						Tooltip("If 1 or 0, travels entire distance in 1 frame");
					}
				}

			} else if(no == 19) { // Prorate
				im::Text("--- Prorate ---");

				im::SetNextItemWidth(width);
				if(im::InputInt("Proration value", &p[0], 0, 0)) markModified();

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
				if(im::InputInt("Param1", &p[0], 0, 0)) markModified();
				im::SameLine(); im::TextDisabled("(?)");
				if(im::IsItemHovered()) {
					Tooltip("1s place: Amount\n10s place: Variable ID");
				}

			} else if(no == 102 || no == 103) { // Increase/Decrease Dash variable
				im::Text(no == 102 ? "--- Increase Dash Var ---" : "--- Decrease Dash Var ---");

				im::SetNextItemWidth(width);
				if(im::InputInt("Amount", &p[0], 0, 0)) markModified();

			} else if(no == 105) { // Change variable
				im::Text("--- Change Variable ---");

				im::SetNextItemWidth(width);
				if(im::InputInt("Variable ID", &p[0], 0, 0)) markModified();
				im::SameLine(); im::TextDisabled("(?)");
				if(im::IsItemHovered()) {
					Tooltip("0-9, can overflow with <0 and >9");
				}

				im::SetNextItemWidth(width);
				if(im::InputInt("Value", &p[1], 0, 0)) markModified();

				im::SetNextItemWidth(width);
				if(im::Combo("Mode", &p[2], "0: Set\0001: Add\000")) {
					markModified();
				}

			} else if(no == 106) { // Make projectile no longer despawn on hit
				im::Text("Deactivates EFTP1 P3 bit0");

			} else if(no == 107) { // Change frames into pattern
				im::Text("--- Change Frames ---");

				im::SetNextItemWidth(width);
				if(im::InputInt("Value", &p[0], 0, 0)) markModified();

				im::SetNextItemWidth(width);
				if(im::Combo("Mode", &p[1], "0: Set\0001: Add\000")) {
					markModified();
				}

			} else if(no == 110) { // Count normal as used
				im::SetNextItemWidth(width);
				if(im::InputInt("Pattern - 1", &p[0], 0, 0)) markModified();
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
				if(im::InputInt("Clear movement", &p[1], 0, 0)) markModified();
				im::SameLine(); im::TextDisabled("(?)");
				if(im::IsItemHovered()) {
					Tooltip("Clear current movement if not 0");
				}

				im::SetNextItemWidth(width);
				if(im::InputInt("Set stored Y accel", &p[2], 0, 0)) markModified();
				im::SameLine(); im::TextDisabled("(?)");
				if(im::IsItemHovered()) {
					Tooltip("If not 0 and current Y velocity/accel < 1");
				}

			} else if(no == 112) { // Change proration
				im::Text("--- Change Proration ---");

				im::SetNextItemWidth(width);
				if(im::InputInt("Proration", &p[0], 0, 0)) markModified();

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
				if(im::InputInt("Partner pattern", &p[0], 0, 0)) markModified();

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
				if(im::InputInt("Value", &p[0], 0, 0)) markModified();

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

			break;
		}

		case 8: // Spawn Actor (effect.ha6)
		{
			im::Text("Same params as Effect 1");

			const char* const actorTypes[] = {
				"5: Clash",
				"26: Charge [X]",
			};

			if(ShowComboWithManual("Actor", &no, actorTypes, IM_ARRAYSIZE(actorTypes), width*2, width)) {
				markModified();
			}

			// Pattern dropdown (same as type 1)
			im::SetNextItemWidth(width);
			if(im::InputInt("Offset X", &p[0], 0, 0)) markModified();
			im::SameLine(0, 20);
			im::SetNextItemWidth(width);
			if(im::InputInt("Offset Y", &p[1], 0, 0)) markModified();

			// Show flagsets collapsed
			if(im::TreeNode("Flagset 1 (same as Type 1)")) {
				unsigned int flagIdx = -1;
				if(BitField("##flags1", (unsigned int*)&p[2], &flagIdx, 13)) {
					markModified();
				}
				im::Text("Raw value: %d", p[2]);
				im::TreePop();
			}

			if(im::TreeNode("Flagset 2 (same as Type 1)")) {
				unsigned int flagIdx = -1;
				if(BitField("##flags2", (unsigned int*)&p[3], &flagIdx, 13)) {
					markModified();
				}
				im::Text("Raw value: %d", p[3]);
				im::TreePop();
			}

			break;
		}

		case 9: // Play Audio
		{
			im::SetNextItemWidth(width*2);
			if(im::Combo("Bank", &no,
				"0: Universal sounds\000"
				"1: Character specific\000")) {
				markModified();
			}

			im::SetNextItemWidth(width);
			if(im::InputInt("Sound ID", &p[0], 0, 0)) markModified();

			im::SetNextItemWidth(width);
			if(im::InputInt("Probability", &p[1], 0, 0)) markModified();
			im::SameLine(); im::TextDisabled("(?)");
			if(im::IsItemHovered()) {
				Tooltip("0 = 100%");
			}

			im::SetNextItemWidth(width);
			if(im::InputInt("Different sounds", &p[2], 0, 0)) markModified();
			im::SameLine(); im::TextDisabled("(?)");
			if(im::IsItemHovered()) {
				Tooltip("1 and 0 equivalent, IDs adjacent");
			}

			im::SetNextItemWidth(width);
			if(im::InputInt("Unknown (param4)", &p[3], 0, 0)) markModified();
			im::SameLine(); im::TextDisabled("(?)");
			if(im::IsItemHovered()) {
				Tooltip("Sometimes 1");
			}

			im::SetNextItemWidth(width);
			if(im::InputInt("Unknown (param6)", &p[5], 0, 0)) markModified();

			im::SetNextItemWidth(width);
			if(im::InputInt("Unknown (param7)", &p[6], 0, 0)) markModified();
			im::SameLine(); im::TextDisabled("(?)");
			if(im::IsItemHovered()) {
				Tooltip("Either 2 or 0, probably related to param6");
			}

			im::SetNextItemWidth(width);
			if(im::InputInt("Unknown (param12)", &p[11], 0, 0)) markModified();
			im::SameLine(); im::TextDisabled("(?)");
			if(im::IsItemHovered()) {
				Tooltip("Sometimes 1, used in time up/intro/win");
			}

			break;
		}

		case 257: // Arc typo
		{
			im::Text("Arc pattern 124 typo - no effect");
			im::Text("Parameters:");
			if(im::InputScalarN("##params", ImGuiDataType_S32, p, 6, NULL, NULL, "%d", 0)) {
				markModified();
			}
			break;
		}

		case 1000: // Spawn and follow
		{
			im::Text("Spawns and follows (Dust of Osiris, Sion)");
			im::Text("Probably interacts with pattern data");

			im::SetNextItemWidth(width);
			if(im::InputInt("Pattern", &no, 0, 0)) {
				markModified();
			}

			im::Text("Parameters:");
			if(im::InputScalarN("##params", ImGuiDataType_S32, p, 6, NULL, NULL, "%d", 0)) {
				markModified();
			}
			break;
		}

		case 10002: // Unknown
		{
			im::Text("Only for projectiles");

			im::SetNextItemWidth(width);
			if(im::InputInt("Param1 (*256)", &p[0], 0, 0)) markModified();
			im::SameLine(); im::TextDisabled("(?)");
			if(im::IsItemHovered()) {
				Tooltip("Calculates X + (Param1 * 256)");
			}

			im::SetNextItemWidth(width);
			if(im::InputInt("Param2 (*256)", &p[1], 0, 0)) markModified();
			im::SameLine(); im::TextDisabled("(?)");
			if(im::IsItemHovered()) {
				Tooltip("Calculates Y + (Param2 * 256)");
			}

			im::Text("Other parameters:");
			if(im::InputScalarN("##params", ImGuiDataType_S32, p+2, 6, NULL, NULL, "%d", 0)) {
				markModified();
			}
			break;
		}

		// Add more cases for other effect types as needed...

		default:
			// Unknown effect type - show raw parameters
			im::SetNextItemWidth(width);
			if(im::InputInt("Number", &no, 0, 0)) {
				markModified();
			}
			im::Text("Parameters:");
			if(im::InputScalarN("##params", ImGuiDataType_S32, p, 6, NULL, NULL, "%d", 0)) {
				markModified();
			}
			if(im::InputScalarN("##params2", ImGuiDataType_S32, p+6, 6, NULL, NULL, "%d", 0)) {
				markModified();
			}
			break;
	}
}

inline void AsDisplay(Frame_AS *as, FrameData *frameData = nullptr, int patternIndex = -1, std::function<void()> onModified = nullptr)
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
	if(BitField("Movement Flags", &as->movementFlags, &flagIndex, 8)) {
		markModified();
	}
	switch (flagIndex)
	{
		case 0: Tooltip("Set Y"); break;
		case 4: Tooltip("Set X"); break;
		case 1: Tooltip("Add Y"); break;
		case 5: Tooltip("Add X"); break;
	}

	im::SetNextItemWidth(width*2);
	if(im::InputInt2("Speed", as->speed)) {
		markModified();
	}
	im::SameLine(0.f, 20); im::SetNextItemWidth(width);
	if(im::InputInt("Max X speed", &as->maxSpeedX, 0, 0)) {
		markModified();
	}
	im::SetNextItemWidth(width*2);
	if(im::InputInt2("Accel", as->accel)) {
		markModified();
	}
	
	im::Separator();
	flagIndex = -1;
	if(BitField("Flagset 1", &as->statusFlags[0], &flagIndex)) {
		markModified();
	}
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
	if(BitField("Flagset 2", &as->statusFlags[1], &flagIndex)) {
		markModified();
	}
	switch (flagIndex)
	{
		case 0: Tooltip("Can always EX cancel"); break;
		case 2: Tooltip("Can only jump cancel"); break;
		case 31: Tooltip("Can't block"); break;
	}

	im::SetNextItemWidth(width);
	if(im::InputInt("Number of hits", &as->hitsNumber, 0, 0)) {
		markModified();
	}
	im::SameLine(0,20.f);
	if(im::Checkbox("Player can move", &as->canMove)) {
		markModified();
	}
	if(im::Combo("State", &as->stanceState, stateList, IM_ARRAYSIZE(stateList))) {
		markModified();
	}
	if(im::Combo("Invincibility", &as->invincibility, invulList, IM_ARRAYSIZE(invulList))) {
		markModified();
	}
	if(im::Combo("Counterhit", &as->counterType, counterList, IM_ARRAYSIZE(counterList))) {
		markModified();
	}
	if(im::Combo("Cancel normal", &as->cancelNormal, cancelList, IM_ARRAYSIZE(cancelList))) {
		markModified();
	}
	if(im::Combo("Cancel special", &as->cancelSpecial, cancelList, IM_ARRAYSIZE(cancelList))) {
		markModified();
	}
	

	im::Separator();
	flagIndex = -1;
	if(BitField("Sine flags", &as->sineFlags, &flagIndex, 8)) {
		markModified();
	}
	switch (flagIndex)
	{
		case 0: Tooltip("Use Y"); break;
		case 4: Tooltip("Use X"); break;
	}
	if(im::InputInt4("Sinewave", as->sineParameters)) {
		markModified();
	}
	im::SameLine();
	im::TextDisabled("(?)");
	if(im::IsItemHovered())
		Tooltip("Sine parameters:\nX dist, Y dist\nX frequency, Y frequency");
	if(im::InputFloat2("Phases", as->sinePhases)) {
		markModified();
	}
}

inline void AtDisplay(Frame_AT *at, FrameData *frameData = nullptr, int patternIndex = -1, std::function<void()> onModified = nullptr)
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

	if(BitField("Guard Flags", &at->guard_flags, &flagIndex)) {
		markModified();
	}
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
	if(BitField("Hit Flags", &at->otherFlags, &flagIndex)) {
		markModified();
	}
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
	if(im::InputInt("Custom Blockstop", &at->blockStopTime, 0,0)) {
		markModified();
	}

	im::SetNextItemWidth(width*2);
	if(im::Combo("Hitstop", &at->hitStop, hitStopList, IM_ARRAYSIZE(hitStopList))) {
		markModified();
	}
	im::SameLine(0.f, 20);
	im::SetNextItemWidth(width);
	if(im::InputInt("Custom##Hitstop", &at->hitStopTime, 0,0)) {
		markModified();
	}


	im::SetNextItemWidth(width);
	if(im::InputInt("Untech time", &at->untechTime, 0,0)) {
		markModified();
	}
	im::SameLine(0.f, 20); im::SetNextItemWidth(width);
	if(im::InputInt("Circuit break time", &at->breakTime, 0,0)) {
		markModified();
	}

	im::SetNextItemWidth(width);
	if(im::InputFloat("Extra gravity", &at->extraGravity, 0,0)) {
		markModified();
	}
	im::SameLine(0.f, 20);
	if(im::Checkbox("Hitgrab", &at->hitgrab)) {
		markModified();
	}


	im::SetNextItemWidth(width);
	if(im::InputInt("Correction %", &at->correction, 0, 0)) {
		markModified();
	}
	im::SameLine(0.f, 20); im::SetNextItemWidth(width*2);
	if(im::Combo("Type##Correction", &at->correction_type, "Normal\0Multiplicative\0Subtractive\0")) {
		markModified();
	}

	im::SetNextItemWidth(width);
	if(im::InputInt("VS damage", &at->red_damage, 0, 0)) {
		markModified();
	}
	im::SameLine(0.f, 20); im::SetNextItemWidth(width);
	if(im::InputInt("Damage", &at->damage, 0, 0)) {
		markModified();
	}

	im::SetNextItemWidth(width);
	if(im::InputInt("Guard damage", &at->guard_damage, 0, 0)) {
		markModified();
	}
	im::SameLine(0.f, 20); im::SetNextItemWidth(width);
	if(im::InputInt("Meter gain", &at->meter_gain, 0, 0)) {
		markModified();
	}

	im::Separator();
	auto comboWidth = (im::GetWindowWidth())/4.f;

	// Guard Vector
	im::Text("Guard Vector:");
	im::SameLine(0.f, 20);
	static bool guardVectorManual = false;
	if(im::Checkbox("Manual##gv", &guardVectorManual)) {
	}
	if(im::IsItemHovered()) {
		Tooltip("Enable manual vector entry");
	}

	const char* const vectorLabels[] = {"Stand", "Air", "Crouch"};
	if(guardVectorManual) {
		// Manual mode - show InputInt3
		if(im::InputInt3("##guardVec", at->guardVector)) {
			markModified();
		}
	} else {
		// Dropdown mode - Stand, Air, Crouch with dropdowns
		for(int i = 0; i < 3; i++)
		{
			im::PushID(100+i);
			if(ShowComboWithManual(vectorLabels[i], &at->guardVector[i], hitVectorList, IM_ARRAYSIZE(hitVectorList), width*6, width)) {
			markModified();
		}
			im::PopID();
		}
	}

	// Guard Vector Flags
	for(int i = 0; i < 3; i++)
	{
		im::SetNextItemWidth(comboWidth);
		if(i > 0)
			im::SameLine();
		im::PushID(i);
		if(im::Combo("##GFLAG", &at->gVFlags[i], vectorFlags, IM_ARRAYSIZE(vectorFlags))) {
		markModified();
	}
		im::PopID();
	}

	im::Separator();

	// Hit Vector
	im::Text("Hit Vector:");
	im::SameLine(0.f, 20);
	static bool hitVectorManual = false;
	if(im::Checkbox("Manual##hv", &hitVectorManual)) {
	}
	if(im::IsItemHovered()) {
		Tooltip("Enable manual vector entry");
	}

	if(hitVectorManual) {
		// Manual mode - show InputInt3
		if(im::InputInt3("##hitVec", at->hitVector)) {
			markModified();
		}
	} else {
		// Dropdown mode - Stand, Air, Crouch with dropdowns
		for(int i = 0; i < 3; i++)
		{
			im::PushID(200+i);
			if(ShowComboWithManual(vectorLabels[i], &at->hitVector[i], hitVectorList, IM_ARRAYSIZE(hitVectorList), width*6, width)) {
			markModified();
		}
			im::PopID();
		}
	}

	// Hit Vector Flags
	for(int i = 0; i < 3; i++)
	{
		im::SetNextItemWidth(comboWidth);
		if(i > 0)
			im::SameLine();
		im::PushID(i);
		if(im::Combo("##HFLAG", &at->hVFlags[i], vectorFlags, IM_ARRAYSIZE(vectorFlags))) {
		markModified();
	}
		im::PopID();
	}
	im::Separator();

	im::SetNextItemWidth(150);
	if(im::Combo("Hit effect", &at->hitEffect, hitEffectList, IM_ARRAYSIZE(hitEffectList))) {
		markModified();
	}
	im::SameLine(0, 20.f);
	im::SetNextItemWidth(70);
	if(im::InputInt("ID##Hit effect", &at->hitEffect, 0, 0)) {
		markModified();
	}

	im::SetNextItemWidth(70);
	if(im::InputInt("Sound effect", &at->soundEffect, 0, 0)) {
		markModified();
	}
	im::SameLine(0, 20.f); im::SetNextItemWidth(120);

	if(im::Combo("Added effect", &at->addedEffect, addedEffectList, IM_ARRAYSIZE(addedEffectList))) {
		markModified();
	}



}

inline void AfDisplay(Frame_AF *af, int &selectedLayer, FrameData *frameData = nullptr, int patternIndex = -1, std::function<void()> onModified = nullptr)
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

	// Copy/paste buttons for animation section
	static Frame_AF copiedAnimation = {};
	if(im::SmallButton("Copy animation")) {
		copiedAnimation.spriteId = af->spriteId;
		copiedAnimation.usePat = af->usePat;
		copiedAnimation.duration = af->duration;
		copiedAnimation.aniType = af->aniType;
		copiedAnimation.aniFlag = af->aniFlag;
		copiedAnimation.jump = af->jump;
		copiedAnimation.landJump = af->landJump;
		copiedAnimation.priority = af->priority;
		copiedAnimation.loopCount = af->loopCount;
		copiedAnimation.loopEnd = af->loopEnd;
	}
	if(im::IsItemHovered()) Tooltip("Copy sprite, duration, jumps, priority, and loops");
	im::SameLine();
	if(im::SmallButton("Paste animation")) {
		af->spriteId = copiedAnimation.spriteId;
		af->usePat = copiedAnimation.usePat;
		af->duration = copiedAnimation.duration;
		af->aniType = copiedAnimation.aniType;
		af->aniFlag = copiedAnimation.aniFlag;
		af->jump = copiedAnimation.jump;
		af->landJump = copiedAnimation.landJump;
		af->priority = copiedAnimation.priority;
		af->loopCount = copiedAnimation.loopCount;
		af->loopEnd = copiedAnimation.loopEnd;
		markModified();
	}
	if(im::IsItemHovered()) Tooltip("Paste sprite, duration, jumps, priority, and loops");

	// MBAACC uses flat structure (no layers)
	im::SetNextItemWidth(width*3);
	if(im::InputInt("Sprite", &af->spriteId)) {
		markModified();
	}
	im::SameLine(0, 20.f);
	if(im::Checkbox("Use .pat", &af->usePat)) {
		markModified();
	}

	im::Separator();

	unsigned int flagIndex = -1;
	if(BitField("Animation flags", &af->aniFlag, &flagIndex, 4)) {
		markModified();
	}
	switch (flagIndex)
	{
		case 0: Tooltip("Landing frame: Land to pattern"); break;
		case 1: Tooltip("Loop: Decrement loops and go to end if 0"); break;
		case 2: Tooltip("Go to: Use relative offset"); break;
		case 3: Tooltip("End of loop: Use relative offset"); break;
	}

	if(im::Combo("Animation", &af->aniType, animationList, IM_ARRAYSIZE(animationList))) {
		markModified();
	}

	im::SetNextItemWidth(width);
	if(im::InputInt("Go to", &af->jump, 0, 0)) {
		markModified();
	}
	im::SameLine(0.f, 20); im::SetNextItemWidth(width);
	if(im::InputInt("Landing frame", &af->landJump, 0, 0)) {
		markModified();
	}

	// Landing frame tools
	if(frameData && patternIndex >= 0) {
		static int landingFrameToolValue = 0;
		static int landingFrameRange[2] = {0, 0};

		im::SetNextItemWidth(width);
		im::InputInt("##LandingValue", &landingFrameToolValue, 0, 0);
		if(im::IsItemHovered()) Tooltip("Landing frame value to set");

		im::SameLine();
		if(im::SmallButton("Set All")) {
			auto seq = frameData->get_sequence(patternIndex);
			if(seq) {
				for(int i = 0; i < seq->frames.size(); i++) {
					seq->frames[i].AF.landJump = landingFrameToolValue;
				}
				frameData->mark_modified(patternIndex);
				markModified();
			}
		}
		if(im::IsItemHovered()) Tooltip("Apply landing frame to all frames in pattern");

		im::SameLine();
		if(im::SmallButton("Set Range")) {
			im::OpenPopup("LandingFrameRange");
		}
		if(im::IsItemHovered()) Tooltip("Apply landing frame to a range of frames");

		// Range popup
		if(im::BeginPopup("LandingFrameRange")) {
			auto seq = frameData->get_sequence(patternIndex);
			if(seq) {
				const int maxFrame = seq->frames.size() - 1;

				// Clamp range values
				if(landingFrameRange[0] < 0) landingFrameRange[0] = 0;
				if(landingFrameRange[1] < 0) landingFrameRange[1] = 0;
				if(landingFrameRange[0] > maxFrame) landingFrameRange[0] = maxFrame;
				if(landingFrameRange[1] > maxFrame) landingFrameRange[1] = maxFrame;

				im::Text("Set landing frame for range");
				im::Separator();
				im::InputInt2("Frame range", landingFrameRange);
				im::InputInt("Landing frame value", &landingFrameToolValue);

				if(im::Button("Apply", ImVec2(120, 0))) {
					for(int i = landingFrameRange[0]; i <= landingFrameRange[1] && i >= 0 && i < seq->frames.size(); i++) {
						seq->frames[i].AF.landJump = landingFrameToolValue;
					}
					frameData->mark_modified(patternIndex);
					markModified();
					im::CloseCurrentPopup();
				}
				im::SameLine();
				if(im::Button("Cancel", ImVec2(120, 0))) {
					im::CloseCurrentPopup();
				}
			}
			im::EndPopup();
		}
	}

	im::SetNextItemWidth(width);
	if(im::InputInt("Z-Priority", &af->priority, 0, 0)) {
		markModified();
	}
	im::SetNextItemWidth(width);
	if(im::InputInt("Loop N times", &af->loopCount, 0, 0)) {
		markModified();
	}
	im::SameLine(0,20); im::SetNextItemWidth(width);
	if(im::InputInt("End of loop", &af->loopEnd, 0, 0)) {
		markModified();
	}
	if(im::InputInt("Duration", &af->duration, 1, 0)) {
		markModified();
	}

	im::Separator();

	// Copy/paste buttons for transform section
	static Frame_AF copiedTransform = {};
	if(im::SmallButton("Copy transforms")) {
		copiedTransform.offset_x = af->offset_x;
		copiedTransform.offset_y = af->offset_y;
		copiedTransform.blend_mode = af->blend_mode;
		memcpy(copiedTransform.rgba, af->rgba, sizeof(af->rgba));
		memcpy(copiedTransform.rotation, af->rotation, sizeof(af->rotation));
		memcpy(copiedTransform.scale, af->scale, sizeof(af->scale));
		copiedTransform.AFRT = af->AFRT;
		copiedTransform.interpolationType = af->interpolationType;
	}
	if(im::IsItemHovered()) Tooltip("Copy offset, rotation, scale, color, blend mode, and interpolation");
	im::SameLine();
	if(im::SmallButton("Paste transforms")) {
		af->offset_x = copiedTransform.offset_x;
		af->offset_y = copiedTransform.offset_y;
		af->blend_mode = copiedTransform.blend_mode;
		memcpy(af->rgba, copiedTransform.rgba, sizeof(af->rgba));
		memcpy(af->rotation, copiedTransform.rotation, sizeof(af->rotation));
		memcpy(af->scale, copiedTransform.scale, sizeof(af->scale));
		af->AFRT = copiedTransform.AFRT;
		af->interpolationType = copiedTransform.interpolationType;
		markModified();
	}
	if(im::IsItemHovered()) Tooltip("Paste offset, rotation, scale, color, blend mode, and interpolation");

	if(im::Combo("Interpolation", &af->interpolationType, interpolationList, IM_ARRAYSIZE(interpolationList))) {
		markModified();
	}

	im::SetNextItemWidth(width);
	if(im::DragInt("X", &af->offset_x)) {
		markModified();
	}
	im::SameLine();
	im::SetNextItemWidth(width);
	if(im::DragInt("Y", &af->offset_y)) {
		markModified();
	}

	int mode = af->blend_mode-1;
	if(mode < 1)
		mode = 0;
	if (im::Combo("Blend Mode", &mode, "Normal\0Additive\0Subtractive\0"))
	{
		af->blend_mode=mode+1;
		markModified();
	}
	if(im::ColorEdit4("Color", af->rgba)) {
		markModified();
	}

	if(im::DragFloat3("Rot XYZ", af->rotation, 0.005)) {
		markModified();
	}
	if(im::DragFloat2("Scale", af->scale, 0.1)) {
		markModified();
	}
	if(im::Checkbox("Rotation keeps scale set by EF", &af->AFRT)) {
		markModified();
	}

}