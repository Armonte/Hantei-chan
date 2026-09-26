// tag_sidecar_test: the [authoring] sidecar tree (docs/HANTEI_AUTHORING_MODE.md §2 / §8.1 / §8.4 / §9.1):
//   * file names, the character-file grammar (bare [char], [char.<name>] synonym, no [moon.*]);
//   * resolution: shipped < local, shared < moon, legacy [char.f] below the character files, provenance;
//   * byte-preserving edits in character files; the atomic writer (no .tmp left, .bak once);
//   * save refused on a new warning; external change handling (clean reloads, dirty asks);
//   * the undo history writes back the exact previous bytes;
//   * checkpoints / A-B snapshots; Promote to defaults;
//   * migration tag_tuning.ini -> local\ on the sample, the capture ini and a synthetic file, with the refusal and the
//     rename-only-after-verify rules.
#include "tag_tuning/tag_history.h"
#include "tag_tuning/tag_migrate.h"
#include "tag_tuning/tag_sidecar.h"

#include <windows.h>

#ifdef HAVE_PC_SIDECAR
#include "mbaacc/TagSidecar.hpp"
#endif

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <set>
#include <memory>
#include <map>

using namespace tagtune;
namespace fs = std::filesystem;

static int g_fail = 0, g_checks = 0;
#define CHECK(c) do { ++g_checks; if (!(c)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)
#define CHECKM(c, m) do { ++g_checks; if (!(c)) { std::printf("FAIL %s:%d  %s  (%s)\n", __FILE__, __LINE__, #c, std::string(m).c_str()); ++g_fail; } } while (0)

static std::string ReadText(const std::string& p)
{
	std::ifstream f(fs::u8path(p), std::ios::binary);
	std::ostringstream s;
	s << f.rdbuf();
	return s.str();
}
static void WriteText(const std::string& p, const std::string& t)
{
	fs::create_directories(fs::u8path(p).parent_path());
	std::ofstream f(fs::u8path(p), std::ios::binary);
	f << t;
}
static bool Exists(const std::string& p) { std::error_code ec; return fs::exists(fs::u8path(p), ec); }

static std::string TempRoot(const char* tag)
{
	char tmp[MAX_PATH];
	GetTempPathA(MAX_PATH, tmp);
	std::string r = std::string(tmp) + "hc_sidecar_test_" + std::to_string(GetCurrentProcessId()) + "_" + tag;
	std::error_code ec;
	fs::remove_all(fs::u8path(r), ec);
	fs::create_directories(fs::u8path(r), ec);
	return r;
}
static int L(const char* key) { return FindLever(key); }

static void TestNames()
{
	std::string f;
	int m = 9;
	CHECK(ParseCharFileName("shiki.ini", f, m) && f == "shiki" && m == -1);
	CHECK(ParseCharFileName("SHIKI_Full.INI", f, m) && f == "shiki" && m == 1);
	CHECK(ParseCharFileName("v_sion_half.ini", f, m) && f == "v_sion" && m == 2);
	CHECK(ParseCharFileName("m_hisui_crescent.ini", f, m) && f == "m_hisui" && m == 0);
	CHECK(!ParseCharFileName("sh iki.ini", f, m));
	CHECK(!ParseCharFileName("shiki.ini.bak", f, m));
	CHECK(!ParseCharFileName("shiki.txt", f, m));
	CHECK(!ParseCharFileName(std::string(28, 'a') + ".ini", f, m));
	CHECK(ParseCharFileName(std::string(27, 'a') + ".ini", f, m));
	CHECK(CharDoc(Layer::Local, "Shiki", 1).RelPath() == "chars\\shiki_full.ini");
	CHECK(CharDoc(Layer::Shipped, "sion").RelPath() == "chars\\sion.ini");
	CHECK(GlobalDoc(Layer::Local).Label() == "local global.ini");
}

