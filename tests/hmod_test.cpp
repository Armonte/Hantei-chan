// Mod projects (src/modproject): hmod.json round trip and validation, HA6 canonical slot hashing, change tracking against a
// vanilla index, the conflict check (same file / same HA6 slot / same charaselect cell) and both exporters (pcmod for
// MBAACC, the UNI2-IM / MBTL-IM Packs layout). Everything runs on synthetic folders under <outdir>.
#include "modproject/hmod_conflict.h"
#include "modproject/hmod_export.h"
#include "modproject/hmod_ha6.h"
#include "modproject/hmod_import.h"
#include "modproject/hmod_project.h"
#include "modproject/hmod_track.h"
#include "pcmod/pcmod_package.h"
#include "pcmod/pcmod_vanilla.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace hmod;
static int g_failures = 0;
#define CHECK(expr) do { if (!(expr)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); ++g_failures; } } while (0)
#define CHECK_EQ(a, b) do { const std::string a_ = (a), b_ = (b); if (a_ != b_) { std::printf("FAIL %s:%d: %s\n  got      %s\n  expected %s\n", __FILE__, __LINE__, #a, a_.c_str(), b_.c_str()); ++g_failures; } } while (0)

static void Put(const fs::path& p, const std::string& s) { fs::create_directories(p.parent_path()); std::ofstream f(p, std::ios::binary); f << s; }
static std::string Get(const fs::path& p) { std::ifstream f(p, std::ios::binary); return std::string(std::istreambuf_iterator<char>(f), {}); }
static std::vector<uint8_t> Bytes(const std::string& s) { return std::vector<uint8_t>(s.begin(), s.end()); }

// ---- synthetic HA6 ----
static void W32(std::string& b, uint32_t v) { b.append((const char*)&v, 4); }
static std::string Frame(uint32_t dur, bool shorthand = false)
{
	std::string b = "AFST";
	if (shorthand && dur < 10) b += "AFD" + std::to_string(dur); else { b += "AFDL"; W32(b, dur); }
	return b + "AFEDFSTR";
}
struct Pat { uint32_t id; std::string name; std::string body; bool pds2 = false; };
static std::string Ha6(const std::vector<Pat>& pats)
{
	std::string b = "Hantei6DataFile";
	b.resize(32, '\0');
	b += "_STR"; W32(b, 1000);
	for (const Pat& p : pats) {
		b += "PSTR"; W32(b, p.id);
		if (!p.name.empty()) { std::string n = p.name; n.push_back('\0'); while (n.size() % 4) n.push_back('\0'); b += "PTT2"; W32(b, (uint32_t)n.size()); b += n; }
		if (p.pds2) { b += "PDS2"; W32(b, 8); W32(b, 1); W32(b, 2); }
		b += p.body;
		b += "PEND";
	}
	return b + "_END";
}

static Project MakeProject(const fs::path& dir, const std::string& game)
{
	fs::remove_all(dir);
	Project p; std::string err;
	CHECK(Create(dir.u8string(), "mymod", game, p, err));
	p.author = "tester";
	return p;
}

static int Run(int argc, char** argv);
int main(int argc, char** argv)
{
	std::setvbuf(stdout, nullptr, _IONBF, 0);
	try { return Run(argc, argv); }
	catch (const std::exception& e) { std::printf("EXCEPTION %s\n", e.what()); return 4; }
}

