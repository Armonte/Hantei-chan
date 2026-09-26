#ifndef TAG_LEVERS_H_GUARD
#define TAG_LEVERS_H_GUARD
// The MBAACC TAG tuning levers, as the Tag / Team panel needs them: key, group, kind, scope, range, default, choice
// names and help, plus the value parse/format rules and the built-in styles.
//
// COPIED FROM PovertyCaster pc-adapters/mbaacc/include/mbaacc/MbaaccTagTuning.hpp (main 5f043727, 2026-09-24):
// kLevers (+ the Tuning field defaults), kEntryStyleNames/kEntryAnchorNames/kKoRuleNames/kAssistEntryNames/
// kAssistSlotEntryNames, motionCode/packMotion/unpackMotion, parseLeverValue, formatLeverValue, kBuiltinStyles.
// THAT HEADER IS THE AUTHORITY; this is a mirror, because Hantei-chan does not build against PovertyCaster (the
// same arrangement as game_link_proto.h). Rows are in the same order (the wire order there). A lever added or
// changed there must be changed here: tests/tag_tuning_test.cpp compares every row, every default and every
// built-in style against the PovertyCaster header when it is on disk (PC_TAG_TUNING_HPP in CMakeLists.txt), and
// against the reference list in tag_tuning.sample.ini otherwise.
//
// Values are int32 like the header's leverGet/leverSet: Bool 0/1, Enum an index into `names`, Motion a packMotion
// value. Guide: docs/tag_research/TAG_TUNING_GUIDE.md. Panel: docs/HANTEI_TAG_PANEL.md.
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>