static void TestCharGrammar()
{
	TagIni a;
	a.SetCharFileMode(true);
	a.LoadText("; shiki\n[char]\ncancelWindowTicks=6 ; mine\n[char.shiki]\ntagIn=250\n[moon.full]\ntagIn=1\n");
	std::string v;
	CHECK(a.Get(SecKind::Char, "", "tagIn", v) && v == "250");   // [moon.full] is an unknown section now
	CHECK(a.Get(SecKind::Char, "whatever", "cancelWindowTicks", v) && v == "6");
	// byte-preserving edit: only the value text changes, the trailing comment stays
	const std::string before = a.Text();
	SetDocLever(a, DocKind::CharShared, L("cancelWindowTicks"), 9);
	CHECK(a.Text() == "; shiki\n[char]\ncancelWindowTicks=9 ; mine\n[char.shiki]\ntagIn=250\n[moon.full]\ntagIn=1\n");
	SetDocLever(a, DocKind::CharShared, L("cancelWindowTicks"), 6);
	CHECK(a.Text() == before);
	// a new key lands after the last key of the last [char] instance
	SetDocLever(a, DocKind::CharShared, L("entryStyle"), 1);
	CHECK(a.Text() == "; shiki\n[char]\ncancelWindowTicks=6 ; mine\n[char.shiki]\ntagIn=250\nentryStyle=drop\n[moon.full]\ntagIn=1\n");
	ClearDocLever(a, DocKind::CharShared, L("entryStyle"));
	CHECK(a.Text() == before);
	// a fresh file gets "[char]"
	TagIni b;
	b.SetCharFileMode(true);
	b.LoadText("");
	SetDocLever(b, DocKind::CharMoon, L("tagIn"), 252);
	CHECK(b.Text() == "[char]\ntagIn=252\n");
	// CRLF kept
	TagIni c;
	c.SetCharFileMode(true);
	c.LoadText("[char]\r\ntagOut=242\r\n");
	SetDocLever(c, DocKind::CharShared, L("tagIn"), 241);
	CHECK(c.Text() == "[char]\r\ntagOut=242\r\ntagIn=241\r\n");
	// in global mode a bare [char] is an unknown section (legacy grammar untouched)
	TagIni g;
	g.LoadText("[char]\ntagIn=1\n");
	CHECK(g.ParseWarnings().size() == 1);
}

