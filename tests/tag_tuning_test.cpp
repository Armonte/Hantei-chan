// Tag / Team panel unit tests (docs/HANTEI_TAG_PANEL.md §6):
//   1. tag_tuning.ini round trip: an unedited load/save is byte-identical (LF and CRLF), and each edit touches only
//      the key it edits (the rest of the file is byte-identical around it).
//   2. The lever table (src/tag_tuning/tag_levers.h) against the reference list in tag_tuning.sample.ini, and, when
//      the PovertyCaster header is on disk (PC_TAG_TUNING_HPP), row by row against MbaaccTagTuning.hpp itself plus the
//      resolution of the sample and of edited files through both parsers.
//   3. Assist slots: slot resolution, motion validation, and the default assist per character file against the
//      TAG_TUNING_GUIDE.md §3.1 table (fixtures: shiki C/F/H; with --data <game data dir> every row whose _c.txt is
//      on disk).
//
//   tag_tuning_test [--fixtures <dir>] [--data <game data dir>]
#include "../src/tag_tuning/tag_assist.h"
#include "../src/tag_tuning/tag_ini.h"

#ifdef HAVE_PC_TAG_TUNING
#include "mbaacc/MbaaccTagTuning.hpp"
#endif

#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace tagtune;

static int g_fail = 0, g_pass = 0;
#define CHECK(c) do { if (c) ++g_pass; else { ++g_fail; std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)
#define CHECKM(c, m) do { if (c) ++g_pass; else { ++g_fail; std::printf("FAIL %s:%d: %s -- %s\n", __FILE__, __LINE__, #c, std::string(m).c_str()); } } while (0)

static bool ReadAll(const std::string& p, std::string& out)
{
	std::ifstream f(p, std::ios::binary);
	if (!f) return false;
	std::ostringstream s;
	s << f.rdbuf();
	out = s.str();
	return true;
}

// The edit touched only [pos, pos+len) of `before`: common prefix + common suffix cover the rest.
static void DiffSpan(const std::string& a, const std::string& b, size_t& pre, size_t& sufA, size_t& sufB)
{
	pre = 0;
	while (pre < a.size() && pre < b.size() && a[pre] == b[pre]) ++pre;
	size_t s = 0;
	while (s < a.size() - pre && s < b.size() - pre && a[a.size() - 1 - s] == b[b.size() - 1 - s]) ++s;
	sufA = a.size() - s;
	sufB = b.size() - s;
}
static std::string Changed(const std::string& a, const std::string& b)   // "removed -> added"
{
	size_t pre, ea, eb;
	DiffSpan(a, b, pre, ea, eb);
	return a.substr(pre, ea - pre) + " -> " + b.substr(pre, eb - pre);
}

static std::string ToCrlf(const std::string& s)
{
	std::string o;
	for (char c : s) { if (c == '\n') o += '\r'; o += c; }
	return o;
}

