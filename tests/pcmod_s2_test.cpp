// pcmod schema 2 (PovertyCaster docs/design/pcmod-manifest.md S2.1-S2.8): the digests PovertyCaster's own code computes for
// tests/fixtures/pcmod_s2 (regenerate with tools/pcmod/s2_vectors_from_pc.sh), export round trip, author-time verification
// against a SYNTHETIC PE32 image (no real MBAA.exe needed), the named refusals, and digest stability.
#include "pcmod/pcmod_manifest.h"
#include "pcmod/pcmod_schema2.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

using namespace pcmod;
namespace S = pcmod::s2;
static int g_failures = 0;
#define CHECK(expr) do { if (!(expr)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); ++g_failures; } } while (0)
#define CHECK_EQ(a, b) do { const std::string a_ = (a), b_ = (b); if (a_ != b_) { std::printf("FAIL %s:%d: %s\n  got      %s\n  expected %s\n", __FILE__, __LINE__, #a, a_.c_str(), b_.c_str()); ++g_failures; } } while (0)
#define CHECK_HAS(s, w) do { const std::string s_ = (s); if (s_.find(w) == std::string::npos) { std::printf("FAIL %s:%d: '%s' lacks '%s'\n", __FILE__, __LINE__, s_.c_str(), w); ++g_failures; } } while (0)

// PovertyCaster mods/pcmod-schema2 (49bcaa321) digests of tests/fixtures/pcmod_s2 under its test profile.
static const char* kPcPkg = "1086c27689637bfc983c1cdc59b50edf4ac5b6268d6f39611d352aae421c832f";
static const char* kPcSim = "71ea15737164626dc9b6ceea267515fdcb1b4dd31161f27186563f4d92fdc294";
static const char* kPcSet = "cbed787abc0663192d59d47ed55a295dd5e91cc3a08300c038a6d0f2b902c808";
static const char* kPcCanon =
	"patches\n"
	"P00456789 e811223344 call vanilla:00412340\n"
	"P00470100 8b048500112233 jmp vanilla:00480000\n"
	"P004a0010 83f805 raw 83f806\n"
	"features\n"
	"Fcustom-vars abi=1 slots=64\n"
	"rules\n"
	"Rhp=12000\n";
static const std::string kExe = std::string(63, '0') + "1";

static std::string ReadText(const std::string& p) { std::ifstream f(p, std::ios::binary); return std::string(std::istreambuf_iterator<char>(f), {}); }

// The PC test profile (tests/pcmod_resolve s2 block).
static S::Profile PcTestProfile()
{
	S::Profile p;
	p.abiId = "test-prim-1";
	p.imageBegin = 0x401000; p.imageEnd = 0x560000; p.textBegin = 0x401000; p.textEnd = 0x54B000;
	p.reserved = { { 0x00450000, 8, "boot patch: test site" } };
	p.callPrimitives = { "test_hook" };
	p.features = { { "custom-vars", 1, { { "slots", 1, 1024 } } } };
	p.rules = { { "hp", 1, 99999 } };
	return p;
}
// An MBAACC-shaped profile for the synthetic exe: .text/.rdata patchable, .data only via css_roster.
static S::Profile MbaaccLikeProfile()
{
	S::Profile p = PcTestProfile();
	p.imageEnd = 0x54B000; p.textEnd = 0x51B000;
	p.dataTables = { { "css_roster", 0x551D30, 0x551DF0 } };
	return p;
}