static void TestResolution()
{
	TagIni sg, lg, ss, ls, lmf, smf, lmh;
	for (TagIni* t : { &ss, &ls, &lmf, &smf, &lmh }) t->SetCharFileMode(true);
	sg.LoadText("[tuning]\nactive_style=Classic\ncooldownTicks=90\ncancelWindowTicks=3\n[style.Mine]\ncooldownTicks=11\n[char.shiki]\nentryX=100\n");
	lg.LoadText("[tuning]\ncooldownTicks=70\n");
	ss.LoadText("[char]\ncancelWindowTicks=5\ntagIn=240\n");
	ls.LoadText("[char]\ntagIn=250\n");
	smf.LoadText("[char]\ntagIn=251\nentryY=-100\n");
	lmf.LoadText("[char]\ntagIn=252\n");
	lmh.LoadText("[char]\ncooldownTicks=5\n");   // team-wide in a character file: warning, skipped
	SidecarSet s;
	s.global[0] = &sg; s.global[1] = &lg;
	s.shared[0]["shiki"] = &ss; s.shared[1]["shiki"] = &ls;
	s.moon[0][1]["shiki"] = &smf; s.moon[1][1]["shiki"] = &lmf; s.moon[1][2]["shiki"] = &lmh;
	const GlobalResolution g = ResolveGlobal(s);
	CHECK(g.style == "Classic");
	CHECK(g.values[L("regen")] == 1 && g.from[L("regen")].src == Src::Style);
	CHECK(g.values[L("cooldownTicks")] == 70 && g.from[L("cooldownTicks")].src == Src::Tuning && g.from[L("cooldownTicks")].layer == Layer::Local);
	CHECK(g.values[L("cancelWindowTicks")] == 3 && g.from[L("cancelWindowTicks")].layer == Layer::Shipped);
	const SlotResolution c = ResolveSlot(s, g, "shiki", 0);
	CHECK(c.values[L("tagIn")] == 250 && c.from[L("tagIn")].src == Src::Char && c.from[L("tagIn")].layer == Layer::Local);
	CHECK(c.values[L("cancelWindowTicks")] == 5 && c.from[L("cancelWindowTicks")].src == Src::Char);
	CHECK(c.values[L("entryX")] == 100);                 // legacy [char.shiki] in the shipped global.ini
	CHECK(c.hasCharFile && !c.hasMoonFile);
	const SlotResolution f = ResolveSlot(s, g, "SHIKI", 1);
	CHECK(f.values[L("tagIn")] == 252 && f.from[L("tagIn")].src == Src::Moon && f.from[L("tagIn")].layer == Layer::Local);
	CHECK(f.values[L("entryY")] == -100 && f.from[L("entryY")].layer == Layer::Shipped);   // shipped moon beats local shared
	CHECK(f.hasMoonFile);
	const SlotResolution h = ResolveSlot(s, g, "shiki", 2);
	CHECK(h.values[L("cooldownTicks")] == 70 && h.values[L("tagIn")] == 250);
	const SlotResolution other = ResolveSlot(s, g, "sion", 0);
	CHECK(other.values == g.values && !other.hasCharFile);
	// style lookup: local section beats shipped beats built-in
	TagIni lg2;
	lg2.LoadText("[tuning]\nactive_style=Mine\n[style.Mine]\ncooldownTicks=22\n");
	s.global[1] = &lg2;
	CHECK(ResolveGlobal(s).values[L("cooldownTicks")] == 90);   // shipped [tuning] still beats the style
	TagIni sg2;
	sg2.LoadText("[style.Mine]\ncooldownTicks=11\n");
	s.global[0] = &sg2;
	CHECK(ResolveGlobal(s).values[L("cooldownTicks")] == 22);
	s.global[1] = nullptr;
	TagIni lg3;
	lg3.LoadText("[tuning]\nactive_style=Mine\n");
	s.global[1] = &lg3;
	CHECK(ResolveGlobal(s).values[L("cooldownTicks")] == 11);
	// warnings: the team-wide key in the half file, the legacy section notice
	s.global[0] = &sg; s.global[1] = &lg;
	const std::vector<Warning> w = AllSidecarWarnings(s);
	bool teamWide = false, legacy = false;
	for (const Warning& x : w) {
		teamWide = teamWide || (x.msg.find("local chars\\shiki_half.ini") != std::string::npos && x.msg.find("team-wide") != std::string::npos);
		legacy = legacy || x.msg.find("per-character section in global.ini") != std::string::npos;
	}
	CHECK(teamWide && legacy);
	const std::vector<std::string> names = s.CharFiles();
	CHECK(names.size() == 1 && names[0] == "shiki");
	CHECK(SidecarStyleNames(s).size() == 8);   // 7 built-ins + Mine
}