namespace tagtune {

enum class LeverKind : uint8_t { Bool, Int, Enum, Motion };
// Sim = changes the simulation (must match in netplay); SimBoot = applied once at the game's boot (parkX);
// Harness = the offline stress harness only; CharOnly = only in a [char.<file>] section.
enum class LeverScope : uint8_t { Sim, SimBoot, Harness, CharOnly };

inline constexpr const char* kEntryStyleNames[] = { "edge", "drop", "arc" };
inline constexpr const char* kEntryAnchorNames[] = { "camera", "point" };
inline constexpr const char* kKoRuleNames[] = { "oneDown", "allDown" };
inline constexpr const char* kAssistEntryNames[] = { "behind", "edge", "drop", "arc" };
inline constexpr const char* kAssistSlotEntryNames[] = { "default", "behind", "edge", "drop", "arc" };

struct Lever {
	const char* key;       // ini key (case-insensitive)
	const char* group;     // panel grouping (the header's group string)
	LeverKind kind;
	LeverScope scope;
	int32_t lo, hi;        // inclusive
	int32_t def;           // the tag::Tuning field initialiser
	const char* const* names;
	const char* help;
	bool perChar;          // may appear in [char.<file>]
};

// Same order as PovertyCaster kLevers. def = the Tuning{} value (entryX 0xA000 = 40960).
inline constexpr Lever kLevers[] = {
	{ "cooldownTicks", "Swap", LeverKind::Int, LeverScope::Sim, 1, 6000, 120, nullptr, "ticks in state 101 before the team can tag again (stock 120)", false },
	{ "guardSwapHit", "Guards", LeverKind::Bool, LeverScope::Sim, 0, 1, 1, nullptr, "S1: a hit in the 2-tick swap window cancels the swap (else tag-lock)", false },
	{ "exitTimeoutTicks", "Guards", LeverKind::Int, LeverScope::Sim, 0, 6000, 180, nullptr, "S2: force-park an exit still running this long after the swap (0 = off)", false },
	{ "regen", "Regen", LeverKind::Bool, LeverScope::Sim, 0, 1, 0, nullptr, "MBAC: the resting partner regains red health", false },
	{ "regenPerTick", "Regen", LeverKind::Int, LeverScope::Sim, 0, 11400, 1, nullptr, "health per tick while resting (MBAC 1)", false },
	{ "parkX", "Park", LeverKind::Int, LeverScope::SimBoot, -2000000, 2000000, -102400, nullptr, "resting partner x (byte patch at boot: restart to change)", false },
	{ "parkY", "Park", LeverKind::Int, LeverScope::Sim, -2000000, 2000000, 0, nullptr, "resting partner y", false },
	{ "pinTeamPoint", "Harness", LeverKind::Bool, LeverScope::Harness, 0, 1, 1, nullptr, "stress harness: pin each team's point's health (not netplay)", false },
	{ "cancelWindowTicks", "Tag-in cancel", LeverKind::Int, LeverScope::Sim, -1, 255, -1, nullptr, "tag-in actionable from this tick (-1 = the pattern's own flags)", true },
	{ "cancelNormals", "Tag-in cancel", LeverKind::Bool, LeverScope::Sim, 0, 1, 1, nullptr, "window allows normals / jumps / guard / walk", true },
	{ "cancelSpecials", "Tag-in cancel", LeverKind::Bool, LeverScope::Sim, 0, 1, 1, nullptr, "window allows command-list specials / supers", true },
	{ "airTagOut", "Air tag", LeverKind::Bool, LeverScope::Sim, 0, 1, 0, nullptr, "22D while airborne; the outgoing starts at its lift-off frame", true },
	{ "airTagIn", "Air tag", LeverKind::Bool, LeverScope::Sim, 0, 1, 0, nullptr, "the incoming enters airborne (entryStyle drop / arc)", true },
	{ "entryStyle", "Entry", LeverKind::Enum, LeverScope::Sim, 0, 2, 0, kEntryStyleNames, "edge = stock dash-in; drop / arc need airTagIn", true },
	{ "entryX", "Entry", LeverKind::Int, LeverScope::Sim, 0, 200000, 0xA000, nullptr, "edge / arc start: distance from the camera centre (stock 40960)", true },
	{ "dropX", "Entry", LeverKind::Int, LeverScope::Sim, -200000, 200000, 16384, nullptr, "drop start: distance from the camera centre, own side", true },
	{ "entryY", "Entry", LeverKind::Int, LeverScope::Sim, -200000, 0, -40000, nullptr, "drop start height (negative = up)", true },
	{ "arcY", "Entry", LeverKind::Int, LeverScope::Sim, -200000, 0, 0, nullptr, "arc start height", true },
	{ "arcVelX", "Entry", LeverKind::Int, LeverScope::Sim, -10000, 10000, 900, nullptr, "arc forward speed", true },
	{ "arcVelY", "Entry", LeverKind::Int, LeverScope::Sim, -10000, 10000, -2800, nullptr, "arc initial vertical speed (negative = up)", true },
	{ "dropVelY", "Entry", LeverKind::Int, LeverScope::Sim, -10000, 10000, 0, nullptr, "drop initial vertical speed (positive = down)", true },
	{ "entryGravity", "Entry", LeverKind::Int, LeverScope::Sim, 0, 2000, 150, nullptr, "air entries: y acceleration per tick", true },
	{ "entryAnchor", "Entry", LeverKind::Enum, LeverScope::Sim, 0, 1, 0, kEntryAnchorNames, "x distances measured from: camera = screen edge, point = behind the point", true },
	{ "entryInvulnTicks", "Entry", LeverKind::Int, LeverScope::Sim, 0, 255, 0, nullptr, "strike + throw invulnerability ticks on entry (0 = none, stock)", true },
	{ "tagIn", "Patterns", LeverKind::Int, LeverScope::CharOnly, 0, 999, 0, nullptr, "[char] tag-in pattern number (0 = the TeamChangeData table)", true },
	{ "tagOut", "Patterns", LeverKind::Int, LeverScope::CharOnly, 0, 999, 0, nullptr, "[char] tag-out pattern number (0 = the TeamChangeData table)", true },
	{ "koRule", "KO rule", LeverKind::Enum, LeverScope::Sim, 0, 1, 0, kKoRuleNames, "oneDown = the point's KO ends the round; allDown = forced tag-in", false },
	{ "forcedEntryDelayTicks", "KO rule", LeverKind::Int, LeverScope::Sim, 0, 600, 60, nullptr, "allDown: ticks from the point's KO to the partner's entry", false },
	{ "swapRedKeepPct", "Entry", LeverKind::Int, LeverScope::Sim, 0, 100, 50, nullptr, "% of the incoming's red-health gap kept at the swap (stock 50)", true },
	{ "assistEnabled", "Assist", LeverKind::Bool, LeverScope::Sim, 0, 1, 0, nullptr, "the assist button (FN1) calls the partner in for one move", false },
	{ "assistEntry", "Assist", LeverKind::Enum, LeverScope::Sim, 0, 3, 0, kAssistEntryNames, "default entry: behind the point / screen edge / drop from above / jump arc", true },
	{ "assistOffsetX", "Assist", LeverKind::Int, LeverScope::Sim, 0, 200000, 36000, nullptr, "behind / drop: start distance behind the point", true },
	{ "assistEntryTicks", "Assist", LeverKind::Int, LeverScope::Sim, 0, 120, 8, nullptr, "behind / edge: entry ticks before the action (drop / arc: on landing)", true },
	{ "assistCooldownTicks", "Assist", LeverKind::Int, LeverScope::Sim, 0, 6000, 180, nullptr, "ticks after the assist parks before the next call", true },
	{ "assistMaxTicks", "Assist", LeverKind::Int, LeverScope::Sim, 1, 6000, 150, nullptr, "the move is cut off after this many ticks", true },
	{ "assistDamagePct", "Assist", LeverKind::Int, LeverScope::Sim, 0, 1000, 100, nullptr, "% of a hit's damage the assist takes (100 = stock)", true },
	{ "assistMeter", "Assist", LeverKind::Bool, LeverScope::Sim, 0, 1, 1, nullptr, "meter moves allowed as assists (paid from the point's meter)", false },
	{ "assistPromote", "Assist", LeverKind::Bool, LeverScope::Sim, 0, 1, 1, nullptr, "22D during an assist promotes the assist to point", false },
	{ "assistFromBlockstun", "Assist", LeverKind::Bool, LeverScope::Sim, 0, 1, 0, nullptr, "the assist may be called from blockstun (alpha-counter style)", false },
	{ "assist.5.entry", "Assist slots", LeverKind::Enum, LeverScope::Sim, 0, 4, 0, kAssistSlotEntryNames, "5+FN1 entry (default = slot 5's, then assistEntry)", true },
	{ "assist.2.entry", "Assist slots", LeverKind::Enum, LeverScope::Sim, 0, 4, 0, kAssistSlotEntryNames, "2+FN1 entry (default = slot 5's, then assistEntry)", true },
	{ "assist.6.entry", "Assist slots", LeverKind::Enum, LeverScope::Sim, 0, 4, 0, kAssistSlotEntryNames, "6+FN1 entry (default = slot 5's, then assistEntry)", true },
	{ "assist.4.entry", "Assist slots", LeverKind::Enum, LeverScope::Sim, 0, 4, 0, kAssistSlotEntryNames, "4+FN1 entry (default = slot 5's, then assistEntry)", true },
	{ "assist.8.entry", "Assist slots", LeverKind::Enum, LeverScope::Sim, 0, 4, 0, kAssistSlotEntryNames, "8+FN1 entry (default = slot 5's, then assistEntry)", true },
	{ "assist.5.pattern", "Assist slots", LeverKind::Int, LeverScope::CharOnly, 0, 999, 0, nullptr, "[char] 5+FN1: play this hantei pattern directly (0 = unset)", true },
	{ "assist.5.command", "Assist slots", LeverKind::Int, LeverScope::CharOnly, -1, 999, -1, nullptr, "[char] 5+FN1: the _c.txt command id whose pattern plays (-1 = unset)", true },
	{ "assist.5.motion", "Assist slots", LeverKind::Motion, LeverScope::CharOnly, 0, 0x3FFFFFFF, 0, nullptr, "[char] 5+FN1: feed this input through the command interpreter (e.g. 236C)", true },
	{ "assist.2.pattern", "Assist slots", LeverKind::Int, LeverScope::CharOnly, 0, 999, 0, nullptr, "[char] 2+FN1: play this hantei pattern directly (0 = unset)", true },
	{ "assist.2.command", "Assist slots", LeverKind::Int, LeverScope::CharOnly, -1, 999, -1, nullptr, "[char] 2+FN1: the _c.txt command id whose pattern plays (-1 = unset)", true },
	{ "assist.2.motion", "Assist slots", LeverKind::Motion, LeverScope::CharOnly, 0, 0x3FFFFFFF, 0, nullptr, "[char] 2+FN1: feed this input through the command interpreter (e.g. 236C)", true },
	{ "assist.6.pattern", "Assist slots", LeverKind::Int, LeverScope::CharOnly, 0, 999, 0, nullptr, "[char] 6+FN1: play this hantei pattern directly (0 = unset)", true },
	{ "assist.6.command", "Assist slots", LeverKind::Int, LeverScope::CharOnly, -1, 999, -1, nullptr, "[char] 6+FN1: the _c.txt command id whose pattern plays (-1 = unset)", true },
	{ "assist.6.motion", "Assist slots", LeverKind::Motion, LeverScope::CharOnly, 0, 0x3FFFFFFF, 0, nullptr, "[char] 6+FN1: feed this input through the command interpreter (e.g. 236C)", true },
	{ "assist.4.pattern", "Assist slots", LeverKind::Int, LeverScope::CharOnly, 0, 999, 0, nullptr, "[char] 4+FN1: play this hantei pattern directly (0 = unset)", true },
	{ "assist.4.command", "Assist slots", LeverKind::Int, LeverScope::CharOnly, -1, 999, -1, nullptr, "[char] 4+FN1: the _c.txt command id whose pattern plays (-1 = unset)", true },
	{ "assist.4.motion", "Assist slots", LeverKind::Motion, LeverScope::CharOnly, 0, 0x3FFFFFFF, 0, nullptr, "[char] 4+FN1: feed this input through the command interpreter (e.g. 236C)", true },
	{ "assist.8.pattern", "Assist slots", LeverKind::Int, LeverScope::CharOnly, 0, 999, 0, nullptr, "[char] 8+FN1: play this hantei pattern directly (0 = unset)", true },
	{ "assist.8.command", "Assist slots", LeverKind::Int, LeverScope::CharOnly, -1, 999, -1, nullptr, "[char] 8+FN1: the _c.txt command id whose pattern plays (-1 = unset)", true },
	{ "assist.8.motion", "Assist slots", LeverKind::Motion, LeverScope::CharOnly, 0, 0x3FFFFFFF, 0, nullptr, "[char] 8+FN1: feed this input through the command interpreter (e.g. 236C)", true },
};
inline constexpr size_t kLeverCount = sizeof(kLevers) / sizeof(kLevers[0]);
static_assert(kLeverCount == 59, "lever count changed: re-check the mirror against MbaaccTagTuning.hpp");

using Values = std::array<int32_t, kLeverCount>;
inline Values DefaultValues() { Values v{}; for (size_t i = 0; i < kLeverCount; ++i) v[i] = kLevers[i].def; return v; }

// ---- [authoring] the lever table hash (docs/HANTEI_AUTHORING_MODE.md §3.5): FNV-1a 32 over, per lever in table order,
// the key bytes + 0x00, kind (u8), scope (u8), lo / hi / default (i32 LE each), perChar (u8). pchost.dll reports its own in
// LinkCaps.leverTableHash / LinkTuningGlobal.leverTableHash; on a mismatch the editor never writes a lever it cannot name.
inline uint32_t LeverTableHash(const Lever* table, size_t count)
{
	uint32_t h = 2166136261u;
	auto byte = [&h](uint8_t b) { h ^= b; h *= 16777619u; };
	auto i32 = [&byte](int32_t v) { const uint32_t u = (uint32_t)v; for (int k = 0; k < 4; ++k) byte((uint8_t)(u >> (8 * k))); };
	for (size_t i = 0; i < count; ++i) {
		const Lever& l = table[i];
		for (const char* p = l.key; *p; ++p) byte((uint8_t)*p);
		byte(0);
		byte((uint8_t)l.kind);
		byte((uint8_t)l.scope);
		i32(l.lo); i32(l.hi); i32(l.def);
		byte(l.perChar ? 1 : 0);
	}
	return h;
}
inline uint32_t LeverTableHash() { return LeverTableHash(kLevers, kLeverCount); }
inline int PerCharLeverCount() { int n = 0; for (const Lever& l : kLevers) n += l.perChar ? 1 : 0; return n; }

// Directional assist slots in the header's order: slot index 0..4 = directions 5, 2, 6, 4, 8.
inline constexpr int kAssistDirs[5] = { 5, 2, 6, 4, 8 };

inline bool ieq(std::string_view a, std::string_view b)
{
	if (a.size() != b.size()) return false;
	for (size_t k = 0; k < a.size(); ++k) {
		char x = a[k], y = b[k];
		if (x >= 'A' && x <= 'Z') x = (char)(x - 'A' + 'a');
		if (y >= 'A' && y <= 'Z') y = (char)(y - 'A' + 'a');
		if (x != y) return false;
	}
	return true;
}

inline int FindLever(std::string_view key)
{
	for (size_t i = 0; i < kLeverCount; ++i) if (ieq(kLevers[i].key, key)) return (int)i;
	return -1;
}

// ---- motion text <-> int32 (the header's packMotion: 5 bits a character, 6 characters) ----
inline int MotionCode(char c)
{
	if (c >= '0' && c <= '9') return 1 + (c - '0');
	if (c >= 'a' && c <= 'f') c = (char)(c - 'a' + 'A');
	if (c >= 'A' && c <= 'F') return 11 + (c - 'A');
	if (c == '+') return 17;
	if (c == 'V' || c == 'v') return 18;
	return 0;
}
inline char MotionChar(int code)
{
	if (code >= 1 && code <= 10) return (char)('0' + code - 1);
	if (code >= 11 && code <= 16) return (char)('A' + code - 11);
	return code == 17 ? '+' : code == 18 ? 'V' : 0;
}
inline bool PackMotion(std::string_view s, int32_t& out)
{
	uint32_t v = 0;
	if (s.empty() || s.size() > 6) return false;
	for (size_t k = 0; k < s.size(); ++k) {
		const int c = MotionCode(s[k]);
		if (!c) return false;
		v |= (uint32_t)c << (5 * k);
	}
	out = (int32_t)v;
	return true;
}
inline std::string UnpackMotion(int32_t v)
{
	std::string s;
	for (int k = 0; k < 6; ++k) { const char c = MotionChar(((uint32_t)v >> (5 * k)) & 31); if (!c) break; s += c; }
	return s;
}

// The header's parseLeverValue, verbatim in behaviour. False = the game skips the key and keeps the previous value.
inline bool ParseLeverValue(const Lever& l, std::string_view s, int32_t& out)
{
	while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.remove_prefix(1);
	while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r')) s.remove_suffix(1);
	if (s.empty()) return false;
	if (l.kind == LeverKind::Bool) {
		for (const char* t : { "1", "true", "on", "yes" }) if (ieq(s, t)) { out = 1; return true; }
		for (const char* f : { "0", "false", "off", "no" }) if (ieq(s, f)) { out = 0; return true; }
		return false;
	}
	if (l.kind == LeverKind::Motion) {
		if (s == "0" || ieq(s, "none") || ieq(s, "-")) { out = 0; return true; }
		return PackMotion(s, out);
	}
	if (l.kind == LeverKind::Enum && l.names)
		for (int32_t k = l.lo; k <= l.hi; ++k) if (ieq(s, l.names[k])) { out = k; return true; }
	if (l.names == kAssistEntryNames && ieq(s, "above")) { out = 2; return true; }
	if (l.names == kAssistSlotEntryNames && ieq(s, "above")) { out = 3; return true; }
	if (l.names == kKoRuleNames) {
		if (ieq(s, "mbac")) { out = 0; return true; }
		if (ieq(s, "mbaa") || ieq(s, "marvel")) { out = 1; return true; }
		if (ieq(s, "2")) { out = 1; return true; }
	}
	std::string tmp(s);
	char* end = nullptr;
	const long v = std::strtol(tmp.c_str(), &end, 10);
	if (end == tmp.c_str() || *end || v < l.lo || v > l.hi) return false;
	out = (int32_t)v;
	return true;
}

inline std::string FormatLeverValue(const Lever& l, int32_t v)
{
	if (l.kind == LeverKind::Motion) return v ? UnpackMotion(v) : std::string("0");
	if (l.kind == LeverKind::Enum && l.names && v >= l.lo && v <= l.hi) return l.names[v];
	return std::to_string(v);
}

// Built-in styles (header kBuiltinStyles). An ini [style.<name>] of the same name replaces one.
struct BuiltinStyle { const char* name; const char* desc; const char* body; };
inline constexpr BuiltinStyle kBuiltinStyles[] = {
	{ "Stock", "the defaults: stock MBAC swap + the tag-lock guards, no regen", "" },
	{ "Classic", "MBAC: 22D ground tag, 120f cooldown, dash-in from the edge, reserve regen 1/f",
	  "regen=1\nregenPerTick=1\n" },
	{ "ActiveTag", "MvCI-style active switch: air and ground tags, early tag-in cancels, short cooldown",
	  "regen=1\nairTagOut=1\ncancelWindowTicks=4\ncooldownTicks=60\n" },
	{ "Chaos", "Marvel chaos: air tags, jump-in arc entry, tag-in actionable at once, minimal cooldown",
	  "regen=1\nregenPerTick=2\nairTagOut=1\nairTagIn=1\nentryStyle=arc\ncancelWindowTicks=0\ncooldownTicks=1\n" },
	{ "DropIn", "drop-from-top entry, otherwise Classic", "regen=1\nairTagIn=1\nentryStyle=drop\n" },
	{ "Assist", "point + assists (Skullgirls/BBTag): FN1 calls the partner behind you for its light special",
	  "regen=1\nassistEnabled=1\n" },
	{ "Freestyle", "Chaos + assists: edge fly-in assists, promote on 22D, short assist cooldown",
	  "regen=1\nregenPerTick=2\nairTagOut=1\nairTagIn=1\nentryStyle=arc\ncancelWindowTicks=0\ncooldownTicks=1\n"
	  "assistEnabled=1\nassistEntry=edge\nassistEntryTicks=12\nassistCooldownTicks=90\n" },
};
inline const BuiltinStyle* FindBuiltinStyle(std::string_view name)
{
	for (const BuiltinStyle& s : kBuiltinStyles) if (ieq(s.name, name)) return &s;
	return nullptr;
}

// ---- TeamChangeData (PovertyCaster MbaaccTag.hpp kTeamChangeTable, main 5f043727) ----
// The tag-in / tag-out a character gets when its [char] section sets no tagIn / tagOut. mod = the pattern exists
// only in the tag_mod HA6 files; the game checks the loaded data and otherwise falls back (inFallback) or excludes
// the character from tag.
struct TeamChangeRow { const char* file; int16_t tagIn, tagOut; bool inMod, outMod; int16_t inFallback; };
inline constexpr TeamChangeRow kTeamChangeTable[] = {
	{ "sion", 241, 242, false, false, 0 },     { "arc", 235, 236, false, false, 0 },
	{ "ciel", 243, 244, false, false, 0 },     { "akiha", 177, 178, false, false, 0 },
	{ "kohaku", 188, 189, false, false, 0 },   { "shiki", 241, 242, false, false, 0 },
	{ "miyako", 241, 242, false, false, 0 },   { "warakia", 241, 242, false, false, 0 },
	{ "nero", 244, 245, false, false, 0 },     { "v_sion", 241, 242, false, false, 0 },
	{ "warc", 197, 198, false, false, 0 },     { "akaakiha", 241, 242, false, false, 0 },
	{ "m_hisui", 241, 242, false, false, 0 },  { "nanaya", 241, 242, false, false, 0 },
	{ "satsuki", 241, 242, false, false, 0 },  { "len", 241, 242, false, false, 0 },
	{ "neco", 241, 242, false, false, 0 },     { "aoko", 240, 241, false, false, 0 },
	{ "wlen", 241, 242, false, false, 0 },     { "nechaos", 241, 242, false, false, 0 },
	{ "kishima", 241, 242, false, false, 0 },  { "neco_p", 241, 242, false, false, 0 },
	{ "m_hisui_m", 241, 242, false, false, 0 }, { "m_hisui_p", 241, 242, false, false, 0 },
	{ "kohaku_m", 188, 189, false, false, 0 }, { "hisui", 243, 242, true, false, 241 },
	{ "ries", 241, 242, true, true, 0 },       { "roa", 235, 236, true, true, 0 },
	{ "hermes", 232, 233, true, true, 0 },     { "ryougi", 241, 242, true, true, 0 },
	{ "p_ciel", 237, 238, true, true, 0 },     { "s_akiha", 241, 242, true, true, 0 },
	{ "p_arc", 237, 238, true, true, 0 },
};
inline const TeamChangeRow* FindTeamChangeRow(std::string_view file)
{
	for (const TeamChangeRow& r : kTeamChangeTable) if (ieq(r.file, file)) return &r;
	return nullptr;
}

} // namespace tagtune

#endif