static void TestRoundTrip(const std::string& sample)
{
	for (int crlf = 0; crlf < 2; ++crlf) {
		const std::string raw = crlf ? ToCrlf(sample) : sample;
		TagIni ini;
		ini.LoadText(raw);
		CHECK(ini.Text() == raw);
		CHECK(!ini.IsDirty());
		const std::string nl = crlf ? "\r\n" : "\n";

		// a value in place: koRule=oneDown -> allDown touches only the value
		TagIni a = ini;
		SetTuningLever(a, FindLever("koRule"), 1);
		CHECKM(Changed(raw, a.Text()) == "one -> all", Changed(raw, a.Text()));
		std::string v;
		CHECK(a.Get(SecKind::Tuning, "", "koRule", v) && v == "allDown");
		CHECK(a.IsDirty());
		// setting the same value back restores the exact bytes
		SetTuningLever(a, FindLever("koRule"), 0);
		CHECK(a.Text() == raw);

		// a new [tuning] key is one inserted line right after the last [tuning] key line
		TagIni b = ini;
		SetTuningLever(b, FindLever("cancelWindowTicks"), 4);
		CHECKM(Changed(raw, b.Text()) == " -> cancelWindowTicks=4" + nl, Changed(raw, b.Text()));
		CHECK(b.Text().find("koRule=oneDown" + nl + "cancelWindowTicks=4" + nl) != std::string::npos);
		ClearTuningLever(b, FindLever("cancelWindowTicks"));
		CHECK(b.Text() == raw);

		// a style key edit, inside [style.Chaos] only
		TagIni c = ini;
		c.Set(SecKind::Style, "chaos", "cooldownTicks", "30");
		CHECKM(Changed(raw, c.Text()) == "1 -> 30", Changed(raw, c.Text()));
		CHECK(c.Text().find("[style.Chaos]") != std::string::npos);
		Values cv;
		std::vector<Warning> w;
		StyleBase(c, "Chaos", cv, w);
		CHECK(cv[FindLever("cooldownTicks")] == 30);

		// a new [char.*] section at the end, then removed: back to the original bytes
		TagIni d = ini;
		SetCharLever(d, "Sion", FindLever("entryX"), 12000);
		CHECK(d.Text().compare(0, raw.size(), raw) == 0);   // only appended
		CHECK(d.Text().find("[char.sion]" + nl + "entryX=12000" + nl) != std::string::npos);
		SetCharLever(d, "sion", FindLever("entryInvulnTicks"), 8);
		CHECK(d.Text().find("entryX=12000" + nl + "entryInvulnTicks=8" + nl) != std::string::npos);
		d.RemoveSection(SecKind::Char, "SION");
		CHECKM(d.Text() == raw, Changed(raw, d.Text()));
	}

	// trailing comments and spacing survive a value edit; unknown keys are kept and untouched
	{
		const std::string raw = "; head\r\n[tuning]\r\nactive_style = Chaos\r\ncancelWindowTicks =  4   ; early\r\nmyNote=keep me\r\n"
		                        "[char.miyako]\r\ncancelWindowTicks=6 # hers\r\n[char.MIYAKO]\r\nentryStyle=drop\r\n";
		TagIni ini;
		ini.LoadText(raw);
		SetTuningLever(ini, FindLever("cancelWindowTicks"), 10);
		CHECKM(Changed(raw, ini.Text()) == "4 -> 10", Changed(raw, ini.Text()));
		CHECK(ini.Text().find("cancelWindowTicks =  10   ; early") != std::string::npos);
		CHECK(ini.Text().find("myNote=keep me") != std::string::npos);
		CHECK(ini.ActiveStyle() == "Chaos");
		// both [char.miyako] instances merge, as the game reads them
		const Resolved r = Resolve(ini);
		const Values m = ForChar(ini, r.global, "miyako");
		CHECK(m[FindLever("cancelWindowTicks")] == 6);
		CHECK(m[FindLever("entryStyle")] == 1);
		// a char edit changes the last occurrence
		SetCharLever(ini, "miyako", FindLever("cancelWindowTicks"), 7);
		std::string v;
		CHECK(ini.Get(SecKind::Char, "Miyako", "cancelWindowTicks", v) && v == "7");
		CHECK(ini.Text().find("cancelWindowTicks=7 # hers") != std::string::npos);
		// the unknown key is a warning the game logs; an edit adds no new warning
		const std::vector<Warning> before = AllWarnings(ini);
		CHECK(before.size() == 1);
		CHECK(NewWarnings(before, AllWarnings(ini)).empty());
		ini.Set(SecKind::Tuning, "", "cooldownTicks", "0");   // out of range 1..6000
		CHECK(NewWarnings(before, AllWarnings(ini)).size() == 1);
	}

	// no trailing newline, empty file
	{
		TagIni ini;
		ini.LoadText("[tuning]\nregen=1");
		SetTuningLever(ini, FindLever("cooldownTicks"), 60);
		CHECKM(ini.Text() == "[tuning]\nregen=1\ncooldownTicks=60", ini.Text());
		TagIni e;
		e.LoadText("");
		SetTuningLever(e, FindLever("regen"), 1);
		CHECKM(e.Text() == "[tuning]\nregen=1\n", e.Text());
		CHECK(Resolve(e).global[FindLever("regen")] == 1);
	}

	// Save as style / select style (F3 semantics: the style becomes active, [tuning] lever keys go, others stay)
	{
		TagIni ini;
		ini.LoadText(sample);
		Resolved r = Resolve(ini);
		Values live = r.global;
		live[FindLever("cooldownTicks")] = 45;
		SaveAsStyle(ini, "Mine", live);
		CHECK(ini.ActiveStyle() == "Mine");
		const Resolved r2 = Resolve(ini);
		CHECK(r2.global == live);
		CHECK(r2.warnings.empty());
		std::string v;
		CHECK(!ini.Get(SecKind::Tuning, "", "koRule", v));
		CHECK(ini.Get(SecKind::Style, "mine", "cooldownTicks", v) && v == "45");
		SelectStyle(ini, "Chaos", true);
		CHECK(Resolve(ini).style == "Chaos");
		CHECK(Resolve(ini).global[FindLever("entryStyle")] == 2);
	}
}