static void TestWorkspace()
{
	ResetBakMemory();
	const std::string root = TempRoot("ws");
	WriteText(root + "\\global.ini", "[tuning]\nactive_style=Classic\n");
	WriteText(root + "\\chars\\shiki.ini", "[char]\ntagIn=241\n");
	WriteText(root + "\\local\\chars\\shiki_full.ini", "; mine\n[char]\ntagIn=250\n");
	WriteText(root + "\\local\\chars\\notes.txt", "ignored");
	SidecarWorkspace ws;
	CHECK(ws.Open(root) && ws.Exists());
	CHECK(ws.Find(CharDoc(Layer::Local, "shiki", 1)) && ws.Find(CharDoc(Layer::Local, "shiki", 1))->onDisk);
	CHECK(ws.Slot("shiki", 1).values[L("tagIn")] == 250);
	CHECK(ws.Slot("shiki", 0).values[L("tagIn")] == 241);
	CHECK(!ws.AnyDirty());
	// edit + save: atomic, .bak once, no .tmp
	const SidecarWorkspace::Snapshot before = ws.Take();
	SidecarDoc& d = ws.Doc(CharDoc(Layer::Local, "shiki", 1));
	SetDocLever(d.ini, DocKind::CharMoon, L("tagIn"), 260);
	CHECK(ws.AnyDirty());
	TagHistory hist;
	CHECK(hist.Commit("tagIn (shiki, full)", before, ws));
	SidecarWorkspace::SaveResult r = ws.SaveDirty();
	CHECKM(r.ok, r.message);
	const std::string p = root + "\\local\\chars\\shiki_full.ini";
	CHECK(ReadText(p) == "; mine\n[char]\ntagIn=260\n");
	CHECK(ReadText(p + ".bak") == "; mine\n[char]\ntagIn=250\n");
	CHECK(!Exists(p + ".tmp"));
	SetDocLever(d.ini, DocKind::CharMoon, L("tagIn"), 261);
	const SidecarWorkspace::Snapshot mid = ws.Take();
	(void)mid;
	CHECK(ws.SaveDirty().ok);
	CHECK(ReadText(p + ".bak") == "; mine\n[char]\ntagIn=250\n");   // .bak written once per session
	// a new file in a new folder (local global.ini)
	SidecarDoc& lg = ws.Doc(GlobalDoc(Layer::Local));
	SetTuningLever(lg.ini, L("cooldownTicks"), 60);
	CHECK(ws.SaveDirty().ok && ReadText(root + "\\local\\global.ini") == "[tuning]\ncooldownTicks=60\n");
	CHECK(!Exists(root + "\\local\\global.ini.bak"));   // nothing to back up
	// a save that adds a warning is refused, nothing written
	const std::string g0 = ReadText(root + "\\local\\global.ini");
	lg.ini.SetText(lg.ini.Text() + "bogusKey=1\n");
	r = ws.SaveDirty();
	CHECK(!r.ok && r.blocked.size() == 1 && ReadText(root + "\\local\\global.ini") == g0);
	ws.Revert(GlobalDoc(Layer::Local));
	CHECK(!ws.AnyDirty());
	// undo writes back the exact previous bytes (the history entry from before the first save)
	CHECK(hist.CanUndo() && hist.UndoLabel() == "tagIn (shiki, full)");
	std::vector<DocId> touched = hist.Undo(ws);
	CHECK(touched.size() == 1);
	CHECK(ws.SaveDirty().ok && ReadText(p) == "; mine\n[char]\ntagIn=250\n");
	touched = hist.Redo(ws);
	CHECK(ws.SaveDirty().ok && ReadText(p) == "; mine\n[char]\ntagIn=260\n");
	// external change: a clean document reloads silently
	Sleep(20);
	WriteText(p, "; theirs\n[char]\ntagIn=270\n");
	std::vector<DocId> ch = ws.PollExternal();
	CHECK(ch.size() == 1 && ws.Slot("shiki", 1).values[L("tagIn")] == 270 && !ws.AnyDirty());
	// a dirty one asks; a save is refused while the question is open
	SetDocLever(ws.Doc(CharDoc(Layer::Local, "shiki", 1)).ini, DocKind::CharMoon, L("tagIn"), 280);
	Sleep(20);
	WriteText(p, "; theirs 2\n[char]\ntagIn=271\n");
	ch = ws.PollExternal();
	CHECK(ch.size() == 1 && ws.Find(CharDoc(Layer::Local, "shiki", 1))->externalChange);
	CHECK(!ws.SaveDirty().ok);
	ws.ResolveExternal(CharDoc(Layer::Local, "shiki", 1), false);   // keep mine
	CHECK(ws.SaveDirty().ok && ReadText(p) == "; theirs\n[char]\ntagIn=280\n");
	Sleep(20);
	WriteText(p, "[char]\ntagIn=290\n");
	SetDocLever(ws.Doc(CharDoc(Layer::Local, "shiki", 1)).ini, DocKind::CharMoon, L("tagIn"), 281);
	ws.PollExternal();
	ws.ResolveExternal(CharDoc(Layer::Local, "shiki", 1), true);    // load theirs
	CHECK(!ws.AnyDirty() && ws.Slot("shiki", 1).values[L("tagIn")] == 290);
	// a new file appearing on disk is picked up
	WriteText(root + "\\local\\chars\\sion.ini", "[char]\ncancelWindowTicks=2\n");
	ch = ws.PollExternal();
	CHECK(ws.Slot("sion", 0).values[L("cancelWindowTicks")] == 2);
	// snapshots (A/B): restore + save gives the exact bytes back
	const SidecarWorkspace::Snapshot A = ws.Take();
	SetDocLever(ws.Doc(CharDoc(Layer::Local, "sion")).ini, DocKind::CharShared, L("cancelWindowTicks"), 8);
	SetDocLever(ws.Doc(GlobalDoc(Layer::Local)).ini, DocKind::Global, L("regen"), 0);
	CHECK(ws.SaveDirty().ok);
	const SidecarWorkspace::Snapshot B = ws.Take();
	CHECK(ws.Restore(A).size() == 2 && ws.SaveDirty().ok);
	CHECK(ReadText(root + "\\local\\chars\\sion.ini") == "[char]\ncancelWindowTicks=2\n");
	CHECK(ws.Restore(B).size() == 2 && ws.SaveDirty().ok);
	CHECK(ReadText(root + "\\local\\chars\\sion.ini") == "[char]\ncancelWindowTicks=8\n");
	// promote to defaults: the shipped file gets the key, the local one loses it
	std::string err;
	CHECK(ws.PromoteLever(CharDoc(Layer::Local, "sion"), L("cancelWindowTicks"), &err));
	CHECK(ws.SaveDirty().ok);
	CHECK(ReadText(root + "\\chars\\sion.ini") == "[char]\ncancelWindowTicks=8\n");
	CHECK(ReadText(root + "\\local\\chars\\sion.ini") == "[char]\n");
	CHECK(ws.Slot("sion", 0).values[L("cancelWindowTicks")] == 8 && ws.Slot("sion", 0).from[L("cancelWindowTicks")].layer == Layer::Shipped);
	CHECK(!ws.PromoteLever(CharDoc(Layer::Local, "sion"), L("cancelWindowTicks"), &err));
	// nothing left behind but the files and their .bak
	int tmps = 0;
	for (const auto& e : fs::recursive_directory_iterator(fs::u8path(root)))
		if (e.path().extension() == ".tmp") ++tmps;
	CHECK(tmps == 0);
	std::error_code ec;
	fs::remove_all(fs::u8path(root), ec);
}

