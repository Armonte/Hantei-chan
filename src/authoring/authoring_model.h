#ifndef AUTHORING_MODEL_H_GUARD
#define AUTHORING_MODEL_H_GUARD
// [authoring] The pure half of Authoring Mode (docs/HANTEI_AUTHORING_MODE.md §5, §6.3 H6), unit-tested by
// tests/authoring_model_test.cpp:
//   * the roster: the static mirror (roster_mirror.h) or the game's LinkRoster, with the TAG / TEAM eligibility;
//   * a match setup <-> LinkMatchSetup bytes <-> 128 lower-case hex digits (PCHOST_MBAACC_AUTHORING_SETUP);
//   * setup validation (§3.4.2 rules 1-8, as far as they can be decided without the game);
//   * the characters to open (duo -> File2 too) and their .txt paths;
//   * the launch environment (§3.7);
//   * named saved setups + a recent list (a small ini file);
//   * the per-game lever tables (§8.12): which table interprets a LinkCaps.gameId.
#include "../game_link_proto.h"

#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace authoring {

namespace wire = gamelink::wire;

// ---- the roster ----
struct RosterChar {
	int chara = -1;
	int selector = -1;
	std::string name, file1, file2, fullName;
	uint8_t flags = 0;         // wire::kRoster*
	bool Duo() const { return (flags & wire::kRosterDuo) != 0; }
};
// The mirror, in grid order. modData = the game loads tag_mod data (the house install: 0002.p moved aside).
std::vector<RosterChar> MirrorRoster(bool modData);
// The game's roster (every page received, in order).
std::vector<RosterChar> RosterFromLink(const std::vector<wire::Roster>& pages);
const RosterChar* FindChara(const std::vector<RosterChar>& r, int chara);
const RosterChar* FindFile(const std::vector<RosterChar>& r, const std::string& file1);

enum class Mode : uint8_t { Versus = 0, Tag = 1, Team = 2 };
const char* ModeName(Mode m);
// Why a character may not be picked in a mode ("" = allowed). Versus allows everything.
std::string BanReason(const RosterChar& c, Mode m);

// ---- the setup ----
struct SlotPick { int chara = -1; int moon = 0; int palette = 0; bool Empty() const { return chara < 0; } };
struct Setup {
	Mode mode = Mode::Tag;
	uint8_t scene = (uint8_t)wire::Scene::Auto;
	bool assists = true, keepBgm = false, force = false, resetPos = false, hotOnly = false, tuningFirst = true;
	int stage = 0;               // 1..99, 0 keep, -1 random
	int koRule = 0xFF;           // 0 oneDown, 1 allDown, 0xFF the tuning's
	int timer = 0xFF;            // 0 infinite, 1/2/4, 0xFF the scene's default
	SlotPick slot[4];            // ENGINE slots: 0 P1 point, 1 P2 point, 2 P1 partner, 3 P2 partner
	uint8_t assist[2][5] = {};   // per side, per direction (5,2,6,4,8): 0 = tuning, k = kAssistMotionChoices[k-1]
	// editor-only (saved setups, not on the wire)
	std::string name;            // the saved setup's name
	std::string style;           // the style the setup picker chose ("" = leave global.ini alone)
	bool operator==(const Setup& o) const;
	bool operator!=(const Setup& o) const { return !(*this == o); }
};
// Engine slot of (side, member): member 0 = point (slot = side), 1 = partner (slot = side + 2).
inline int EngineSlot(int side, int member) { return side + 2 * member; }
const char* SlotRoleName(int slot);   // "P1 point", "P2 point", "P1 partner", "P2 partner"

wire::MatchSetup ToWire(const Setup& s);
Setup FromWire(const wire::MatchSetup& w);
std::string ToHex(const wire::MatchSetup& w);                 // 128 lower-case hex digits
bool FromHex(const std::string& hex, wire::MatchSetup& out);   // false on a bad length / digit
// One line: "TAG Tohno C3 + Sion C0 vs V.Sion C0 + Miyako F0, stage 16, assists"
std::string Describe(const Setup& s, const std::vector<RosterChar>& roster);

// ---- validation ----
struct ValidateContext {
	bool team4p = false;                                         // LinkCaps kCapTeam4P (unknown offline: false)
	bool trainingScene = true;                                   // kCapTrainingScene
	std::function<int(const std::string& file1)> paletteCount;   // -1 = unknown (not checked)
	std::function<bool(int stage)> stageExists;                  // null = not checked
	std::function<bool(const std::string& file1, int moon)> txtExists;   // null = not checked (the loose-file preflight)
};
struct Problem {
	int slot = -1;               // -1 = the whole setup
	int16_t status = 0;          // the wire::Status the game would answer
	std::string msg;
};
std::vector<Problem> Validate(const Setup& s, const std::vector<RosterChar>& roster, const ValidateContext& ctx);

// ---- characters to open ----
struct OpenTarget {
	int slot = -1;               // engine slot (the first one that uses it)
	std::string file;            // lower-case data-file name
	int moon = 0;
	std::string txtPath;         // <gamedir>\data\<file>_<moon>.txt
	bool point = false;
	bool second = false;         // File2 of a duo point (1v1)
};
std::vector<OpenTarget> FilesToOpen(const Setup& s, const std::vector<RosterChar>& roster, const std::string& gameDir);

// <gamedir>\data\<file1>.pal: the first dword, capped at 256. -1 when the file cannot be read.
int PaletteCountOf(const std::string& palPath);

// ---- launch (§3.7) ----
std::vector<std::pair<std::string, std::string>> LaunchEnv(const Setup& s);
// A CreateProcessW environment block: the parent's environment with `vars` replacing / adding (sorted, UTF-16,
// double-NUL terminated), as UTF-16 code units.
std::wstring BuildEnvBlock(const std::vector<std::pair<std::string, std::string>>& vars, const wchar_t* parentBlock);

// ---- saved setups ----
struct SetupLibrary {
	std::vector<Setup> named;    // by name
	std::vector<Setup> recent;   // newest first, capped
	int recentCap = 12;
	void Save(const Setup& s, const std::string& name);    // replace a same-named one
	bool Remove(const std::string& name);
	void PushRecent(const Setup& s);                       // dedup (same fields), newest first
	const Setup* Find(const std::string& name) const;
	std::string Serialize() const;
	bool Parse(const std::string& text);
};

// ---- per-game lever tables (§8.12) ----
struct GameTable {
	const char* gameId;          // "mbaacc"
	uint32_t leverTableHash;
	int leverCount;
	int perCharLeverCount;
};
// The table for a LinkCaps.gameId ("" = mbaacc, an early revision-1 DLL). nullptr = this editor has no table for it.
const GameTable* FindGameTable(const std::string& gameId);

} // namespace authoring

#endif
