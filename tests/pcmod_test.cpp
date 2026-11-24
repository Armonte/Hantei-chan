// pcmod schema 1 (docs/modmanager/pcmod-manifest_schema1.md): the four digests PovertyCaster pins for its fixture
// (tests/fixtures/pcmod; pins re-taken at PovertyCaster 96889671e), the ActCadenza P3 package digests (tests/fixtures/pcmod_p3),
// the §3 per-path class table, every §3 path refusal, the reserved/unknown-key refusals,
// a pack -> verify round trip, the zip layout (single top-level <id>/), and tamper detection on folder and archive.
#include "pcmod/pcmod_manifest.h"
#include "pcmod/pcmod_package.h"
#include "pcmod/pcmod_zip.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
static int g_failures = 0;
#define CHECK(expr) do { if (!(expr)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); ++g_failures; } } while (0)
#define CHECK_EQ(a, b) do { const std::string a_ = (a), b_ = (b); if (a_ != b_) { std::printf("FAIL %s:%d: %s\n  got      %s\n  expected %s\n", __FILE__, __LINE__, #a, a_.c_str(), b_.c_str()); ++g_failures; } } while (0)

static const std::string kExe = std::string(63, '0') + "1";
static const char* kPkgDigest = "eb3db61182ed3ee9875f68f6f668a1671ff3744340e073dffcbfc966998ddf27";
static const char* kSimDigest = "16a83aa066f3a94620eed7219529cb3b6463d158e080c02190345a1d1ddc6041";
static const char* kSetDigest = "8c4fe7957fcc0be5912de92c093dcab0a47d812b6d080e6ab5517588df7341cc";
static const char* kP3PkgDigest = "7721d18a3b3f8984582ca65a27ceea41992d6c9db48958a4ea6503750c4b2fe9";
static const char* kP3SimDigest = "d6d72ad9295f0354dcabcfeffe06d06d2b031a07488460fd87c329e040026ef8";
static const char* kVanillaSet = "f9956a1ae59ce26aa14e3c4d97597da9a665727f9b87539cd48432b10636c0db";
static const char* kFooSha = "2c26b46b68ffc68ff99b453c1d30413413422d706483bfa0f98a5e886266e7ae";

static std::string ReadText(const fs::path& p) { std::ifstream f(p, std::ios::binary); return std::string(std::istreambuf_iterator<char>(f), {}); }
static void WriteText(const fs::path& p, const std::string& s) { fs::create_directories(p.parent_path()); std::ofstream f(p, std::ios::binary); f << s; }

static pcmod::Manifest Minimal(pcmod::PackageClass cls, const std::string& path)
{
	pcmod::Manifest m;
	m.id = "x"; m.version = "1.0.0"; m.cls = cls; m.exe = {kExe};
	m.files.push_back({path, kFooSha, 3});
	return m;
}