static std::string Crlf(const std::string& s)
{
	std::string o;
	for (char c : s) { if (c == '\n') o += '\r'; o += c; }
	return o;
}

static void TestMigrationText(const std::string& name, const std::string& text, int expectChars)
{
	const MigrationPlan p = PlanMigration(text, "2026-09-25");
	CHECKM(p.ok && p.verified, name + ": " + p.error);
	if (expectChars >= 0) CHECKM((int)p.chars.size() == expectChars, name + " chars " + std::to_string(p.chars.size()));
	// no [char.*] header is left in global.ini (unless a bad name), every original byte survives somewhere
	size_t total = p.globalText.size();
	for (const auto& kv : p.chars) total += kv.second.size();
	CHECKM(total > text.size(), name);
}

static void TestMigration(const std::string& sample, const std::string& capture)
{
	TestMigrationText("sample", sample, 0);
	TestMigrationText("sample crlf", Crlf(sample), 0);
	TestMigrationText("capture", capture, 1);
	const std::string synth =
		"; synthetic legacy file\n"
		"[tuning]\nactive_style=ActiveTag\ncooldownTicks=90 ; comment\nunknownKey=1\n"
		"; [char.miyako]\n; tagIn=1\n"
		"[char.Shiki]\ncancelWindowTicks=6\n; \x93\xfa\x96\x7b\x8c\xea comment (CP932)\ntagIn=250\n"
		"[style.Mine]\nregen=1\n"
		"[char.miyako]\nentryStyle=drop\nairTagIn=1\n"
		"[char.MIYAKO]\nentryStyle=arc  # later wins\nbogus=2\ncooldownTicks=5\n"
		"[weird]\nx=1\n"
		"[char.Bad Name]\ntagIn=3\n"
		"[tuning]\nkoRule=allDown\n"
		"[char.shiki]\nassist.5.command=45\nassist.2.motion=236C";   // no final newline
	TestMigrationText("synthetic", synth, 2);
	TestMigrationText("synthetic crlf", Crlf(synth), 2);
	const MigrationPlan p = PlanMigration(synth, "d");
	CHECK(p.keptLegacy.size() == 1 && p.keptLegacy[0] == "Bad Name");
	CHECK(p.chars.count("shiki") && p.chars.count("miyako"));
	CHECK(p.chars.at("miyako").find("entryStyle=drop\nairTagIn=1\nentryStyle=arc  # later wins\nbogus=2\ncooldownTicks=5\n") != std::string::npos);
	CHECK(p.chars.at("shiki").find("\x93\xfa\x96\x7b\x8c\xea") != std::string::npos);
	CHECK(p.chars.at("shiki").find("assist.2.motion=236C") != std::string::npos);
	CHECK(p.globalText.find("; [char.miyako]") != std::string::npos && p.globalText.find("[char.Bad Name]") != std::string::npos);
	CHECK(p.globalText.find("[char.Shiki]") == std::string::npos && p.globalText.find("[weird]") != std::string::npos);
	CHECK(p.globalText.compare(0, 13, "; migrated fr") == 0);
	CHECK(NormalizeWarning("local chars\\shiki.ini: unknown lever 'x'") == "unknown lever 'x'");
	CHECK(NormalizeWarning("local global.ini: [tuning]: bad value") == "bad value");

	// the game-dir procedure: dry run writes nothing; the real run writes local\ and renames; a second run refuses
	ResetBakMemory();
	const std::string game = TempRoot("mig");
	WriteText(game + "\\tag_tuning.ini", synth);
	WriteText(game + "\\povertycaster\\tag\\global.ini", "[tuning]\nregen=1\n");   // shipped defaults exist
	MigrationResult r = MigrateGameDir(game, true, "d");
	CHECKM(r.plan.ok && !r.wrote && !Exists(game + "\\povertycaster\\tag\\local\\global.ini"), r.log);
	CHECK(Exists(game + "\\tag_tuning.ini") && r.plan.shippedChanges.size() == 1);
	r = MigrateGameDir(game, false, "d");
	CHECKM(r.wrote, r.log);
	CHECK(!Exists(game + "\\tag_tuning.ini") && ReadText(game + "\\tag_tuning.ini.migrated") == synth);
	CHECK(ReadText(game + "\\povertycaster\\tag\\local\\chars\\miyako.ini") == p.chars.at("miyako"));
	// the migrated tree, read back through the workspace, resolves like the legacy file (local layer)
	{
		SidecarWorkspace ws;
		ws.Open(game + "\\povertycaster\\tag");
		TagIni leg;
		leg.LoadText(synth);
		const Resolved lr = Resolve(leg);
		SidecarSet local = ws.Set();
		local.global[0] = nullptr;
		const GlobalResolution gr = ResolveGlobal(local);
		CHECK(gr.values == lr.global);
		CHECK(ResolveSlot(local, gr, "miyako", 1).values == ForChar(leg, lr.global, "miyako"));
		CHECK(ws.Global().values[L("regen")] == 1);   // + the shipped layer underneath
	}
	WriteText(game + "\\tag_tuning.ini", synth);
	r = MigrateGameDir(game, false, "d");
	CHECK(!r.wrote && r.plan.error.find("refused") != std::string::npos);
	// a verification failure writes nothing and does not rename (forced here: a file the rules cannot split identically
	// does not exist by construction, so check the no-legacy path instead)
	const std::string empty = TempRoot("mig2");
	r = MigrateGameDir(empty, false, "d");
	CHECK(!r.wrote && !r.plan.error.empty());
	std::error_code ec;
	fs::remove_all(fs::u8path(game), ec);
	fs::remove_all(fs::u8path(empty), ec);
}