static int Run(int argc, char** argv)
{
	const fs::path out = argc > 1 ? fs::u8path(argv[1]) : fs::path("build/hmod_test_out");
	fs::remove_all(out);
	fs::create_directories(out);

	// ---- pcmodtool loose-vs-packed: a path that is both a loose file and inside a .p is reported as loose (the game reads it first) ----
	{
		const fs::path g = out / "vgame";
		Put(g / "MBAA.exe", "MZ");
		Put(g / "data" / "p_arc.cg", "loose bytes");
		pcmod::VanillaIndex vi;
		CHECK(vi.Load(g.u8string()));
		vi.packed.insert("data\\p_arc.cg");
		vi.packed.insert("data\\only_packed.cg");
		CHECK_EQ(vi.Where("data\\p_arc.cg"), "loose");
		CHECK_EQ(vi.Where("data\\only_packed.cg"), "packed");
		CHECK_EQ(vi.Where("data\\nothing.cg"), "");
	}

	// ---- glob + normalisation ----
	CHECK(GlobMatch("data\\eddie*.txt", "data\\eddie_1.txt"));
	CHECK(GlobMatch("DATA/*_r.HA6", "data\\akiha_1_r.ha6"));
	CHECK(!GlobMatch("data\\*.pal", "data\\sub\\x.cg"));
	CHECK(GlobMatch("*-balance", "akiha-balance"));
	CHECK_EQ(NormKey(".\\Data/Akiha.HA6"), "data\\akiha.ha6");

	// ---- hmod.json round trip + validation ----
	{
		Project p;
		p.id = "gonpmod"; p.name = "gonp's mods"; p.author = "gonp"; p.game = "mbaacc"; p.base.exeSha256 = std::string(64, 'a');
		Asset a; a.id = "akiha-balance"; a.category = "character.framedata"; a.targets = {"data\\akiha_*_r.HA6"}; a.origin.kind = "vanilla-derived"; a.depends = {"system-vectors"};
		a.origin.base.push_back({"data\\akiha_1_r.HA6", "0002.p", std::string(64, 'b')});
		Asset s; s.id = "system-vectors"; s.category = "system.constants"; s.targets = {"data\\vector.txt"};
		Asset e; e.id = "eddie"; e.category = "character.new"; e.origin.kind = "clone"; e.origin.of = "nero"; e.chara.charaNo = 10; e.chara.name = "NERO"; e.chara.file2 = "EDDIE"; e.chara.cell = 10;
		p.assets = {a, s, e};
		Package k; k.id = "gonp-balance"; k.assets = {"*-balance", "system-*"}; k.requires_.push_back({"base", "1.0.0"});
		p.packages = {k};
		CHECK_EQ(Validate(p), "");
		const std::string t = Write(p);
		Project q; std::string err;
		CHECK(Parse(t, q, err));
		if (!err.empty()) std::printf("  parse: %s\n", err.c_str());
		CHECK_EQ(Write(q), t);                                   // byte-stable
		CHECK(q.assets.size() == 3 && q.assets[0].origin.base.size() == 1 && q.assets[2].chara.charaNo == 10 && q.assets[2].chara.cell == 10);
		CHECK(ResolvePackageAssets(q, q.packages[0]).size() == 2);
		Project bad = p; bad.assets[0].category = "nonsense";
		CHECK(!Validate(bad).empty());
		bad = p; bad.assets.push_back(p.assets[0]);
		CHECK(Validate(bad).find("duplicate") != std::string::npos);
		bad = p; bad.assets[0].depends = {"nope"};
		CHECK(Validate(bad).find("unknown asset") != std::string::npos);
		bad = p; bad.packages[0].assets = {"zzz"};
		CHECK(!Validate(bad).empty());
		bad = p; bad.id = "Bad Id";
		CHECK(!Validate(bad).empty());
		bad = p; bad.game = "ps2";
		CHECK(!Validate(bad).empty());
		CHECK(!Parse("{ not json", q, err));
	}

	// ---- HA6 canonical slots ----
	{
		const std::string a = Ha6({{300, "jab", Frame(5)}, {301, "stub", ""}});
		const std::string a2 = Ha6({{300, "jab", Frame(5, true), true}, {301, "stub", ""}});       // shorthand + PDS2: same semantics
		const std::string a3 = Ha6({{300, "JAB RENAMED", Frame(5)}, {301, "stub", ""}});
		const std::string a4 = Ha6({{300, "jab", Frame(6)}, {301, "stub", Frame(2)}});
		Ha6Slots s1, s2, s3, s4; std::string err;
		CHECK(SplitHa6(Bytes(a), s1, err) && SplitHa6(Bytes(a2), s2, err) && SplitHa6(Bytes(a3), s3, err) && SplitHa6(Bytes(a4), s4, err));
		CHECK(s1[300].real && !s1[301].real);
		CHECK_EQ(s1[300].canon, s2[300].canon);
		CHECK(CompareSlot(&s1[300], &s2[300]) == SlotDelta::Same);
		CHECK(CompareSlot(&s1[300], &s3[300]) == SlotDelta::NameOnly);
		CHECK(CompareSlot(&s1[300], &s4[300]) == SlotDelta::Modified);
		CHECK(CompareSlot(&s1[301], &s4[301]) == SlotDelta::Filled);
		CHECK(CompareSlot(&s4[301], &s1[301]) == SlotDelta::Cleared);
		CHECK(CompareSlot(nullptr, &s1[301]) == SlotDelta::Same);                                    // absent vs stub
		CHECK(DiffSlots(s1, s2).claimed.empty());
		CHECK(DiffSlots(s1, s3).claimed.empty() && DiffSlots(s1, s3).nameOnly == 1);
		CHECK(DiffSlots(s1, s4).claimed.size() == 2);
		Ha6Slots bad;
		CHECK(!SplitHa6(Bytes("not an ha6 file at all, definitely"), bad, err));
		CHECK(!SplitHa6(Bytes(Ha6({{1, "x", "ZZZZ"}})), bad, err));
	}

	// ---- vanilla index save / load ----
	VanillaIndex van;
	const std::string vha6 = Ha6({{300, "jab", Frame(5)}, {301, "stub", ""}});
	van.AddBytes("data\\akiha_1_r.HA6", "0003.p", Bytes(vha6));
	van.AddBytes("data\\akiha_1.HA6", "0003.p", Bytes(Ha6({{300, "jab", Frame(5)}, {301, "stub", ""}, {302, "x", Frame(3)}})));
	van.AddBytes("data\\_type.txt", "0001.p", Bytes("ShieldKeepTime = 60\r\nGG_MaxVal = 8000\r\n"));
	van.AddBytes("data\\same.txt", "0001.p", Bytes("same\r\n"));
	van.AddBytes("charaselect.txt", "0001.p", Bytes("[System]\r\n\r\n[CharaNo_010]\r\n\tName = NERO\r\n\tFile1 = NERO\r\n\tFile2 = 0\r\n\r\n[CharaNo_011]\r\n\tName = MIYAKO\r\n\tFile1 = MIYAKO\r\n\tFile2 = 0\r\n"));
	{
		const std::string tsv = (out / "vanilla.tsv").u8string(), tsv2 = (out / "vanilla2.tsv").u8string();
		std::string err;
		CHECK(van.Save(tsv, err));
		VanillaIndex v2;
		CHECK(v2.Load(tsv, err));
		CHECK(v2.size() == van.size());
		CHECK(v2.Find("data\\_type.txt") && v2.Find("data\\_type.txt")->pack == "0001.p");
		CHECK(v2.Save(tsv2, err));
		CHECK_EQ(Get(out / "vanilla.tsv"), Get(out / "vanilla2.tsv"));
	}

	// ---- tracking ----
	const fs::path projDir = out / "proj";
	Project p = MakeProject(projDir, "mbaacc");
	p.base.exeSha256 = std::string(63, '0') + "1";
	{
		Asset bal; bal.id = "akiha-balance"; bal.category = "character.framedata"; bal.origin.kind = "vanilla-derived"; bal.targets = {"data\\*"};
		p.assets.push_back(bal);
		const fs::path ad = projDir / "assets" / "akiha-balance" / "data";
		Put(ad / "akiha_1_r.HA6", Ha6({{300, "jab", Frame(9)}, {301, "stub", Frame(4)}}));       // modifies 300, fills 301
		Put(ad / "akiha_1.HA6", Ha6({{300, "jab", Frame(5, true), true}, {301, "stub", ""}, {302, "RENAMED", Frame(3)}}));   // re-serialised + rename only
		Put(ad / "_type.txt", "ShieldKeepTime = 30\r\nGG_MaxVal = 8000\r\n");                      // changed
		Put(ad / "same.txt", "same\r\n");                                                         // unchanged
		Put(ad / "newfile.txt", "hello\r\n");                                                    // added
		Put(ad / "hantei4.inf", "x");                                                              // clutter
		Put(ad / "pic - Copy.png", "x");                                                           // clutter
		Put(projDir / "assets" / "akiha-balance" / "readme.md", "notes");                         // orphan (outside targets)
		auto t = TrackAsset(p, p.assets[0], &van);
		auto find = [&](const char* n) -> const TrackedFile* { for (const auto& f : t) if (f.norm == n) return &f; return nullptr; };
		CHECK(find("data\\akiha_1_r.ha6") && find("data\\akiha_1_r.ha6")->status == Status::Changed && find("data\\akiha_1_r.ha6")->slotsClaimed == 2);
		CHECK(find("data\\akiha_1.ha6") && find("data\\akiha_1.ha6")->status == Status::Reencoded);
		CHECK(find("data\\_type.txt") && find("data\\_type.txt")->status == Status::Changed);
		CHECK(find("data\\same.txt") && find("data\\same.txt")->status == Status::Unchanged);
		CHECK(find("data\\newfile.txt") && find("data\\newfile.txt")->status == Status::Added);
		CHECK(find("data\\hantei4.inf") && find("data\\hantei4.inf")->status == Status::Clutter);
		CHECK(find("data\\pic - copy.png") && find("data\\pic - copy.png")->status == Status::Clutter);
		CHECK(find("readme.md") && find("readme.md")->status == Status::Orphan);
		// CRLF vs LF text is re-encoded, not changed
		Put(ad / "same.txt", "same\n");
		t = TrackAsset(p, p.assets[0], &van);
		CHECK(find("data\\same.txt")->status == Status::Reencoded);
		Put(ad / "same.txt", "same\r\n");
		t = TrackAsset(p, p.assets[0], &van);
		CHECK(FillBases(p, t) == 1 && p.assets[0].origin.base.size() >= 3);
		CHECK(FillBases(p, t) == 0);                                                              // idempotent
		const StatusCounts c = Count(t);
		CHECK(c.changed == 2 && c.added == 1 && c.unchanged == 1 && c.reencoded == 1 && c.clutter == 2 && c.orphan == 1);
		// no index: everything is "changed (assumed)"
		auto t0 = TrackAsset(p, p.assets[0], nullptr);
		CHECK(!t0.empty() && t0[0].status == Status::Changed);
	}

	// ---- pcmod export + conflict check ----
	const fs::path game = out / "game";                       // a fake MBAACC install with mods\
	fs::create_directories(game / "mods");
	{
		p.assets[0].targets = {"data\\*"};
		Package k; k.id = "mymod-balance"; k.assets = {"akiha-balance"};
		p.packages = {k};
		std::string err;
		CHECK(Save(p, err));
		ExportOptions o; o.gameDir = game.u8string(); o.outDir = (out / "pkg").u8string();
		// dry run first
		o.dryRun = true;
		ExportReport r = ExportPcmod(p, o, &van);
		CHECK(r.ok);
		CHECK(!fs::exists(out / "pkg"));
		o.dryRun = false;

		// an installed mod that ships the same file with different bytes: refuse, write nothing
		const fs::path other = game / "mods" / "othermod";
		pcmod::Manifest om; om.id = "othermod"; om.version = "1.0.0"; om.cls = pcmod::PackageClass::Sim; om.exe = {p.base.exeSha256};
		{
			std::vector<pcmod::SourceFile> src = {{"data\\_type.txt", (other / "data" / "_type.txt").u8string()}};
			fs::path stage = out / "stage_other";
			fs::remove_all(stage);
			Put(stage / "data" / "_type.txt", "ShieldKeepTime = 99\r\n");
			src[0].absPath = (stage / "data" / "_type.txt").u8string();
			const pcmod::PackResult pr = pcmod::Pack(src, om, (game / "mods").u8string(), false);
			CHECK(pr.ok);
			if (!pr.ok) std::printf("  pack: %s\n", pr.error.c_str());
		}
		r = ExportPcmod(p, o, &van);
		CHECK(!r.ok);
		CHECK(AnyBlocking(r.conflicts));
		CHECK(!r.conflicts.empty() && r.conflicts[0].kind == ConflictKind::File && r.conflicts[0].other == "othermod");
		CHECK(!fs::exists(out / "pkg" / "mymod-balance"));
		CHECK(!r.outputs.size());

		// declaring `conflicts` resolves it (they can never be enabled together)
		p.packages[0].conflicts = {"othermod"};
		r = ExportPcmod(p, o, &van);
		CHECK(r.ok);
		if (!r.ok) std::printf("%s", ReportText(r).c_str());
		CHECK(r.outputs.size() == 1 && fs::exists(out / "pkg" / "mymod-balance" / "pcmod.json"));
		const pcmod::VerifyResult vr = pcmod::VerifyPath((out / "pkg" / "mymod-balance").u8string());
		CHECK(vr.ok);
		// only changed + added files ship: 2 HA6? (akiha_1_r changed, _type changed, newfile added); re-encoded / unchanged / clutter / orphan do not
		CHECK(vr.manifest.files.size() == 3);
		CHECK(vr.manifest.cls == pcmod::PackageClass::Sim);
		CHECK(p.outputs.size() == 1 && p.outputs[0].target == "pcmod" && p.outputs[0].files.size() == 3);
		{ Project re; CHECK(Load(projDir.u8string(), re, err)); CHECK(re.outputs.size() == 1); }   // outputs persisted in hmod.json

		// byte-identical content is not a conflict
		{
			pcmod::Manifest om2 = om; om2.id = "twin";
			fs::path stage = out / "stage_twin"; fs::remove_all(stage);
			Put(stage / "data" / "_type.txt", "ShieldKeepTime = 30\r\nGG_MaxVal = 8000\r\n");
			CHECK(pcmod::Pack({{"data\\_type.txt", (stage / "data" / "_type.txt").u8string()}}, om2, (game / "mods").u8string(), false).ok);
		}
		fs::remove_all(game / "mods" / "othermod");
		p.packages[0].conflicts.clear();
		r = ExportPcmod(p, o, &van);
		bool sawTwin = false;
		for (const Conflict& c : r.conflicts) if (c.other == "twin") { sawTwin = true; CHECK(!c.blocks); }
		CHECK(sawTwin);
		CHECK(r.ok);
		fs::remove_all(game / "mods" / "twin");
		fs::remove_all(out / "pkg");

		// same HA6 slot, different files of the same moon layer set: refuse
		{
			pcmod::Manifest hm = om; hm.id = "slotmod";
			fs::path stage = out / "stage_slot"; fs::remove_all(stage);
			Put(stage / "data" / "akiha_1.HA6", Ha6({{300, "jab", Frame(7)}, {301, "stub", ""}, {302, "x", Frame(3)}}));   // modifies slot 300 against vanilla akiha_1
			CHECK(pcmod::Pack({{"data\\akiha_1.HA6", (stage / "data" / "akiha_1.HA6").u8string()}}, hm, (game / "mods").u8string(), false).ok);
			r = ExportPcmod(p, o, &van);
			CHECK(!r.ok);
			bool slot = false;
			for (const Conflict& c : r.conflicts) if (c.kind == ConflictKind::Ha6Slot && c.other == "slotmod" && c.blocks) slot = true;
			CHECK(slot);
			// disjoint slots (other mod only touches slot 305): no slot conflict
			Put(stage / "data" / "akiha_1.HA6", Ha6({{300, "jab", Frame(5)}, {301, "stub", ""}, {302, "x", Frame(3)}, {305, "new", Frame(2)}}));
			CHECK(pcmod::Pack({{"data\\akiha_1.HA6", (stage / "data" / "akiha_1.HA6").u8string()}}, hm, (game / "mods").u8string(), false).ok);
			r = ExportPcmod(p, o, &van);
			for (const Conflict& c : r.conflicts) CHECK(c.kind != ConflictKind::Ha6Slot);
			CHECK(r.ok);
			fs::remove_all(game / "mods" / "slotmod");
			fs::remove_all(out / "pkg");
		}
	}

	// ---- charaselect cell conflicts ----
	{
		Project q = MakeProject(out / "proj_cs", "mbaacc");
		q.base.exeSha256 = p.base.exeSha256;
		Asset a; a.id = "eddie"; a.category = "character.new"; a.origin.kind = "clone"; a.origin.of = "nero"; a.chara.charaNo = 10; a.chara.name = "EDDIE"; a.chara.file1 = "NERO"; a.chara.file2 = "EDDIE";
		q.assets.push_back(a);
		Put(out / "proj_cs" / "assets" / "eddie" / "data" / "eddie.txt", "[DataFile]\r\n");
		Package k; k.id = "eddie-pkg"; k.assets = {"eddie"};
		q.packages.push_back(k);
		const fs::path g2 = out / "game2";
		fs::create_directories(g2 / "mods");
		// another installed mod rewrites [CharaNo_010] (Nero's cell) differently
		fs::path stage = out / "stage_cs"; fs::remove_all(stage);
		Put(stage / "charaselect.txt", "[CharaNo_010]\r\n\tName = NERO\r\n\tFile1 = NERO\r\n\tFile2 = OTHERGUY\r\n\tTagType = 3\r\n\r\n[CharaNo_011]\r\n\tName = MIYAKO\r\n\tFile1 = MIYAKO\r\n\tFile2 = 0\r\n");
		pcmod::Manifest cm; cm.id = "csmod"; cm.version = "1.0.0"; cm.cls = pcmod::PackageClass::Sim; cm.exe = {q.base.exeSha256};
		CHECK(pcmod::Pack({{"charaselect.txt", (stage / "charaselect.txt").u8string()}}, cm, (g2 / "mods").u8string(), false).ok);
		ExportOptions o; o.gameDir = g2.u8string(); o.outDir = (out / "pkg2").u8string();
		ExportReport r = ExportPcmod(q, o, &van);
		CHECK(!r.ok);
		bool cell = false;
		for (const Conflict& c : r.conflicts) if (c.kind == ConflictKind::CharaCell && c.blocks) cell = true;
		CHECK(cell);
		CHECK(!fs::exists(out / "pkg2"));
		// the same entry claimed identically is fine; MIYAKO is untouched vs vanilla so it never counts as a claim
		q.assets[0].chara.name = "NERO"; q.assets[0].chara.file2 = "OTHERGUY";
		r = ExportPcmod(q, o, &van);
		for (const Conflict& c : r.conflicts) CHECK(c.kind != ConflictKind::CharaCell || !c.blocks);
		// two assets in this project claiming the same grid cell, in different packages
		fs::remove_all(g2 / "mods" / "csmod");
		Asset b = a; b.id = "other-guy"; b.chara.charaNo = 99; b.chara.name = "OTHER"; b.chara.cell = 5;
		q.assets[0].chara.cell = 5; q.assets[0].chara.charaNo = 98; q.assets[0].chara.name = "EDDIE";
		q.assets.push_back(b);
		Put(out / "proj_cs" / "assets" / "other-guy" / "data" / "other.txt", "[DataFile]\r\n");
		Package k2; k2.id = "other-pkg"; k2.assets = {"other-guy"};
		q.packages.push_back(k2);
		r = ExportPcmod(q, o, &van);
		CHECK(!r.ok);
		bool gridCell = false;
		for (const Conflict& c : r.conflicts) if (c.kind == ConflictKind::CharaCell && c.what.find("grid cell 5") != std::string::npos) gridCell = true;
		CHECK(gridCell);
	}

	// ---- a sim package without an exe hash, and a refused path, are refused ----
	{
		Project q = MakeProject(out / "proj_ref", "mbaacc");
		Asset a; a.id = "stuff"; a.category = "other"; a.origin.kind = "authored";
		q.assets.push_back(a);
		Put(out / "proj_ref" / "assets" / "stuff" / "data" / "x.txt", "x");
		Put(out / "proj_ref" / "assets" / "stuff" / "system" / "y.txt", "y");
		Package k; k.id = "stuff-pkg"; k.assets = {"stuff"};
		q.packages.push_back(k);
		ExportOptions o; o.outDir = (out / "pkg3").u8string(); o.gameDir = (out / "nogame").u8string();
		ExportReport r = ExportPcmod(q, o, nullptr);
		CHECK(!r.ok);
		bool refusedPath = false, noExe = false;
		for (const std::string& e : r.errors) { if (e.find("system\\y.txt") != std::string::npos) refusedPath = true; if (e.find("exe hash") != std::string::npos) noExe = true; }
		CHECK(refusedPath && noExe);
		CHECK(!fs::exists(out / "pkg3"));
	}

	// ---- UNI2-IM / MBTL-IM mod-folder export ----
	{
		const fs::path root = out / "uni2";
		fs::create_directories(root / "UNI2-IM" / "Mods");
		fs::create_directories(root / "UNI2-IM" / "Packs" / "voice-pack" / "se" / "battle_se");
		Put(root / "UNI2-IM" / "Packs" / "voice-pack" / "se" / "battle_se" / "SE000.wav", "RIFFAAAA");
		Put(root / "UNI2-IM" / "Packs" / "voice-pack" / "mod.ini", "[Mod]\r\nName = Voices\r\n");
		VanillaIndex uv;
		uv.AddBytes("data\\chr025\\chr025.pal", "d", Bytes("PAL0"));
		Project q = MakeProject(out / "proj_uni2", "uni2");
		Asset a; a.id = "pal-swap"; a.category = "palettes"; a.origin.kind = "vanilla-derived";
		q.assets.push_back(a);
		Put(out / "proj_uni2" / "assets" / "pal-swap" / "data" / "chr025" / "chr025.pal", "PAL1");
		Put(out / "proj_uni2" / "assets" / "pal-swap" / "se" / "battle_se" / "SE000.wav", "RIFFBBBB");
		Package k; k.id = "pal-swap-pack"; k.version = "1.2.0"; k.name = "Palette swap"; k.assets = {"pal-swap"};
		q.packages.push_back(k);
		ExportOptions o; o.gameDir = root.u8string();
		// the voice pack ships the same SE file with different bytes: refuse, write nothing
		ExportReport r = ExportIm(q, o, &uv);
		CHECK(!r.ok);
		bool f = false;
		for (const Conflict& c : r.conflicts) if (c.kind == ConflictKind::File && c.other == "voice-pack" && c.blocks) f = true;
		CHECK(f);
		CHECK(!fs::exists(root / "UNI2-IM" / "Packs" / "pal-swap-pack"));
		q.packages[0].conflicts = {"voice-pack"};
		r = ExportIm(q, o, &uv);
		CHECK(r.ok);
		if (!r.ok) std::printf("%s", ReportText(r).c_str());
		const fs::path pk = root / "UNI2-IM" / "Packs" / "pal-swap-pack";
		CHECK_EQ(Get(pk / "data" / "chr025" / "chr025.pal"), "PAL1");
		CHECK(fs::exists(pk / "se" / "battle_se" / "SE000.wav"));
		const std::string ini = Get(pk / "mod.ini");
		CHECK(ini.find("[Mod]") != std::string::npos && ini.find("Name = Palette swap") != std::string::npos && ini.find("Version = 1.2.0") != std::string::npos && ini.find("Project = mymod") != std::string::npos);
		CHECK(r.outputs.size() == 1 && r.outputs[0].target == "im-packs");
		bool simWarn = false;
		for (const auto& w : r.warnings) if (w.find("simulation") != std::string::npos) simWarn = true;
		CHECK(simWarn);                                                                           // data\ ships: online warning
		// re-export is idempotent and updates in place
		Put(out / "proj_uni2" / "assets" / "pal-swap" / "data" / "chr025" / "chr025.pal", "PAL2");
		r = ExportIm(q, o, &uv);
		CHECK(r.ok);
		if (!r.ok) std::printf("%s", ReportText(r).c_str());
		CHECK_EQ(Get(pk / "data" / "chr025" / "chr025.pal"), "PAL2");
		// a removed asset file disappears from the pack on re-export
		fs::remove(out / "proj_uni2" / "assets" / "pal-swap" / "se" / "battle_se" / "SE000.wav");
		r = ExportIm(q, o, &uv);
		CHECK(r.ok);
		CHECK(!fs::exists(pk / "se" / "battle_se" / "SE000.wav"));
		// a folder this project did not write is never touched
		Put(root / "UNI2-IM" / "Packs" / "foreign" / "data" / "x.txt", "x");
		Project q2 = q;
		q2.packages[0].id = "foreign";
		q2.outputs.clear();
		r = ExportIm(q2, o, &uv);
		CHECK(!r.ok);
		CHECK_EQ(Get(root / "UNI2-IM" / "Packs" / "foreign" / "data" / "x.txt"), "x");
		// Mods target: refuses to overwrite a foreign file, writes when free
		Put(root / "UNI2-IM" / "Mods" / "data" / "chr025" / "chr025.pal", "THEIRS");
		o.imUsePacks = false;
		r = ExportIm(q, o, &uv);
		CHECK(!r.ok);
		CHECK_EQ(Get(root / "UNI2-IM" / "Mods" / "data" / "chr025" / "chr025.pal"), "THEIRS");
		fs::remove(root / "UNI2-IM" / "Mods" / "data" / "chr025" / "chr025.pal");
		r = ExportIm(q, o, &uv);
		CHECK(r.ok);
		CHECK_EQ(Get(root / "UNI2-IM" / "Mods" / "data" / "chr025" / "chr025.pal"), "PAL2");
		// the IM must be installed
		ExportOptions none; none.gameDir = (out / "empty").u8string();
		fs::create_directories(out / "empty");
		CHECK(!ExportIm(q, none, &uv).ok);
		// MBAACC project cannot use the IM export and vice versa
		CHECK(!ExportIm(p, o, &uv).ok);
		CHECK(!ExportPcmod(q, o, &uv).ok);
	}

	// ---- install import: a modded install's loose files against the vanilla index ----
	{
		std::string id, cat, why;
		ClassifyInstallPath("data\\m_hisui_m_0_c.txt", id, cat, why);
		CHECK_EQ(id, "char-m_hisui_m");
		ClassifyInstallPath("data\\s_akiha_1p.ha6", id, cat, why);
		CHECK_EQ(id, "char-s_akiha");
		ClassifyInstallPath("data\\p_arc_d_selist.txt", id, cat, why);
		CHECK_EQ(id, "char-p_arc_d");
		ClassifyInstallPath("data\\vector.txt", id, cat, why);
		CHECK(id == "system-constants" && cat == "system.constants");
		ClassifyInstallPath("data\\arc.wmt", id, cat, why);
		CHECK(id.empty() && why.find("write-only") != std::string::npos);
		ClassifyInstallPath("story\\battle\\arc_win.txt", id, cat, why);
		CHECK_EQ(id, "story");

		const fs::path inst = out / "modded";
		Put(inst / "MBAA.exe", "MZ");
		Put(inst / "data" / "akiha_1_r.HA6", Ha6({{300, "jab", Frame(9)}, {301, "stub", ""}}));                 // changed slot
		Put(inst / "data" / "akiha_1.HA6", Ha6({{300, "jab", Frame(5, true), true}, {301, "stub", ""}, {302, "x", Frame(3)}}));   // re-encoded only
		Put(inst / "data" / "same.txt", "same\r\n");                                                            // unchanged
		Put(inst / "data" / "_type.txt", "ShieldKeepTime = 45\r\nGG_MaxVal = 8000\r\n");                   // changed
		Put(inst / "data" / "eddie_0.txt", "[DataFile]\r\n");                                                 // added
		Put(inst / "data" / "eddie.wmt", "x");                                                                 // skipped
		Put(inst / "data" / "hantei4.inf", "x");                                                               // clutter
		Put(inst / "se" / "battle_se" / "Eddie_B000.wav", "RIFF");                                             // added
		Put(inst / "Tools" / "trainer.exe", "MZ");                                                             // not scanned
		Project q = MakeProject(out / "proj_import", "mbaacc");
		ImportInstallResult r;
		std::string err;
		CHECK(ImportInstall(q, inst.u8string(), van, r, err));
		if (!err.empty()) std::printf("  import: %s\n", err.c_str());
		CHECK(r.imported == 4 && r.reencoded == 1 && r.unchanged == 1 && r.clutter == 1 && r.skipped == 1);
		CHECK(r.perAsset["char-akiha"] == 1 && r.perAsset["char-eddie"] == 1 && r.perAsset["system-constants"] == 1 && r.perAsset["audio-se"] == 1);
		CHECK(q.FindAsset("char-akiha") && q.FindAsset("char-akiha")->origin.kind == "install-import" && q.FindAsset("char-akiha")->targets.size() == 1);
		CHECK(!fs::exists(out / "proj_import" / "assets" / "char-akiha" / "data" / "akiha_1.HA6"));
		CHECK(fs::exists(out / "proj_import" / "assets" / "char-akiha" / "data" / "akiha_1_r.HA6"));
		CHECK(q.sources.size() == 1 && q.sources[0].kind == "install-import");
		CHECK_EQ(Validate(q), "");
		Project bad = MakeProject(out / "proj_import_uni", "uni2");
		CHECK(!ImportInstall(bad, inst.u8string(), van, r, err));
	}

	if (g_failures) { std::printf("%d failure(s)\n", g_failures); return 1; }
	std::printf("hmod_test: all checks passed\n");
	return 0;
}