// The sample's commented reference list: ";   key = default   [group] range (tag) — help".
static void TestLeversVsSample(const std::string& sample)
{
	std::istringstream in(sample);
	std::string ln;
	int rows = 0;
	std::vector<bool> seen(kLeverCount, false);
	while (std::getline(in, ln)) {
		if (ln.rfind(";   ", 0) != 0) continue;
		const size_t eq = ln.find(" = "), br = ln.find("   [");
		if (eq == std::string::npos || br == std::string::npos || br < eq) continue;
		const std::string key = ln.substr(4, eq - 4);
		const int li = FindLever(key);
		if (key.find('<') != std::string::npos) continue;   // assist.<d>.* lines
		CHECKM(li >= 0, "sample lists an unknown lever " + key);
		if (li < 0) continue;
		const Lever& l = kLevers[li];
		seen[li] = true;
		++rows;
		CHECKM(ln.substr(eq + 3, br - eq - 3) == FormatLeverValue(l, l.def), key + " default");
		const size_t close = ln.find(']', br);
		CHECKM(ln.substr(br + 4, close - br - 4) == l.group, key + " group");
		std::string range;
		if (l.kind == LeverKind::Enum) for (int k = l.lo; k <= l.hi; ++k) range += std::string(k == l.lo ? "" : "|") + l.names[k];
		else if (l.kind == LeverKind::Bool) range = "0|1";
		else range = std::to_string(l.lo) + ".." + std::to_string(l.hi);
		CHECKM(ln.compare(close + 2, range.size(), range) == 0, key + " range " + range);
		const bool boot = ln.find("(boot only)") != std::string::npos, harness = ln.find("(harness only)") != std::string::npos,
		           charOnly = ln.find("([char.*] only)") != std::string::npos;
		CHECKM(boot == (l.scope == LeverScope::SimBoot), key + " boot");
		CHECKM(harness == (l.scope == LeverScope::Harness), key + " harness");
		CHECKM(charOnly == (l.scope == LeverScope::CharOnly), key + " char-only");
		// help wording in the hand-written sample lags the header in places: reported, not failed
		if (ln.find(l.help) == std::string::npos && l.scope != LeverScope::CharOnly)
			std::printf("NOTE sample help differs for %s (the header is the authority)\n", key.c_str());
	}
	// every non-slot lever is listed there (the slot actions are described by the assist.<d>.* lines)
	for (size_t i = 0; i < kLeverCount; ++i)
		if (std::strncmp(kLevers[i].key, "assist.", 7) != 0 && !seen[i])
			std::printf("NOTE the sample's reference list does not list %s yet (sample drift, header is the authority)\n", kLevers[i].key);
	CHECK(rows >= 38);
	// the sample resolves without warnings: Classic + koRule
	TagIni ini;
	ini.LoadText(sample);
	const Resolved r = Resolve(ini);
	CHECK(r.warnings.empty());
	CHECK(r.style == "Classic");
	CHECK(r.global[FindLever("regen")] == 1);
	CHECK(StyleNames(ini).size() == 7);   // the built-ins; the sample's sections repeat them
	// the sample's [style.*] repeat the built-ins exactly
	for (const BuiltinStyle& b : kBuiltinStyles) {
		if (!ini.HasSection(SecKind::Style, b.name)) continue;
		Values fromIni, fromCode;
		std::vector<Warning> w;
		StyleBase(ini, b.name, fromIni, w);
		TagIni empty;
		StyleBase(empty, b.name, fromCode, w);
		CHECKM(fromIni == fromCode, b.name);
	}
}