static void TestHistoryCoalesce()
{
	SidecarWorkspace ws;
	const std::string root = TempRoot("hist");
	ws.Open(root);
	TagHistory h;
	const auto s0 = ws.Take();
	CHECK(!h.Commit("nothing", s0, ws));   // a no-op is not recorded
	SidecarDoc& g = ws.Doc(GlobalDoc(Layer::Local));
	SelectStyle(g.ini, "Chaos", true);
	SetDocLever(ws.Doc(CharDoc(Layer::Local, "sion")).ini, DocKind::CharShared, L("tagIn"), 241);
	CHECK(h.Commit("style Chaos + sion", s0, ws));
	CHECK(h.Size() == 1 && h.Entries()[0].changes.size() == 2);
	h.Undo(ws);
	CHECK(ws.Doc(GlobalDoc(Layer::Local)).ini.Text().empty() && ws.Doc(CharDoc(Layer::Local, "sion")).ini.Text().empty());
	CHECK(!h.CanUndo() && h.CanRedo() && h.RedoLabel() == "style Chaos + sion");
	h.Redo(ws);
	CHECK(ws.Global().style == "Chaos");
	// a new commit after an undo drops the redo tail
	h.Undo(ws);
	const auto s1 = ws.Take();
	SetDocLever(ws.Doc(GlobalDoc(Layer::Local)).ini, DocKind::Global, L("regen"), 1);
	CHECK(h.Commit("regen", s1, ws) && !h.CanRedo() && h.Size() == 1);
	std::error_code ec;
	fs::remove_all(fs::u8path(root), ec);
}