// ---- a synthetic PE32: headers, .text 0x401000, .rdata 0x51C000, .data 0x54B000, .rsrc 0x560000 (image base 0x400000) ----
struct Sec { const char* name; uint32_t rva, size; };
static const Sec kSecs[] = { { ".text", 0x1000, 0x11A000 }, { ".rdata", 0x11C000, 0x2F000 }, { ".data", 0x14B000, 0x15000 }, { ".rsrc", 0x160000, 0x2000 } };
static void Put32(std::vector<uint8_t>& b, size_t o, uint32_t v) { for (int i = 0; i < 4; ++i) b[o + i] = (uint8_t)(v >> (8 * i)); }
static void Put16(std::vector<uint8_t>& b, size_t o, uint16_t v) { b[o] = (uint8_t)v; b[o + 1] = (uint8_t)(v >> 8); }
static std::vector<uint8_t> MakePe(std::vector<std::pair<uint32_t, std::vector<uint8_t>>> content)
{
	const uint32_t hdr = 0x400;
	uint32_t raw = hdr;
	std::vector<uint32_t> rawOff;
	for (const Sec& s : kSecs) { rawOff.push_back(raw); raw += s.size; }
	std::vector<uint8_t> b(raw, 0);
	b[0] = 'M'; b[1] = 'Z';
	Put32(b, 0x3C, 0x80);
	std::memcpy(&b[0x80], "PE\0\0", 4);
	Put16(b, 0x84, 0x14C);
	Put16(b, 0x86, (uint16_t)(sizeof kSecs / sizeof kSecs[0]));
	Put16(b, 0x94, 0xE0);
	const size_t opt = 0x98;
	Put16(b, opt, 0x10B);
	Put32(b, opt + 28, 0x400000);
	Put32(b, opt + 60, hdr);
	size_t sh = opt + 0xE0;
	for (size_t i = 0; i < sizeof kSecs / sizeof kSecs[0]; ++i, sh += 40) {
		std::memcpy(&b[sh], kSecs[i].name, std::strlen(kSecs[i].name));
		Put32(b, sh + 8, kSecs[i].size); Put32(b, sh + 12, kSecs[i].rva);
		Put32(b, sh + 16, kSecs[i].size); Put32(b, sh + 20, rawOff[i]);
	}
	for (auto& [va, bytes] : content) {
		for (size_t i = 0; i < sizeof kSecs / sizeof kSecs[0]; ++i) {
			const uint32_t rva = va - 0x400000;
			if (rva >= kSecs[i].rva && rva < kSecs[i].rva + kSecs[i].size)
				std::memcpy(&b[rawOff[i] + (rva - kSecs[i].rva)], bytes.data(), bytes.size());
		}
	}
	return b;
}
static std::vector<uint8_t> Hx(const char* s) { std::vector<uint8_t> v; S::ParseHexBytes(s, v); return v; }

static S::Patch Raw(uint32_t a, const char* o, const char* n) { S::Patch p; p.addr = a; p.orig = Hx(o); p.raw = Hx(n); return p; }
static S::Patch Xfer(uint32_t a, const char* o, S::Kind k, uint32_t t) { S::Patch p; p.addr = a; p.orig = Hx(o); p.kind = k; p.toVanilla = true; p.target = t; return p; }

// gonp-like authoring data: a call redirect, a switch case -> jump-table dword in .rdata, a one-byte cmp change.
static S::Data GonpLike()
{
	S::Data d;
	d.patches.push_back(Xfer(0x456789, "E8 11 22 33 44", S::Kind::Call, 0x412340));
	d.patches.push_back(Raw(0x530010, "00 01 47 00", "40 23 41 00"));   // case-jmp: jump-table entry in .rdata
	d.patches.push_back(Raw(0x4A0012, "05", "06"));                     // cmp eax,5 -> 6 (the imm8)
	return d;
}
static std::vector<uint8_t> GonpExe()
{
	return MakePe({ { 0x456789, Hx("E8 11 22 33 44") }, { 0x530010, Hx("00 01 47 00") }, { 0x4A0010, Hx("83 F8 05") },
	                { 0x412340, Hx("55 8B EC") }, { 0x551D30, Hx("16 00 00 00") } });
}

static Manifest BaseManifest()
{
	Manifest m;
	m.id = "gonpy"; m.version = "1.0.0"; m.game = "mbaacc"; m.cls = PackageClass::Sim; m.exe = { kExe };
	m.files.push_back({ "data/a.txt", "2c26b46b68ffc68ff99b453c1d30413413422d706483bfa0f98a5e886266e7ae", 3 });
	return m;
}

static bool HasCode(const std::vector<S::Issue>& v, S::IssueCode c) { for (auto& i : v) if (i.code == c) return true; return false; }