#ifdef HAVE_PC_TAG_TUNING
namespace pct = mbaacc::tag;
static Values FromPc(const pct::Tuning& t)
{
	Values v{};
	for (size_t i = 0; i < pct::kLeverCount; ++i) v[i] = pct::leverGet(t, pct::kLevers[i]);
	return v;
}
static void TestAgainstPovertyCaster(const std::string& sample)
{
	CHECK(pct::kLeverCount == kLeverCount);
	const pct::Tuning d;
	size_t sim = 0;
	for (size_t i = 0; i < kLeverCount && i < pct::kLeverCount; ++i) {
		const pct::Lever& p = pct::kLevers[i];
		const Lever& m = kLevers[i];
		const std::string k = p.key;
		CHECKM(k == m.key, k + " key/order");
		CHECKM(std::string(p.group) == m.group, k + " group");
		CHECKM((int)p.kind == (int)m.kind, k + " kind");
		CHECKM((int)p.scope == (int)m.scope, k + " scope");
		CHECKM(p.lo == m.lo && p.hi == m.hi, k + " range");
		CHECKM(pct::leverGet(d, p) == m.def, k + " default");
		CHECKM(std::string(p.help) == m.help, k + " help");
		CHECKM(p.perChar == m.perChar, k + " perChar");
		CHECKM((p.names == nullptr) == (m.names == nullptr), k + " names");
		if (p.names && m.names) for (int n = p.lo; n <= p.hi; ++n) CHECKM(std::string(p.names[n]) == m.names[n], k + " name");
		sim += m.scope == LeverScope::Sim || m.scope == LeverScope::SimBoot;
	}
	CHECK(sim == pct::tuningBlockFields());
	CHECK(sizeof(pct::kBuiltinStyles) / sizeof(pct::kBuiltinStyles[0]) == sizeof(kBuiltinStyles) / sizeof(kBuiltinStyles[0]));
	for (size_t i = 0; i < sizeof(kBuiltinStyles) / sizeof(kBuiltinStyles[0]); ++i) {
		CHECK(std::string(pct::kBuiltinStyles[i].name) == kBuiltinStyles[i].name);
		CHECK(std::string(pct::kBuiltinStyles[i].body) == kBuiltinStyles[i].body);
	}
	// value parsing: every lever against a set of tricky inputs
	const char* inputs[] = { "0", "1", "-1", "on", "OFF", "yes", "2", "999", "1000", "-200000", "200001", "40960", " 5 ",
	                         "5x", "", "edge", "ARC", "point", "allDown", "mbaa", "marvel", "mbac", "above", "default",
	                         "236C", "2+a+b", "41236C", "4123698", "none", "-", "0V9", "12A", "0x10", "6000", "6001" };
	for (size_t i = 0; i < kLeverCount; ++i)
		for (const char* s : inputs) {
			int32_t a = 12345, b = 12345;
			const bool ra = pct::parseLeverValue(pct::kLevers[i], s, a), rb = ParseLeverValue(kLevers[i], s, b);
			CHECKM(ra == rb && (!ra || a == b), std::string(kLevers[i].key) + " parse '" + s + "'");
			if (ra) CHECK(pct::formatLeverValue(pct::kLevers[i], a) == FormatLeverValue(kLevers[i], b));
		}
	// resolution of the sample and of edited variants, global and per character, through both parsers
	std::vector<std::string> texts;
	texts.push_back(sample);
	{
		TagIni e; e.LoadText(sample);
		SetTuningLever(e, FindLever("cancelWindowTicks"), 4);
		SetCharLever(e, "Sion", FindLever("entryAnchor"), 1);
		SetCharLever(e, "sion", FindLever("assist.2.motion"), 0);
		e.Set(SecKind::Char, "sion", "assist.2.motion", "236C");
		e.Set(SecKind::Char, "shiki", "assist.5.command", "45");
		e.Set(SecKind::Char, "shiki", "assist.6.pattern", "455");
		e.Set(SecKind::Char, "shiki", "regen", "1");            // team-wide in a char section: warned, skipped
		e.Set(SecKind::Tuning, "", "tagIn", "250");             // char-only in [tuning]: warned, skipped
		e.Set(SecKind::Tuning, "", "koRule", "marvel");
		e.Set(SecKind::Tuning, "", "bogus", "1");
		texts.push_back(e.Text());
		SelectStyle(e, "Freestyle", false);
		texts.push_back(e.Text());
		e.Set(SecKind::Tuning, "", "active_style", "NoSuchStyle");
		texts.push_back(e.Text());
		texts.push_back(ToCrlf(e.Text()));
	}
	for (const std::string& t : texts) {
		const pct::TuningIni pi = pct::parseTuningIni(t);
		const pct::ResolvedTuning pr = pct::resolveTuning(pi);
		TagIni mi;
		mi.LoadText(t);
		const Resolved mr = Resolve(mi);
		CHECK(FromPc(pr.t) == mr.global);
		CHECK(pr.style == mr.style);
		std::vector<Warning> all = AllWarnings(mi);
		CHECKM(pr.warnings.size() == all.size(), std::to_string(pr.warnings.size()) + " vs " + std::to_string(all.size()));
		for (const char* f : { "sion", "SHIKI", "miyako", "nobody" })
			CHECK(FromPc(pct::tuningForChar(pr.t, pr.chars, f)) == ForChar(mi, mr.global, f));
		CHECK(pct::styleNames(pi) == StyleNames(mi));
	}
	std::printf("PovertyCaster header comparison: %zu levers, %zu files\n", kLeverCount, texts.size());
}
#endif