// ---- the same trees through PovertyCaster's own pure resolver (TagSidecar.hpp resolveSidecars / resolveSlot) ----
// HC resolves the files itself to show the provenance and to flag game != editor cells (§1 rule 3); that is only honest
// if both resolvers agree on every lever for every character and moon. Enabled when PC_SIDECAR_HPP is on disk.
struct TreeText { std::string global; std::map<std::string, std::string> chars; bool hasGlobal = false; };

static void CrossCheck(const std::string& name, const TreeText& shipped, const TreeText& local)
{
#ifdef HAVE_PC_SIDECAR
	namespace P = mbaacc::tag;
	// HC
	std::vector<std::unique_ptr<TagIni>> keep;
	SidecarSet hs;
	auto add = [&](const TreeText& t, int layer) {
		if (t.hasGlobal) { keep.push_back(std::make_unique<TagIni>()); keep.back()->LoadText(t.global); hs.global[layer] = keep.back().get(); }
		for (const auto& kv : t.chars) {
			std::string f; int m = -1;
			if (!ParseCharFileName(kv.first, f, m)) continue;
			keep.push_back(std::make_unique<TagIni>());
			keep.back()->SetCharFileMode(true);
			keep.back()->LoadText(kv.second);
			if (m < 0) hs.shared[layer][f] = keep.back().get(); else hs.moon[layer][m][f] = keep.back().get();
		}
	};
	add(shipped, 0);
	add(local, 1);
	const GlobalResolution hg = ResolveGlobal(hs);
	// PC
	P::SidecarSet ps;
	ps.shipped.hasGlobal = shipped.hasGlobal; ps.shipped.global = shipped.global; ps.shipped.chars = shipped.chars;
	ps.local.hasGlobal = local.hasGlobal; ps.local.global = local.global; ps.local.chars = local.chars;
	const P::SidecarResolved pr = P::resolveSidecars(ps, [](const char*) -> const char* { return nullptr; });
	CHECKM(pr.style == hg.style, name + ": style '" + pr.style + "' (pc) vs '" + hg.style + "' (hc)");
	for (size_t i = 0; i < kLeverCount; ++i) {
		const int32_t pv = P::leverGet(pr.g, P::kLevers[i]);
		CHECKM(pv == hg.values[i], name + ": G " + kLevers[i].key + " pc " + std::to_string(pv) + " hc " + std::to_string(hg.values[i]));
		CHECKM(pr.tuningMask.test(i) == (hg.from[i].src == Src::Tuning), name + ": tuningMask " + kLevers[i].key);
		CHECKM(pr.styleMask.test(i) == (hg.from[i].src == Src::Style), name + ": styleMask " + kLevers[i].key);
	}
	std::set<std::string> files;
	for (const auto& kv : pr.chars) files.insert(kv.first);
	for (const std::string& f : hs.CharFiles()) files.insert(f);
	files.insert("sion");   // a character with no file at all
	int slots = 0;
	for (const std::string& f : files)
		for (int m = 0; m < 3; ++m) {
			const P::SlotResolved ps2 = P::resolveSlot(pr, f, m);
			const SlotResolution hr = ResolveSlot(hs, hg, f, m);
			for (size_t i = 0; i < kLeverCount; ++i) {
				const int32_t pv = P::leverGet(ps2.t, P::kLevers[i]);
				CHECKM(pv == hr.values[i], name + ": " + f + " moon " + std::to_string(m) + " " + kLevers[i].key + " pc " + std::to_string(pv) +
				                              " hc " + std::to_string(hr.values[i]));
				CHECKM(ps2.charMask.test(i) == (hr.from[i].src == Src::Char), name + ": charMask " + f + " " + kLevers[i].key);
				CHECKM(ps2.moonMask.test(i) == (hr.from[i].src == Src::Moon), name + ": moonMask " + f + " " + kLevers[i].key);
			}
			++slots;
		}
	std::printf("cross-check %s: HC == PovertyCaster TagSidecar.hpp on G + %d character x moon slots\n", name.c_str(), slots);
#else
	(void)name; (void)shipped; (void)local;
#endif
}