int main(int argc, char** argv)
{
	const fs::path fixture = argc > 1 ? fs::u8path(argv[1]) : fs::path("tests/fixtures/pcmod");

	// ---- SHA-256 known answers (FIPS 180-4 examples) ----
	CHECK_EQ(pcmod::Sha256Hex(std::string()), "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
	CHECK_EQ(pcmod::Sha256Hex(std::string("abc")), "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
	CHECK_EQ(pcmod::Sha256Hex(std::string("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq")), "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
	{ std::string m(1000000, 'a'); CHECK_EQ(pcmod::Sha256Hex(m), "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0"); }

	// ---- the pinned fixture digests ----
	pcmod::Manifest fx;
	std::string err;
	CHECK(pcmod::ParseManifest(ReadText(fixture / "pcmod.json"), fx, err));
	if (!err.empty()) std::printf("  fixture: %s\n", err.c_str());
	CHECK(fx.files.size() == 3);
	if (fx.files.size() == 3) {
		CHECK_EQ(fx.files[1].key(), "data\\_type.txt");
		CHECK_EQ(fx.files[0].key(), "data\\sion_3.txt");
	}
	const std::string pkg = pcmod::PackageDigestHex(fx);
	const std::string sim = pcmod::PackageSimDigestHex(fx);
	CHECK_EQ(pkg, kPkgDigest);
	CHECK_EQ(sim, kSimDigest);
	CHECK_EQ(pcmod::SessionDigestHex(kExe, {{fx.id, fx.version, fx.priority, sim}}), kSetDigest);
	CHECK_EQ(pcmod::SessionDigestHex(kExe, {}), kVanillaSet);
	std::printf("package digest  %s\npackage sim     %s\nsession digest  %s\nvanilla set     %s\n", pkg.c_str(), sim.c_str(),
		pcmod::SessionDigestHex(kExe, {{fx.id, fx.version, fx.priority, sim}}).c_str(), pcmod::SessionDigestHex(kExe, {}).c_str());
	// The sim digest ignores cosmetic files (a sim package's art may differ between players) but not sim ones.
	{
		pcmod::Manifest a = fx;
		for (auto& f : a.files) if (f.key().rfind("grp\\", 0) == 0) f.size = 99;
		CHECK_EQ(pcmod::PackageSimDigestHex(a), kSimDigest);
		CHECK(pcmod::PackageDigestHex(a) != kPkgDigest);
		pcmod::Manifest b = fx;
		for (auto& f : b.files) if (f.key() == "data\\_type.txt") f.size = 99;
		CHECK(pcmod::PackageSimDigestHex(b) != kSimDigest);
	}
	// ActCadenza P3 (75 files, one data\*.pal outside the sim digest), rebuilt from the package's file list.
	{
		pcmod::Manifest p3;
		p3.id = "actcadenza"; p3.version = "1.0.3"; p3.cls = pcmod::PackageClass::Sim; p3.exe = {kExe};
		const fs::path tsv = fixture.parent_path() / "pcmod_p3" / "P3_PACKAGE_FILES.tsv";
		const std::string text = ReadText(tsv);
		size_t s0 = 0, pal = 0;
		while (s0 < text.size()) {
			size_t e = text.find('\n', s0);
			if (e == std::string::npos) e = text.size();
			std::string line = text.substr(s0, e - s0);
			s0 = e + 1;
			if (!line.empty() && line.back() == '\r') line.pop_back();
			if (line.empty() || line[0] == '#' || line.rfind("path\t", 0) == 0) continue;
			const size_t t1 = line.find('\t'), t2 = line.find('\t', t1 + 1), t3 = line.find('\t', t2 + 1);
			if (t1 == std::string::npos || t2 == std::string::npos) continue;
			pcmod::FileEntry f;
			f.path = line.substr(0, t1);
			f.sha256 = line.substr(t1 + 1, t2 - t1 - 1);
			f.size = std::stoull(line.substr(t2 + 1, t3 == std::string::npos ? std::string::npos : t3 - t2 - 1));
			if (pcmod::ClassifyPath(f.key()) == pcmod::PathClass::Cosmetic) ++pal;
			p3.files.push_back(f);
		}
		CHECK(p3.files.size() == 75);
		CHECK(pal == 1);
		CHECK(pcmod::Validate(p3).empty());
		CHECK_EQ(pcmod::PackageDigestHex(p3), kP3PkgDigest);
		CHECK_EQ(pcmod::PackageSimDigestHex(p3), kP3SimDigest);
		std::printf("P3 package      %s\nP3 package sim  %s\n", pcmod::PackageDigestHex(p3).c_str(), pcmod::PackageSimDigestHex(p3).c_str());
	}
	// The fixture's content files hash to what its manifest lists.
	{
		const pcmod::VerifyResult v = pcmod::VerifyDirectory(fixture.u8string(), /*requireFolderName*/ false);
		CHECK(v.ok);
		for (const auto& e : v.errors) std::printf("  fixture verify: %s\n", e.c_str());
		CHECK_EQ(v.digestHex, kPkgDigest);
	}
	// Display fields and the file order never move the digest; content, version and class do.
	{
		pcmod::Manifest a = fx;
		a.name = "other"; a.author = "x"; a.description = "y"; a.priority = -3;
		std::swap(a.files[0], a.files[2]);
		CHECK_EQ(pcmod::PackageDigestHex(a), kPkgDigest);
		pcmod::Manifest b = fx; b.version = "1.2.4";
		CHECK(pcmod::PackageDigestHex(b) != kPkgDigest);
		pcmod::Manifest c = fx; c.files[0].size = 4;
		CHECK(pcmod::PackageDigestHex(c) != kPkgDigest);
		pcmod::Manifest d = fx; d.files[0].path = "DATA/sion_3.TXT";   // spelling only
		CHECK_EQ(pcmod::PackageDigestHex(d), kPkgDigest);
	}
	// Session order: priority desc then id; input order irrelevant; cosmetic never passed in.
	{
		pcmod::SessionEntry a{"a", "1.0.0", 0, kSimDigest}, b{"b", "1.0.0", 5, kSimDigest}, c{"c", "1.0.0", 0, kSimDigest};
		CHECK_EQ(pcmod::SessionDigestHex(kExe, {a, b, c}), pcmod::SessionDigestHex(kExe, {c, a, b}));
		pcmod::SessionEntry b2 = b; b2.priority = -5;
		CHECK(pcmod::SessionDigestHex(kExe, {a, b, c}) != pcmod::SessionDigestHex(kExe, {a, b2, c}));
	}

	// ---- §3 path refusals, one per rule ----
	{
		auto refused = [](const std::string& raw) { std::string out, why; return !pcmod::NormalisePath(raw, out, why); };
		CHECK(refused(""));                              // empty
		CHECK(refused("\\data\\a.txt"));                 // leading '\'
		CHECK(refused("/data/a.txt"));                   // leading '/' (normalised to '\')
		CHECK(refused("c:data\\a.txt"));                 // ':' (drive)
		CHECK(refused("data\\a.txt:stream"));            // ':' (ADS)
		CHECK(refused("data\\..\\x.txt"));               // '..' segment
		CHECK(refused("..\\x.txt"));
		CHECK(refused("data\\.\\x.txt"));                // '.' segment
		CHECK(refused("data\\\\a.txt"));                 // empty segment
		CHECK(refused("data//a.txt"));
		CHECK(refused("data\\a.txt\\"));                 // trailing separator = empty segment
		CHECK(refused(std::string("data\\a\x01.txt")));  // control character
		CHECK(refused(std::string("data\\a\x7f.txt")));
		CHECK(refused("data\\" + std::string(236, 'a'))); // 241 bytes
		CHECK(!refused("data\\" + std::string(235, 'a'))); // 240 bytes is the limit
		CHECK(refused("data\\a.txt "));                  // trailing space
		CHECK(refused("data\\a.txt."));                  // trailing dot
		CHECK(refused("data \\a.txt"));                  // trailing space in a directory segment
		CHECK(refused("data\\con"));                     // DOS device names, with or without extension, any case
		CHECK(refused("data\\NUL.txt"));
		CHECK(refused("data\\com1.dat"));
		CHECK(refused("data\\LPT9"));
		CHECK(refused("aux\\a.txt"));
		CHECK(!refused("data\\com10.txt") && !refused("data\\console.txt"));
		std::string out, why;
		CHECK(pcmod::NormalisePath("DATA/Sion_3.TXT", out, why) && out == "data\\sion_3.txt");
		CHECK(pcmod::CanonicalPath("DATA/Sion_3.TXT", out, why) && out == "DATA\\Sion_3.TXT");
	}
	// §3 per-path class table: every row, plus the refused roots.
	{
		using pcmod::PathClass;
		const struct { const char* path; PathClass cls; } rows[] = {
			{"data\\sion_3.txt", PathClass::Sim}, {"data\\_type.txt", PathClass::Sim}, {"data\\sion_3.ha6", PathClass::Sim},
			{"data\\sub\\deep.cg", PathClass::Sim},
			{"data\\sion_3.pal", PathClass::Cosmetic}, {"data\\sub\\x.pal", PathClass::Cosmetic},
			{"data\\_csel\\a.png", PathClass::Cosmetic}, {"data\\_csel\\sub\\a.txt", PathClass::Cosmetic},
			{"data\\_cselx.txt", PathClass::Sim}, {"data\\pal.txt", PathClass::Sim},
			{"story\\arcade.txt", PathClass::Sim}, {"charaselect.txt", PathClass::Sim},
			{"bg\\stage.dat", PathClass::Cosmetic}, {"bgm\\a.ogg", PathClass::Cosmetic}, {"dat\\title.mpg", PathClass::Cosmetic},
			{"grp\\gauge_aa\\g.png", PathClass::Cosmetic}, {"se\\a.wav", PathClass::Cosmetic}, {"shader\\a.fx", PathClass::Cosmetic},
			{"movie\\op.wmv", PathClass::Cosmetic}, {"palettes\\a.pal", PathClass::Cosmetic},
			{"hud_theme.json", PathClass::Cosmetic}, {"font.fnt", PathClass::Cosmetic}, {"_sysfont.ini", PathClass::Cosmetic},
			// refused (fail closed): unclassified roots, root files, sound\ (dropped), near-misses of a real root
			{"packdataversion\\pack_0008_version.txt", PathClass::Refused}, {"system\\a.ini", PathClass::Refused},
			{"information\\a.txt", PathClass::Refused}, {"sound\\a.ogg", PathClass::Refused}, {"mbaa.exe", PathClass::Refused},
			{"readme.txt", PathClass::Refused}, {"0002.p", PathClass::Refused}, {"bin\\x.dll", PathClass::Refused},
			{"data", PathClass::Refused}, {"story", PathClass::Refused}, {"datax\\a.txt", PathClass::Refused},
			{"charaselect.txt\\x", PathClass::Refused}, {"hud_theme.json\\x", PathClass::Refused}, {"x\\charaselect.txt", PathClass::Refused},
		};
		for (const auto& r : rows) {
			const PathClass got = pcmod::ClassifyPath(r.path);
			if (got != r.cls) std::printf("FAIL class %s: got %s expected %s\n", r.path, pcmod::PathClassName(got), pcmod::PathClassName(r.cls));
			CHECK(got == r.cls);
			// The manifest's case and separators do not matter: classification runs on the normalised key.
			std::string upper = r.path, norm, why;
			for (char& ch : upper) { if (ch >= 'a' && ch <= 'z') ch = char(ch - 'a' + 'A'); if (ch == '\\') ch = '/'; }
			if (pcmod::NormalisePath(upper, norm, why)) CHECK(pcmod::ClassifyPath(norm) == r.cls);
		}
	}
	// Package class vs file classes.
	{
		using pcmod::PackageClass;
		CHECK(pcmod::Validate(Minimal(PackageClass::Sim, "data\\a.txt")).empty());
		CHECK(pcmod::Validate(Minimal(PackageClass::Sim, "story\\a.txt")).empty());
		CHECK(pcmod::Validate(Minimal(PackageClass::Sim, "charaselect.txt")).empty());
		CHECK(!pcmod::Validate(Minimal(PackageClass::Sim, "grp\\a.png")).empty());          // sim needs >= 1 sim-class file
		CHECK(!pcmod::Validate(Minimal(PackageClass::Sim, "data\\a.pal")).empty());         // a .pal under data\ is cosmetic
		pcmod::Manifest mixed = Minimal(PackageClass::Sim, "data\\a.txt");
		mixed.files.push_back({"grp\\a.png", kFooSha, 3});
		mixed.files.push_back({"data\\a.pal", kFooSha, 3});
		CHECK(pcmod::Validate(mixed).empty());                                               // sim may carry cosmetic files
		for (const char* p : {"bin\\x.dll", "MBAA.exe", "PackDataVersion\\v.txt", "System\\a.ini", "Information\\a.txt", "sound\\a.ogg"}) {
			CHECK(!pcmod::Validate(Minimal(PackageClass::Sim, p)).empty());
			CHECK(!pcmod::Validate(Minimal(PackageClass::Cosmetic, p)).empty());
		}
		for (const char* p : {"data\\a.txt", "story\\a.txt", "charaselect.txt"})
			CHECK(!pcmod::Validate(Minimal(PackageClass::Cosmetic, p)).empty());              // cosmetic may list no sim file
		for (const char* p : {"grp\\a.png", "se\\a.wav", "bg\\a.dat", "bgm\\a.ogg", "dat\\a.mpg", "shader\\a.fx", "movie\\a.wmv",
		                      "palettes\\a.pal", "hud_theme.json", "font.fnt", "_sysfont.ini", "data\\a.pal", "data\\_csel\\a.png"})
			CHECK(pcmod::Validate(Minimal(PackageClass::Cosmetic, p)).empty());
		CHECK(!pcmod::Validate(Minimal(PackageClass::Cosmetic, "hud_theme.json\\x")).empty());
		// data\**.wmt is written by the game (ResultMenu_LoadWinQuoteData) and never read: refused, with that reason.
		for (const char* p : {"data\\SION.wmt", "data\\sub\\a.WMT"}) {
			std::string n, why;
			CHECK(pcmod::NormalisePath(p, n, why));
			CHECK(pcmod::IsGameWrittenPath(n));
			CHECK(pcmod::ClassifyPath(n) == pcmod::PathClass::Refused);
			CHECK(pcmod::RefusedPathReason(n).find("written by the game, never read") == 0);
			pcmod::Manifest w = Minimal(PackageClass::Sim, "data\\a.txt");
			w.files.push_back({p, kFooSha, 3});
			CHECK(pcmod::Validate(w).find("written by the game, never read") != std::string::npos);
		}
		CHECK(!pcmod::IsGameWrittenPath("story\\battle\\sion_win.txt"));   // the real win-quote input stays sim
		CHECK(pcmod::ClassifyPath("story\\battle\\sion_win.txt") == pcmod::PathClass::Sim);
		CHECK(!pcmod::IsGameWrittenPath("grp\\a.wmt") && pcmod::RefusedPathReason("data\\a.txt").empty());
		pcmod::Manifest dup = Minimal(PackageClass::Sim, "data\\a.txt");
		dup.files.push_back({"DATA/A.TXT", kFooSha, 3});
		CHECK(!pcmod::Validate(dup).empty());                                                // one file, two spellings
		pcmod::Manifest noExe = Minimal(PackageClass::Sim, "data\\a.txt"); noExe.exe.clear();
		CHECK(!pcmod::Validate(noExe).empty());
		pcmod::Manifest cosNoExe = Minimal(PackageClass::Cosmetic, "grp\\a.png"); cosNoExe.exe.clear();
		CHECK(pcmod::Validate(cosNoExe).empty());
		// A cosmetic package's sim digest is the header alone; its package digest still covers every file.
		const pcmod::Manifest cos = Minimal(PackageClass::Cosmetic, "grp\\a.png");
		CHECK_EQ(pcmod::PackageSimDigestHex(cos), pcmod::Sha256Hex(std::string("pcmod-sim-1\nx\n1.0.0\n")));
	}
	// Ids / versions.
	CHECK(!pcmod::ValidId("Act") && !pcmod::ValidId("-a") && !pcmod::ValidId("") && pcmod::ValidId("act.cadenza_v1-2"));
	CHECK(!pcmod::ValidVersion("1.0") && !pcmod::ValidVersion("1.0.0-rc1") && !pcmod::ValidVersion("01.0.0") && !pcmod::ValidVersion("1.0.65536"));
	CHECK(pcmod::ValidVersion("0.10.65535") && pcmod::CompareVersion("1.10.0", "1.9.9") == 1);

	// ---- the reader fails closed; the writer never emits reserved keys and round-trips ----
	{
		const std::string base = pcmod::WriteManifest(Minimal(pcmod::PackageClass::Sim, "data\\a.txt"));
		pcmod::Manifest m;
		CHECK(pcmod::ParseManifest(base, m, err));
		CHECK(pcmod::WriteManifest(m) == base);
		CHECK(base.find("inject") == std::string::npos && base.find("palettes") == std::string::npos && base.find("exe_layers") == std::string::npos);
		auto with = [&](const std::string& extra) { std::string t = base; t.insert(t.find('{') + 1, extra); return t; };
		CHECK(!pcmod::ParseManifest(with("\"inject\": [],"), m, err));
		CHECK(!pcmod::ParseManifest(with("\"palettes\": [],"), m, err));
		CHECK(!pcmod::ParseManifest(with("\"exe_layers\": [],"), m, err));
		CHECK(!pcmod::ParseManifest(with("\"color\": \"red\","), m, err));
		CHECK(!pcmod::ParseManifest(with("\"name\": \"dup\","), m, err));           // duplicate key
		std::string t = base; t.replace(t.find("\"priority\": 0"), 13, "\"priority\": 1.5");
		CHECK(!pcmod::ParseManifest(t, m, err));
		t = base; t.replace(t.find("\"priority\": 0"), 13, "\"priority\": 5000");
		CHECK(!pcmod::ParseManifest(t, m, err));
		// schema 2 (PC S2.1) is now understood (tests/pcmod_s2_test.cpp); schema 3 is not.
		t = base; t.replace(t.find("\"schema\": 1"), 11, "\"schema\": 2");
		CHECK(pcmod::ParseManifest(t, m, err) && m.schema == 2);
		t = base; t.replace(t.find("\"schema\": 1"), 11, "\"schema\": 3");
		CHECK(!pcmod::ParseManifest(t, m, err));
		CHECK(!pcmod::ParseManifest(base + "x", m, err));

		// ---- "modes" (§2, added 2026-10-15, still schema 1): optional, <= 8 ids, written only when non-empty ----
		CHECK(base.find("modes") == std::string::npos);                              // absent/empty: not written
		CHECK(pcmod::ParseManifest(base, m, err) && m.modes.empty());
		CHECK(pcmod::ParseManifest(with("\"modes\": [],"), m, err) && m.modes.empty());
		CHECK(pcmod::WriteManifest(m) == base);                                     // empty list round-trips to no key
		CHECK(pcmod::ParseManifest(with("\"modes\": [\"tag\", \"team\"],"), m, err));
		CHECK(m.modes == std::vector<std::string>({"tag", "team"}));
		const std::string withModes = pcmod::WriteManifest(m);
		CHECK(withModes.find("\"modes\": [\"tag\", \"team\"]") != std::string::npos);
		pcmod::Manifest back;
		CHECK(pcmod::ParseManifest(withModes, back, err) && back.modes == m.modes && pcmod::WriteManifest(back) == withModes);
		CHECK(pcmod::ParseManifest(with("\"modes\": [\"a\",\"b\",\"c\",\"d\",\"e\",\"f\",\"g\",\"h\"],"), m, err));   // 8 = the limit
		CHECK(!pcmod::ParseManifest(with("\"modes\": [\"a\",\"b\",\"c\",\"d\",\"e\",\"f\",\"g\",\"h\",\"i\"],"), m, err));
		CHECK(!pcmod::ParseManifest(with("\"modes\": [\"Tag\"],"), m, err));        // id grammar: lowercase
		CHECK(!pcmod::ParseManifest(with("\"modes\": [\"\"],"), m, err));
		CHECK(!pcmod::ParseManifest(with("\"modes\": [\"-tag\"],"), m, err));
		CHECK(!pcmod::ParseManifest(with("\"modes\": [\"" + std::string(65, 'a') + "\"],"), m, err));
		CHECK(!pcmod::ParseManifest(with("\"modes\": \"tag\","), m, err));          // not an array
		CHECK(!pcmod::ParseManifest(with("\"modes\": [1],"), m, err));              // not a string
		CHECK(!pcmod::ParseManifest(with("\"modes\": [\"tag\"], \"modes\": [],"), m, err));   // duplicate key
		pcmod::Manifest tooMany = Minimal(pcmod::PackageClass::Sim, "data\\a.txt");
		tooMany.modes = {"a", "b", "c", "d", "e", "f", "g", "h", "i"};
		CHECK(!pcmod::Validate(tooMany).empty());
		tooMany.modes = {"vs", "Bad"};
		CHECK(!pcmod::Validate(tooMany).empty());
		CHECK(pcmod::SplitModes(" tag, team,,tag ;vs") == std::vector<std::string>({"tag", "team", "vs"}));
		CHECK_EQ(pcmod::JoinModes({"tag", "team"}), "tag,team");
		CHECK(pcmod::CountsForMode({}, "vs") && pcmod::CountsForMode({"tag"}, "tag") && pcmod::CountsForMode({"tag"}, ""));
		CHECK(!pcmod::CountsForMode({"tag", "team"}, "vs"));
	}
	// modes is NOT part of any digest: the fixture pins hold with and without it.
	{
		pcmod::Manifest t = fx;
		t.modes = {"tag", "team"};
		CHECK_EQ(pcmod::PackageDigestHex(t), kPkgDigest);
		CHECK_EQ(pcmod::PackageSimDigestHex(t), kSimDigest);
		CHECK_EQ(pcmod::SessionDigestHex(kExe, {{t.id, t.version, t.priority, pcmod::PackageSimDigestHex(t)}}), kSetDigest);
	}

	// ---- round trip: pack the fixture content -> folder + zip -> verify both ----
	std::error_code ec;
	const fs::path tmp = fs::temp_directory_path() / "hantei_pcmod_test";
	fs::remove_all(tmp, ec);
	fs::create_directories(tmp);
	{
		std::vector<pcmod::SourceFile> src;
		CHECK(pcmod::ScanSourceDir(fixture.u8string(), src, err));
		std::vector<pcmod::SourceFile> content;
		for (auto& s : src) if (s.rel.find("ORIGIN") == std::string::npos && s.rel.find(".gitattributes") == std::string::npos) content.push_back(s);
		CHECK(content.size() == 3);
		pcmod::Manifest meta = fx;   // same identity as the fixture
		meta.files.clear();
		const pcmod::PackResult r = pcmod::Pack(content, meta, (tmp / "out").u8string(), true);
		if (!r.ok) std::printf("  pack: %s\n", r.error.c_str());
		CHECK(r.ok);
		CHECK_EQ(r.digestHex, kPkgDigest);
		CHECK_EQ(r.simDigestHex, kSimDigest);
		CHECK(fs::exists(tmp / "out" / "pcmod-fixture" / "pcmod.json"));
		CHECK(fs::exists(tmp / "out" / "pcmod-fixture" / "data" / "SION_3.txt"));
		CHECK(fs::exists(tmp / "out" / "pcmod-fixture-1.2.3.zip"));
		const pcmod::VerifyResult vd = pcmod::VerifyPath((tmp / "out" / "pcmod-fixture").u8string());
		CHECK(vd.ok && vd.digestHex == kPkgDigest);
		const pcmod::VerifyResult vz = pcmod::VerifyPath((tmp / "out" / "pcmod-fixture-1.2.3.zip").u8string());
		CHECK(vz.ok && vz.digestHex == kPkgDigest && vz.simDigestHex == kSimDigest);
		for (const auto& e : vz.errors) std::printf("  zip verify: %s\n", e.c_str());

		// Zip layout: one top-level pcmod-fixture/ directory, '/' names, store-only, pcmod.json first.
		std::vector<uint8_t> zb;
		CHECK(pcmod::ReadAll((tmp / "out" / "pcmod-fixture-1.2.3.zip").u8string(), zb));
		std::vector<pcmod::ZipEntry> ze;
		CHECK(pcmod::ReadZip(zb, ze, err));
		CHECK(ze.size() == 5);
		if (ze.size() == 5) {
			CHECK_EQ(ze[0].name, "pcmod-fixture/");
			CHECK_EQ(ze[1].name, "pcmod-fixture/pcmod.json");
			CHECK_EQ(ze[2].name, "pcmod-fixture/data/_type.txt");
			CHECK_EQ(ze[3].name, "pcmod-fixture/data/SION_3.txt");
			CHECK_EQ(ze[4].name, "pcmod-fixture/grp/Gauge_AA/gauge00.png");
		}
		for (const auto& e : ze) CHECK(e.name.rfind("pcmod-fixture/", 0) == 0);
		// Deterministic: packing again gives byte-identical archive and manifest.
		const std::string json1 = ReadText(tmp / "out" / "pcmod-fixture" / "pcmod.json");
		const pcmod::PackResult r2 = pcmod::Pack(content, meta, (tmp / "out").u8string(), true);
		CHECK(r2.ok);
		std::vector<uint8_t> zb2;
		pcmod::ReadAll((tmp / "out" / "pcmod-fixture-1.2.3.zip").u8string(), zb2);
		CHECK(zb == zb2);
		CHECK(ReadText(tmp / "out" / "pcmod-fixture" / "pcmod.json") == json1);

		// Tamper: an extra entry outside the listed paths, a second top-level dir, changed bytes.
		auto zipWith = [&](std::vector<pcmod::ZipEntry> e) { std::vector<uint8_t> o; std::string w; pcmod::BuildZip(e, o, w); return pcmod::VerifyZipBytes(o); };
		auto extra = ze; extra.push_back({"pcmod-fixture/data/evil.txt", {'x'}});
		CHECK(!zipWith(extra).ok);
		auto twoTop = ze; twoTop.push_back({"other/data/a.txt", {'x'}});
		CHECK(!zipWith(twoTop).ok);
		auto changed = ze; changed[3].data = {'f', 'o', 'x'};
		CHECK(!zipWith(changed).ok);
		auto missing = ze; missing.pop_back();
		CHECK(!zipWith(missing).ok);
		auto withIni = ze; withIni.push_back({"pcmod-fixture/mod.ini", {'[', ']'}});
		CHECK(zipWith(withIni).ok);
		auto traversal = ze; traversal.push_back({"pcmod-fixture/../x.txt", {'x'}});
		CHECK(!zipWith(traversal).ok);

		// Folder tamper: changed bytes -> broken; an unlisted file -> warning only (never served); wrong folder name.
		WriteText(tmp / "out" / "pcmod-fixture" / "data" / "extra.txt", "x");
		pcmod::VerifyResult v = pcmod::VerifyDirectory((tmp / "out" / "pcmod-fixture").u8string());
		CHECK(v.ok && !v.warnings.empty());
		WriteText(tmp / "out" / "pcmod-fixture" / "data" / "_type.txt", "bas");
		v = pcmod::VerifyDirectory((tmp / "out" / "pcmod-fixture").u8string());
		CHECK(!v.ok);
		fs::rename(tmp / "out" / "pcmod-fixture", tmp / "out" / "renamed", ec);
		WriteText(tmp / "out" / "renamed" / "data" / "_type.txt", "bar");
		v = pcmod::VerifyDirectory((tmp / "out" / "renamed").u8string());
		CHECK(!v.ok);   // folder name != id

		// Pack refusals: cosmetic with data\, bad id, a non-package folder in the way.
		pcmod::Manifest cos = meta; cos.cls = pcmod::PackageClass::Cosmetic; cos.id = "cos";
		CHECK(!pcmod::Pack(content, cos, (tmp / "out").u8string(), false).ok);
		pcmod::Manifest badId = meta; badId.id = "Bad";
		CHECK(!pcmod::Pack(content, badId, (tmp / "out").u8string(), false).ok);
		WriteText(tmp / "out" / "busy" / "keep.txt", "x");
		pcmod::Manifest busy = meta; busy.id = "busy";
		CHECK(!pcmod::Pack(content, busy, (tmp / "out").u8string(), false).ok);
		CHECK(fs::exists(tmp / "out" / "busy" / "keep.txt"));

		// mod.ini local state
		WriteText(tmp / "ini" / "mod.ini", "[Config]\r\nenabled=0\r\npriority = 7\r\n");
		bool en = true; int pr = 0;
		CHECK(pcmod::ReadModIni((tmp / "ini").u8string(), en, pr) && !en && pr == 7);
	}
	// ---- per-mode session set (§6): a sim package listing only "tag" stays out of a "vs" session's set ----
	{
		std::vector<pcmod::SourceFile> src, content;
		CHECK(pcmod::ScanSourceDir(fixture.u8string(), src, err));
		for (auto& s : src) if (s.rel.find("ORIGIN") == std::string::npos && s.rel.find(".gitattributes") == std::string::npos) content.push_back(s);
		pcmod::Manifest meta = fx;
		meta.files.clear();
		meta.modes = {"tag"};
		const pcmod::PackResult r = pcmod::Pack(content, meta, (tmp / "modes").u8string(), true);
		CHECK(r.ok);
		CHECK_EQ(r.digestHex, kPkgDigest);       // unchanged by modes
		CHECK_EQ(r.simDigestHex, kSimDigest);
		const std::string dir = (tmp / "modes" / "pcmod-fixture").u8string();
		const pcmod::VerifyResult vz = pcmod::VerifyPath((tmp / "modes" / "pcmod-fixture-1.2.3.zip").u8string());
		CHECK(vz.ok && vz.manifest.modes == std::vector<std::string>({"tag"}) && vz.digestHex == kPkgDigest);
		pcmod::SessionResult s = pcmod::ResolveSession(kExe, {dir});
		CHECK(s.ok && s.simOrder.size() == 1);
		CHECK_EQ(s.digestHex, kSetDigest);       // no mode filter = the plain session sim digest
		s = pcmod::ResolveSession(kExe, {dir}, "tag");
		CHECK(s.ok && s.simOrder.size() == 1);
		CHECK_EQ(s.digestHex, kSetDigest);
		s = pcmod::ResolveSession(kExe, {dir}, "vs");
		CHECK(s.ok && s.simOrder.empty() && !s.notes.empty());
		CHECK_EQ(s.digestHex, kVanillaSet);      // plain VS does not demand the tag-only package
		// Without modes the package counts for every mode.
		meta.modes.clear();
		CHECK(pcmod::Pack(content, meta, (tmp / "modes").u8string(), false).ok);
		s = pcmod::ResolveSession(kExe, {dir}, "vs");
		CHECK(s.ok && s.simOrder.size() == 1);
		CHECK_EQ(s.digestHex, kSetDigest);
	}
	fs::remove_all(tmp, ec);
	if (g_failures) { std::printf("pcmod_test: %d failure(s)\n", g_failures); return 1; }
	std::printf("pcmod_test: PASS\n");
	return 0;
}