int main(int argc, char** argv)
{
	const std::string fx = argc > 1 ? argv[1] : "tests/fixtures/pcmod_s2";

	// ---- 1. bit-exact with PovertyCaster's own digests ----
	{
		Manifest m;
		std::string err;
		CHECK(ParseManifest(ReadText(fx + "/pcmod.json"), m, err));
		CHECK_EQ(err, "");
		CHECK(m.schema == 2 && m.exePatches.present && m.features.present && m.rules.present);
		S::Files t{ ReadText(fx + "/patches.toml"), ReadText(fx + "/features.toml"), ReadText(fx + "/rules.toml") };
		S::Data d;
		CHECK_EQ(S::LoadFromTexts(m, t, PcTestProfile(), d), "");
		CHECK_EQ(m.s2Canon, kPcCanon);
		CHECK_EQ(PackageDigestHex(m), kPcPkg);
		CHECK_EQ(PackageSimDigestHex(m), kPcSim);
		CHECK_EQ(SessionDigestHex(m.exe[0], { { m.id, m.version, 0, PackageSimDigestHex(m), 2 } }, "test-prim-1"), kPcSet);
		// schema-1 sets keep set-1; the ABI id is in set-2
		CHECK(SessionDigestHex(kExe, {}, "x") == "f9956a1ae59ce26aa14e3c4d97597da9a665727f9b87539cd48432b10636c0db");
		CHECK(SessionDigestHex(m.exe[0], { { m.id, m.version, 0, PackageSimDigestHex(m), 2 } }, "other-abi") != kPcSet);
		// edited bytes on disk = broken, named
		S::Files t2 = t; t2.rules += "\n";
		CHECK_HAS(S::LoadFromTexts(m, t2, PcTestProfile(), d), "rules.toml differs from its manifest");
		// writer round trip keeps the manifest (and so the package digest)
		Manifest m2;
		CHECK(ParseManifest(WriteManifest(m), m2, err));
		m2.s2Canon = kPcCanon;   // (the failed load above cleared m.s2Canon)
		CHECK_EQ(PackageDigestHex(m2), kPcPkg);
		CHECK_EQ(PackageSimDigestHex(m2), kPcSim);
	}

	// ---- 2. export: attach -> write -> reparse -> verify against the synthetic exe ----
	S::PeImage exe;
	{
		std::string err;
		CHECK(S::ParsePe(GonpExe(), exe, err));
		CHECK(exe.imageBase == 0x400000 && exe.sections.size() == 4);
		CHECK(exe.SectionAt(0x530010) && exe.SectionAt(0x530010)->name == ".rdata");
	}
	{
		const S::Profile pf = MbaaccLikeProfile();
		Manifest m = BaseManifest();
		S::Files f;
		CHECK_EQ(S::AttachToManifest(m, GonpLike(), pf, f), "");
		CHECK(m.schema == 2 && m.exePatches.present && !m.features.present && !m.rules.present);
		CHECK(f.features.empty() && f.rules.empty());
		CHECK_HAS(f.patches, "call = \"vanilla:0x00412340\"");
		CHECK_EQ(Validate(m), "");
		Manifest back;
		std::string err;
		CHECK(ParseManifest(WriteManifest(m), back, err));
		CHECK_EQ(err, "");
		S::Data d;
		CHECK_EQ(S::LoadFromTexts(back, f, pf, d), "");
		CHECK_EQ(back.s2Canon, m.s2Canon);
		CHECK_EQ(PackageSimDigestHex(back), PackageSimDigestHex(m));
		CHECK_HAS(m.s2Canon, "P00530010 00014700 raw 40234100\n");
		CHECK_HAS(m.s2Canon, "P004a0012 05 raw 06\n");
		const auto issues = S::VerifyPatches(d, exe, pf);
		for (const auto& i : issues) std::printf("  unexpected issue: %s %s\n", S::IssueName(i.code), i.message.c_str());
		CHECK(issues.empty());
		std::vector<uint8_t> b;
		CHECK(S::PatchBytes(d.patches[0], b) && S::HexBytes(b) == "e8b2bbfbff");   // 0x412340 - (0x456789 + 5)
		// css_roster: a .data write inside the allow-listed table passes; one byte outside it does not
		S::Data t; t.patches.push_back(Raw(0x551D30, "16 00 00 00", "FF FF FF FF"));
		CHECK(S::VerifyPatches(t, exe, pf).empty());
		t.patches[0].addr = 0x551D2C;
		CHECK(HasCode(S::VerifyPatches(t, exe, pf), S::IssueCode::OutOfRange));
	}

	// ---- 3. named refusals ----
	{
		const S::Profile pf = MbaaccLikeProfile();
		auto one = [&](S::Patch p) { S::Data d; d.patches.push_back(p); return S::VerifyPatches(d, exe, pf); };
		const auto mism = one(Raw(0x4A0012, "07", "06"));
		CHECK(HasCode(mism, S::IssueCode::OrigMismatch));
		CHECK(!mism.empty() && mism[0].message == "patch @0x004a0012: orig does not match the vanilla executable (found 05)");
		CHECK(HasCode(one(Raw(0x54B100, "00", "01")), S::IssueCode::OutOfRange));          // .data, no table
		CHECK(HasCode(one(Raw(0x560010, "00", "01")), S::IssueCode::Resources));           // .rsrc
		CHECK(HasCode(one(Raw(0x40011E, "00", "20")), S::IssueCode::PeHeader));            // LAA flag
		CHECK(HasCode(one(Raw(0x700000, "00", "01")), S::IssueCode::OutOfRange));          // unmapped
		CHECK(HasCode(one(Xfer(0x4A0010, "83 F8 05", S::Kind::Call, 0x412340)), S::IssueCode::CallTooShort));
		CHECK(HasCode(one(Xfer(0x456789, "E8 11 22 33 44", S::Kind::Jmp, 0x530000)), S::IssueCode::CallTargetNotCode));
		CHECK(HasCode(one(Raw(0x450004, "00 00", "01 01")), S::IssueCode::Reserved));
		S::Data ov = GonpLike();
		ov.patches.push_back(Raw(0x45678B, "22", "23"));
		CHECK(HasCode(S::VerifyPatches(ov, exe, pf), S::IssueCode::Overlap));
		// the same refusals from the PC-mirrored parser (what PovertyCaster will say at boot)
		const std::string hdr = "format = 1\n[[patch]]\n";
		S::Data d;
		auto parse = [&](const std::string& s) { d = S::Data{}; return S::ParsePatches(s, pf, d); };
		CHECK_HAS(parse(hdr + "addr = 0x456789\norig = \"E8 00\"\ncall = \"vanilla:0x00410000\"\n"), "at least 5 bytes");
		CHECK_HAS(parse(hdr + "addr = 0x10\norig = \"00\"\nnew = \"01\"\n"), "outside the patchable image range");
		CHECK_HAS(parse(hdr + "addr = 0x456789\norig = \"00\"\nnew = \"01\"\n[[patch]]\naddr = 0x456789\norig = \"00\"\nnew = \"02\"\n"), "overlaps another patch");
		CHECK_HAS(parse(hdr + "addr = 0x456789\norig = \"E8 00 00 00 00\"\ncall = \"vanilla:0x00530000\"\n"), "is not vanilla code");
		CHECK_HAS(parse(hdr + "addr = 0x456789\norig = \"E8 00 00 00 00\"\ncall = \"pc:nope\"\n"), "unknown primitive 'pc:nope'");
		CHECK_HAS(parse(hdr + "addr = 0x456789\norig = \"00\"\nnew = \"0102\"\n"), "differ in length");
		CHECK_HAS(parse(hdr + "addr = 0x456789\norig = \"00\"\nnew = \"01\"\ncall = \"vanilla:0x00410000\"\n"), "exactly one of new / call / jmp");
		CHECK_HAS(parse(hdr + "addr = 0x456789\norig = \"00\"\nnew = \"01\"\nbogus = 1\n"), "unknown key 'bogus'");
		CHECK_HAS(parse("format = 1\nx = [1,2]\n"), "unsupported value");
		CHECK_HAS(parse("format = 1\nx = 1.5\n"), "floats");
		CHECK_HAS(parse("format = 1\na.b = 1\n"), "dotted");
		CHECK_HAS(parse("format = 1\nformat = 1\n"), "duplicate key");
		CHECK_HAS(parse("format = 1\nx = 'lit'\n"), "unsupported value");
		CHECK_HAS(parse("\xEF\xBB\xBF" "format = 1\n"), "BOM");
		CHECK_HAS(parse("format = 1\r[[patch]]\n"), "stray CR");
		CHECK_HAS(parse("[[patch]]\naddr = 1\n"), "format = 1");
		CHECK_HAS(parse("format = 1\n[x = {a = 1}\n"), "expected ]");
		// features / rules against the profile registry
		S::Profile fp = PcTestProfile();
		d = S::Data{};
		CHECK_HAS(S::ParseFeatures("format = 1\n[[feature]]\nid = \"nope\"\nabi = 1\n", fp, d), "unknown feature 'nope'");
		d = S::Data{};
		CHECK_HAS(S::ParseFeatures("format = 1\n[[feature]]\nid = \"custom-vars\"\nabi = 2\n", fp, d), "abi 2 != 1");
		d = S::Data{};
		CHECK_HAS(S::ParseFeatures("format = 1\n[[feature]]\nid = \"custom-vars\"\nabi = 1\n[feature.params]\nslots = 5000\n", fp, d), "outside 1..1024");
		d = S::Data{};
		CHECK_HAS(S::ParseRules("format = 1\n[rules]\nzzz = 1\n", fp, d), "unknown rule 'zzz'");
		// manifest level
		Manifest m;
		std::string err;
		const std::string sha = std::string(64, 'a');
		const std::string head = "{\"id\":\"a\",\"version\":\"1.0.0\",\"game\":\"mbaacc\",\"class\":\"sim\",\"exe\":[\"" + kExe + "\"],\"files\":[{\"path\":\"data/a.txt\",\"sha256\":\"" + sha + "\",\"size\":3}],";
		const std::string ep = "\"exe_patches\":{\"path\":\"patches.toml\",\"sha256\":\"" + sha + "\",\"size\":1}";
		CHECK(!ParseManifest(head + "\"schema\":1," + ep + "}", m, err)); CHECK_HAS(err, "needs schema 2");
		CHECK(!ParseManifest(head + "\"schema\":2,\"inject\":\"inject.toml\"}", m, err)); CHECK_HAS(err, "inject: reserved");
		CHECK(!ParseManifest(head + "\"schema\":2,\"ha6_patches\":\"x.toml\"}", m, err)); CHECK_HAS(err, "ha6_patches: reserved");
		CHECK(!ParseManifest(head + "\"schema\":2,\"exe_layers\":[{\"id\":\"x\"}]}", m, err)); CHECK_HAS(err, "exe_layers: not supported yet");
		CHECK(ParseManifest(head + "\"schema\":2,\"exe_layers\":[]," + ep + "}", m, err));
		CHECK(!ParseManifest(head + "\"schema\":2,\"exe_patches\":{\"path\":\"Patches.toml\",\"sha256\":\"" + sha + "\",\"size\":1}}", m, err)); CHECK_HAS(err, "bare lowercase");
		CHECK(!ParseManifest(head + "\"schema\":3}", m, err)); CHECK_HAS(err, "only schema 1 and 2");
		std::string cos = head + "\"schema\":2," + ep + "}";
		cos.replace(cos.find("\"sim\""), 5, "\"cosmetic\"");
		CHECK(!ParseManifest(cos, m, err));
	}

	// ---- 4. digest stability: the sim digest is of PARSED values ----
	{
		const S::Profile pf = PcTestProfile();
		Manifest a;
		std::string err;
		CHECK(ParseManifest(ReadText(fx + "/pcmod.json"), a, err));
		auto simOf = [&](const std::string& patches) {
			Manifest m = a;
			S::Files t{ patches, ReadText(fx + "/features.toml"), ReadText(fx + "/rules.toml") };
			m.exePatches.sha256 = Sha256Hex(patches); m.exePatches.size = patches.size();
			S::Data d;
			const std::string e = S::LoadFromTexts(m, t, pf, d);
			if (!e.empty()) std::printf("  load: %s\n", e.c_str());
			return std::make_pair(PackageSimDigestHex(m), PackageDigestHex(m));
		};
		const std::string orig = ReadText(fx + "/patches.toml");
		const auto base = simOf(orig);
		CHECK_EQ(base.first, kPcSim);
		const std::string reordered =
			"format=1\n\n# reordered, respaced, decimal addr, lowercase, no comments on the patches\r\n"
			"[[patch]]\naddr = 4849680\norig = \"83f805\"\nnew  = \"83F806\"\n"
			"[[patch]]\n  addr=0x470100\t# jmp\norig = \"8b 04 85 00 11 22 33\"\njmp = \"vanilla:0x480000\"\n"
			"[[patch]]\naddr = 0x456789\norig = \"e811223344\"\ncall = \"vanilla:0x412340\"\n";
		const auto same = simOf(reordered);
		CHECK_EQ(same.first, kPcSim);           // comments / whitespace / order / hex form never change it
		CHECK(same.second != base.second);      // the package digest is of the file BYTES
		std::string changed = orig;
		changed.replace(changed.find("83 F8 06"), 8, "83 F8 07");
		CHECK(simOf(changed).first != kPcSim);  // any value change does
		std::string tgt = orig;
		tgt.replace(tgt.find("0x00412340"), 10, "0x00412341");
		CHECK(simOf(tgt).first != kPcSim);
		// writer output reparses to the same canonical form as the hand-written fixture
		S::Data d;
		CHECK_EQ(S::ParsePatches(orig, pf, d), "");
		S::Data w;
		CHECK_EQ(S::ParsePatches(S::WritePatchesToml(d), pf, w), "");
		CHECK_EQ(S::Canonical(w), S::Canonical(d));
		S::Data fr;
		CHECK_EQ(S::ParseFeatures(ReadText(fx + "/features.toml"), pf, fr), "");
		CHECK_EQ(S::ParseRules(ReadText(fx + "/rules.toml"), pf, fr), "");
		S::Data fr2;
		CHECK_EQ(S::ParseFeatures(S::WriteFeaturesToml(fr), pf, fr2), "");
		CHECK_EQ(S::ParseRules(S::WriteRulesToml(fr), pf, fr2), "");
		CHECK_EQ(S::Canonical(fr2), S::Canonical(fr));
	}

	if (g_failures) { std::printf("pcmod_s2_test: %d failure(s)\n", g_failures); return 1; }
	std::printf("pcmod_s2_test: all checks passed\n");
	return 0;
}