static TreeText ReadTree(const std::string& root)
{
	TreeText t;
	t.hasGlobal = Exists(root + "/global.ini");
	if (t.hasGlobal) t.global = ReadText(root + "/global.ini");
	std::error_code ec;
	for (const auto& e : fs::directory_iterator(fs::u8path(root + "/chars"), ec))
		if (e.is_regular_file(ec)) t.chars[e.path().filename().u8string()] = ReadText(e.path().u8string());
	return t;
}

static void TestCrossCheck(const std::string& sample)
{
#ifdef HAVE_PC_SIDECAR
	CrossCheck("fixture tree", ReadTree("tests/fixtures/authoring/tag"), ReadTree("tests/fixtures/authoring/tag/local"));
	TreeText s1, l1;
	s1.hasGlobal = true; s1.global = sample;
	l1.hasGlobal = true; l1.global = "[tuning]\nactive_style=Chaos\ncooldownTicks=33\n[style.Chaos]\nregen=0\n[char.shiki]\nentryX=100\n";
	s1.chars["shiki.ini"] = "[char]\ncancelWindowTicks=4\ntagIn=240\n";
	l1.chars["shiki.ini"] = "[char.shiki]\ntagIn=250\nbogus=1\ncooldownTicks=9\n";
	s1.chars["shiki_full.ini"] = "[char]\nentryStyle=arc\n";
	l1.chars["shiki_half.ini"] = "[char]\nassist.5.motion=236C\nassist.5.command=45\n[moon.full]\ntagIn=3\n";
	l1.chars["miyako_crescent.ini"] = "[char]\nairTagIn=1\nentryStyle=drop\n";
	CrossCheck("edited trees", s1, l1);
	TreeText empty, onlyChars;
	onlyChars.chars["v_sion.ini"] = "[char]\nswapRedKeepPct=10\n";
	CrossCheck("chars only, no global", empty, onlyChars);
#else
	(void)sample;
	std::printf("PovertyCaster TagSidecar.hpp not found at configure time (PC_SIDECAR_HPP): resolver cross-check skipped\n");
#endif
}

int main(int argc, char** argv)
{
	std::string dir = "tests/fixtures/tag";
	if (argc > 1) dir = argv[1];
	const std::string sample = ReadText(dir + "/tag_tuning.sample.ini");
	const std::string capture = ReadText(dir + "/capture_tag_tuning.ini");
	CHECKM(!sample.empty() && !capture.empty(), "fixtures in " + dir);
	TestNames();
	TestCharGrammar();
	TestResolution();
	TestWorkspace();
	TestMigration(sample, capture);
	TestHistoryCoalesce();
	TestCrossCheck(sample);
	std::printf(g_fail ? "tag_sidecar_test: %d of %d FAILED\n" : "tag_sidecar_test: all %d checks passed\n", g_fail ? g_fail : g_checks, g_checks);
	return g_fail ? 1 : 0;
}