static void TestAssist(const std::string& fixtures, const std::string& dataDir)
{
	// motions
	int32_t p = 0;
	CHECK(PackMotion("236C", p) && UnpackMotion(p) == "236C");
	CHECK(PackMotion("2+a+b", p) && UnpackMotion(p) == "2+A+B");
	CHECK(ValidateMotion("236C").empty());
	CHECK(!ValidateMotion("4123698").empty());
	CHECK(!ValidateMotion("236X").empty());
	CHECK(!ValidateMotion("").empty());

	// slot resolution (resolveAssistSlot)
	Values v = DefaultValues();
	AssistAction a = ResolveAssistSlot(v, 2);
	CHECK(a.mode == AssistMode::Default && a.fromSlot == -1 && a.entry == 0);
	v[FindLever("assist.5.command")] = 45;
	v[FindLever("assist.5.entry")] = 3;   // drop (1 + 2)
	a = ResolveAssistSlot(v, 3);          // slot 4 empty: slot 5's action and entry
	CHECK(a.mode == AssistMode::Command && a.value == 45 && a.fromSlot == 0 && a.entry == 2);
	v[FindLever("assist.4.entry")] = 2;   // edge
	v[FindLever("assist.4.pattern")] = 455;
	v[FindLever("assist.4.command")] = 40;
	a = ResolveAssistSlot(v, 3);
	CHECK(a.mode == AssistMode::Pattern && a.value == 455 && a.fromSlot == 3 && a.entry == 1);   // pattern > command
	v[FindLever("assistEntry")] = 1;
	v[FindLever("assist.5.entry")] = 0;
	a = ResolveAssistSlot(v, 1);
	CHECK(a.entry == 1 && a.mode == AssistMode::Command);

	// the default assist per file, against the guide table
	std::string tsv;
	CHECK(ReadAll(fixtures + "/assist_defaults.tsv", tsv));
	std::istringstream in(tsv);
	std::string ln;
	int checked = 0, fixtureRows = 0;
	while (std::getline(in, ln)) {
		if (ln.empty() || ln[0] == '#') continue;
		char file[64], id[16], input[16];
		int moon = 0, pat = 0;
		char patText[16];
		if (std::sscanf(ln.c_str(), "%63s %d %15s %15s %15s", file, &moon, id, input, patText) != 5) continue;
		pat = std::atoi(patText);
		const std::string name = std::string(file) + "_" + std::to_string(moon) + "_c.txt";
		std::string bytes;
		const bool fix = ReadAll(fixtures + "/" + name, bytes);
		if (!fix && (dataDir.empty() || !ReadAll(dataDir + "/" + name, bytes))) continue;
		fixtureRows += fix;
		const std::vector<CommandInfo> cmds = ParseCommands(bytes);
		const int d = PickDefaultCommand(cmds);
		if (std::string(id) == "-") { CHECKM(d < 0, name + " should have no default"); ++checked; continue; }
		CHECKM(d >= 0, name + " has no default");
		if (d < 0) continue;
		CHECKM(cmds[d].id == std::atoi(id) && cmds[d].input == input && cmds[d].pattern == pat,
		       name + ": got " + std::to_string(cmds[d].id) + " " + cmds[d].input + " " + std::to_string(cmds[d].pattern) +
		       ", guide " + id + " " + input + " " + patText);
		++checked;
	}
	CHECK(fixtureRows == 3);
	std::printf("default assists checked against the guide: %d rows (%d from fixtures)\n", checked, fixtureRows);

	// shiki C: command 45 = 236A -> 105; motion 236C = the lowest-id command with that input (47, EX)
	std::string shiki;
	CHECK(ReadAll(fixtures + "/shiki_0_c.txt", shiki));
	const std::vector<CommandInfo> cmds = ParseCommands(shiki);
	const CommandInfo* c45 = FindCommand(cmds, 45);
	CHECK(c45 && c45->input == "236A" && c45->pattern == 105);
	const CommandInfo* m = CommandForMotion(cmds, "236c");
	CHECK(m && m->id == 47 && m->meter == 10000);
	AssistAction act;
	act.mode = AssistMode::Motion;
	PackMotion("236C", act.value);
	CHECK(DescribeAction(act, cmds).pattern == 107);
	act.mode = AssistMode::Default;
	CHECK(DescribeAction(act, cmds).pattern == 455);
	act.mode = AssistMode::Command;
	act.value = 9999;
	CHECK(DescribeAction(act, cmds).problem);
}

int main(int argc, char** argv)
{
	std::string fixtures = "tests/fixtures/tag", data;
	for (int i = 1; i + 1 < argc; ++i) {
		if (!std::strcmp(argv[i], "--fixtures")) fixtures = argv[++i];
		else if (!std::strcmp(argv[i], "--data")) data = argv[++i];
	}
	std::string sample;
	if (!ReadAll(fixtures + "/tag_tuning.sample.ini", sample)) {
		std::printf("FAIL: cannot read %s/tag_tuning.sample.ini\n", fixtures.c_str());
		return 1;
	}
	TestRoundTrip(sample);
	TestLeversVsSample(sample);
#ifdef HAVE_PC_TAG_TUNING
	TestAgainstPovertyCaster(sample);
#else
	std::printf("PovertyCaster header not found at configure time: row-by-row comparison skipped\n");
#endif
	TestAssist(fixtures, data);
	std::printf("tag_tuning_test: %d passed, %d failed -> %s\n", g_pass, g_fail, g_fail ? "FAIL" : "PASS");
	return g_fail ? 1 : 0;
}
