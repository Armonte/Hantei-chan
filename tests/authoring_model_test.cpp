// authoring_model_test: the pure half of Authoring Mode (docs/HANTEI_AUTHORING_MODE.md §6.3 H6 / H7):
//   * GOLDEN BYTES for three setups (1v1 training, TAG with assists + choices, TEAM). PovertyCaster's
//     tests/mbaacc_authoring decodes the SAME hex literals (they are listed in the doc, §9.6), so both halves agree
//     on the 64-byte LinkMatchSetup field for field;
//   * protocol struct sizes and a golden LinkCaps / LinkSetupState / LinkTuningSlot byte image;
//   * the validation table (every status the editor can decide offline);
//   * files to open (duo -> File2), palette count, the launch environment + env block, saved setups;
//   * the roster mirror against PovertyCaster's MbaaccRoster.hpp when it is on disk (HAVE_PC_ROSTER).
#include "authoring/authoring_model.h"
#include "authoring/game_launcher.h"
#include "authoring/roster_mirror.h"

#include <windows.h>

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

#ifdef HAVE_PC_ROSTER
#include "mbaacc/MbaaccRoster.hpp"
#endif
#ifdef HAVE_PC_PROTO
#include "pc/proto/Proto.hpp"
#endif

using namespace authoring;
namespace fs = std::filesystem;

static int g_fail = 0, g_checks = 0;
#define CHECK(c) do { ++g_checks; if (!(c)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)
#define CHECKM(c, m) do { ++g_checks; if (!(c)) { std::printf("FAIL %s:%d  %s  (%s)\n", __FILE__, __LINE__, #c, std::string(m).c_str()); ++g_fail; } } while (0)

// ---- the three golden setups (the same literals in PovertyCaster tests/mbaacc_authoring) ----
// 1. 1v1 Training: Tohno (7) C pal 0 vs Sion (0) H pal 5, stage 28, timer/KO default, re-read tuning first.
static const char* kGoldenVersus =
	"010001201c00ffff0700000000000205ffff0000ffff00000000000000000000"
	"0000000000000000000000000000000000000000000000000000000000000000";
// 2. TAG (§6.4 P11a): Tohno C pal 3 + Sion C vs V.Sion C + Miyako F, stage 16, assists on, re-read tuning first;
//    assist choices: P1 2+FN1 = 236A (1), P2 5+FN1 = 236C (13).
static const char* kGoldenTag =
	"010100211000ffff070000030b000000000000000800010000010000000d0000"
	"0000000000000000000000000000000000000000000000000000000000000000";
// 3. TEAM: Arc (1) F pal 1 + Akiha (3) H pal 3 vs Ciel (2) C pal 2 + Nanaya (15) C pal 4, random stage, allDown,
//    infinite timer, Authoring VS.
static const char* kGoldenTeam =
	"01020200ffff01000100010102000002030002030f0000040000000000000000"
	"0000000000000000000000000000000000000000000000000000000000000000";

static std::string Strip(const char* s) { std::string o; for (; *s; ++s) if (*s != ' ') o += *s; return o; }

static Setup SetupVersus()
{
	Setup s;
	s.mode = Mode::Versus;
	s.scene = (uint8_t)wire::Scene::Training;
	s.assists = false;
	s.tuningFirst = true;
	s.stage = 28;
	s.slot[0] = { 7, 0, 0 };
	s.slot[1] = { 0, 2, 5 };
	return s;
}
static Setup SetupTag()
{
	Setup s;
	s.mode = Mode::Tag;
	s.assists = true;
	s.tuningFirst = true;
	s.stage = 16;
	s.slot[0] = { 7, 0, 3 };
	s.slot[1] = { 11, 0, 0 };
	s.slot[2] = { 0, 0, 0 };
	s.slot[3] = { 8, 1, 0 };
	s.assist[0][1] = 1;    // 2+FN1 = 236A
	s.assist[1][0] = 13;   // 5+FN1 = 236C
	return s;
}
static Setup SetupTeam()
{
	Setup s;
	s.mode = Mode::Team;
	s.scene = (uint8_t)wire::Scene::AuthoringVs;
	s.assists = false;
	s.tuningFirst = false;
	s.stage = -1;
	s.koRule = 1;
	s.timer = 0;
	s.slot[0] = { 1, 1, 1 };
	s.slot[1] = { 2, 0, 2 };
	s.slot[2] = { 3, 2, 3 };
	s.slot[3] = { 15, 0, 4 };
	return s;
}

static void TestGolden()
{
	const Setup setups[3] = { SetupVersus(), SetupTag(), SetupTeam() };
	const char* golden[3] = { kGoldenVersus, kGoldenTag, kGoldenTeam };
	for (int i = 0; i < 3; ++i) {
		const std::string hex = ToHex(ToWire(setups[i]));
		CHECKM(hex == Strip(golden[i]), "setup " + std::to_string(i) + " = " + hex);
		wire::MatchSetup w{};
		CHECK(FromHex(Strip(golden[i]), w));
		const Setup back = FromWire(w);
		CHECK(back == setups[i]);
		CHECK(ToHex(w) == Strip(golden[i]));
	}
	// field by field for the TAG one (what the PC test also asserts)
	wire::MatchSetup w{};
	FromHex(Strip(kGoldenTag), w);
	CHECK(w.version == 1 && w.mode == 1 && w.scene == 0 && w.flags == (wire::kSetupAssists | wire::kSetupTuningFirst));
	CHECK(w.stage == 16 && w.koRule == 0xFF && w.timer == 0xFF);
	CHECK(w.slot[0].chara == 7 && w.slot[0].moon == 0 && w.slot[0].palette == 3);
	CHECK(w.slot[1].chara == 11 && w.slot[2].chara == 0 && w.slot[3].chara == 8 && w.slot[3].moon == 1);
	CHECK(w.assist[0][1] == 1 && w.assist[1][0] == 13 && w.dummy[0] == 0 && w.dummy[1] == 0);
	for (uint8_t b : w._reserved) CHECK(b == 0);
	CHECK(!FromHex("00", w) && !FromHex(std::string(128, 'g'), w));
	// Versus clears partner slots and assist choices on the wire
	Setup v = SetupVersus();
	v.assist[0][0] = 3;
	CHECK(ToWire(v).assist[0][0] == 0 && ToWire(v).slot[2].chara == -1);
}


// ---- PovertyCaster's own golden set (docs §10.2, pinned in PC tests/mbaacc_link_authoring): HC encodes AND decodes it ----
static void TestPcGolden()
{
	struct G { const char* name; const char* hex; Setup s; };
	auto pad = [](const char* h) { return std::string(h) + std::string(56, '0'); };
	Setup g1;   // TAG, Authoring VS, Assists|TuningFirst, stage 16; Tohno C3 + Sion C1 vs V.Sion C0 + Miyako F7; P1 2+FN1 = 1
	g1.mode = Mode::Tag; g1.scene = (uint8_t)wire::Scene::AuthoringVs; g1.assists = true; g1.tuningFirst = true; g1.stage = 16;
	g1.slot[0] = { 7, 0, 3 }; g1.slot[1] = { 11, 0, 0 }; g1.slot[2] = { 0, 0, 1 }; g1.slot[3] = { 8, 1, 7 }; g1.assist[0][1] = 1;
	Setup g2;   // 1v1 Training, TuningFirst, stage 16; Tohno C3 vs V.Sion C0
	g2.mode = Mode::Versus; g2.scene = (uint8_t)wire::Scene::Training; g2.assists = false; g2.tuningFirst = true; g2.stage = 16;
	g2.slot[0] = { 7, 0, 3 }; g2.slot[1] = { 11, 0, 0 };
	Setup g3;   // TEAM, Authoring VS, TuningFirst, stage -1, allDown, infinite; Tohno C3 / V.Sion C0 / Sion C1 / Miyako F7
	g3.mode = Mode::Team; g3.scene = (uint8_t)wire::Scene::AuthoringVs; g3.assists = false; g3.tuningFirst = true; g3.stage = -1; g3.koRule = 1; g3.timer = 0;
	g3.slot[0] = { 7, 0, 3 }; g3.slot[1] = { 11, 0, 0 }; g3.slot[2] = { 0, 0, 1 }; g3.slot[3] = { 8, 1, 7 };
	const G gs[3] = {
		{ "G1", "010102211000ffff070000030b0000000000000108000107000100000000000000000000", g1 },
		{ "G2", "010001201000ffff070000030b000000ffff0000ffff0000000000000000000000000000", g2 },
		{ "G3", "01020220ffff0100070000030b0000000000000108000107000000000000000000000000", g3 },
	};
	for (const G& g : gs) {
		const std::string hex = pad(g.hex);
		CHECKM(ToHex(ToWire(g.s)) == hex, std::string(g.name) + " encodes to " + ToHex(ToWire(g.s)));
		wire::MatchSetup w{};
		CHECK(FromHex(hex, w) && FromWire(w) == g.s);
	}
}

// ---- the mirror vs PovertyCaster's Proto.hpp itself, when the header on disk has the authoring structs ----
static void TestAgainstPcProto()
{
#ifdef HAVE_PC_PROTO
	namespace P = pc::proto;
	CHECK(sizeof(P::LinkCommandEx) == sizeof(wire::CommandEx) && sizeof(P::LinkMatchSetup) == sizeof(wire::MatchSetup));
	CHECK(sizeof(P::LinkCaps) == sizeof(wire::Caps) && sizeof(P::LinkRoster) == sizeof(wire::Roster));
	CHECK(sizeof(P::LinkSetupState) == sizeof(wire::SetupState) && sizeof(P::LinkTuningGlobal) == sizeof(wire::TuningGlobal));
	CHECK(sizeof(P::LinkTuningSlot) == sizeof(wire::TuningSlot) && sizeof(P::LinkRosterEntry) == sizeof(wire::RosterEntry));
	CHECK(offsetof(P::LinkCaps, gameId) == offsetof(wire::Caps, gameId) && offsetof(P::LinkSetupState, sessionRole) == offsetof(wire::SetupState, sessionRole));
	CHECK(offsetof(P::LinkSetupState, betweenRounds) == offsetof(wire::SetupState, betweenRounds) && offsetof(P::LinkSetupState, message) == offsetof(wire::SetupState, message));
	CHECK(offsetof(P::LinkTuningGlobal, values) == offsetof(wire::TuningGlobal, values) && offsetof(P::LinkTuningSlot, values) == offsetof(wire::TuningSlot, values));
	CHECK(offsetof(P::LinkTuningSlot, cssMask) == offsetof(wire::TuningSlot, cssMask) && offsetof(P::LinkMatchSetup, assist) == offsetof(wire::MatchSetup, assist));
	CHECK((int)P::LinkOp::QueryCaps == (int)wire::Op::QueryCaps && (int)P::LinkOp::EndAuthoring == (int)wire::Op::EndAuthoring);
	CHECK((int)P::LinkOp::SetMatchSetup == (int)wire::Op::SetMatchSetup && (int)P::IpcKind::LinkCommandEx == (int)wire::Kind::LinkCommandEx);
	CHECK((int)P::IpcKind::LinkTuningSlot == (int)wire::Kind::LinkTuningSlot && (int)P::LinkStatus::Timeout == (int)wire::Status::Timeout);
	CHECK(P::kLinkFlagQueryAfter == wire::kFlagQueryAfter && P::kLinkCapSetup == wire::kCapSetup && P::kSetupTuningFirst == wire::kSetupTuningFirst);
	std::printf("protocol mirror checked against PovertyCaster Proto.hpp\n");
#else
	std::printf("PovertyCaster Proto.hpp with the authoring structs not found at configure time (PC_PROTO_HPP): skipped\n");
#endif
}

static void TestStructs()
{
	CHECK(sizeof(wire::CommandEx) == 8 && sizeof(wire::Pick) == 4 && sizeof(wire::MatchSetup) == 64);
	CHECK(sizeof(wire::Caps) == 48 && sizeof(wire::RosterEntry) == 56 && sizeof(wire::Roster) == 904);
	CHECK(sizeof(wire::SetupState) == 192 && sizeof(wire::TuningGlobal) == 344 && sizeof(wire::TuningSlot) == 312);
	CHECK(sizeof(wire::CommandEx) + sizeof(wire::MatchSetup) == 72);   // the SetMatchSetup message payload
	CHECK((int)wire::Op::QueryCaps == 9 && (int)wire::Op::EndAuthoring == 15 && (int)wire::Kind::LinkCommandEx == 0x110);
	CHECK((int)wire::Kind::LinkTuningSlot == 0x109 && (int)wire::Status::Timeout == -12 && wire::kFlagQueryAfter == 0x20);
	// golden LinkCaps image: revision 1, caps setup|tuning|roster, hash 0x37BDB3FB, 59 levers, gameId "mbaacc"
	wire::Caps c{};
	c.revision = 1;
	c.caps = wire::kCapRoster | wire::kCapSetup | wire::kCapTuning;
	c.leverTableHash = 0x37BDB3FBu;
	c.leverCount = 59;
	std::memcpy(c.gameId, "mbaacc", 6);
	const uint8_t* p = (const uint8_t*)&c;
	CHECK(p[0] == 1 && p[4] == 0x1C && p[8] == 0xFB && p[9] == 0xB3 && p[10] == 0xBD && p[11] == 0x37 && p[12] == 59);
	CHECK(p[36] == 'm' && p[41] == 'c' && p[42] == 0);
	// masks: bit i in word i >> 5
	wire::TuningSlot s{};
	wire::SetMaskBit(s.charMask, 45);
	CHECK(s.charMask[1] == (1u << 13) && wire::MaskBit(s.charMask, 45) && !wire::MaskBit(s.charMask, 44));
	CHECK(FindGameTable("") && FindGameTable("mbaacc") && FindGameTable("mbaacc")->leverTableHash == 0x37BDB3FBu && !FindGameTable("qoh"));
}

static void TestRoster()
{
	const std::vector<RosterChar> r = MirrorRoster(true), packed = MirrorRoster(false);
	CHECK(r.size() == 31);
	const RosterChar* maids = FindChara(r, 4);
	CHECK(maids && maids->Duo() && !BanReason(*maids, Mode::Tag).empty() && !BanReason(*maids, Mode::Team).empty() && BanReason(*maids, Mode::Versus).empty());
	const RosterChar* hime = FindChara(r, 51);
	CHECK(hime && hime->Duo());   // P_ARC has File2 (P_ARC_D): banned in TAG / TEAM like the doc's "Hime ⊘"
	const RosterChar* shiki = FindChara(r, 7);
	CHECK(shiki && shiki->file1 == "shiki" && BanReason(*shiki, Mode::Tag).empty());
	// ries: tag patterns only in tag_mod data
	const RosterChar* ries = FindChara(packed, 30);
	CHECK(ries && !BanReason(*ries, Mode::Tag).empty() && (ries->flags & wire::kRosterNeedsMod));
	CHECK(BanReason(*FindChara(r, 30), Mode::Tag).empty());
	// hisui: its tag-in is mod-only but has a fallback, its tag-out is stock -> allowed without mod data
	CHECK(BanReason(*FindChara(packed, 5), Mode::Tag).empty());
	int duos = 0;
	for (const RosterChar& c : r) duos += c.Duo();
	CHECK(duos == 4);   // Maids, KohaMech, NekoMech, Hime
	CHECK(FindFile(r, "HISUI") && FindFile(r, "hisui")->chara == 5);   // the single character, not the duo
	CHECK(std::string(MbacStemForChara(28)) == "KISHIMA" && std::string(MbacStemForChara(30)).empty());
	int mbac = 0;
	for (const RosterChar& c : r) mbac += MbacStemForChara(c.chara)[0] != 0;
	CHECK(mbac == 22);
	// a LinkRoster page decodes to the same entries
	wire::Roster page{};
	page.count = 2; page.total = 2; page.pageCount = 1;
	page.e[0].chara = 7; page.e[0].selector = 3; page.e[0].flags = wire::kRosterTagOk | wire::kRosterTeamOk | wire::kRosterHasTcRow;
	std::strcpy(page.e[0].file1, "shiki"); std::strcpy(page.e[0].name, "Tohno");
	page.e[1].chara = 4; page.e[1].flags = wire::kRosterDuo; std::strcpy(page.e[1].file1, "hisui"); std::strcpy(page.e[1].file2, "kohaku");
	const std::vector<RosterChar> lr = RosterFromLink({ page });
	CHECK(lr.size() == 2 && lr[0].name == "Tohno" && lr[0].fullName == "Shiki Tohno" && lr[1].Duo() && lr[1].file2 == "kohaku");
#ifdef HAVE_PC_ROSTER
	CHECK(mbaacc::kCssSelectorCount == (unsigned)kRosterMirrorCount);
	for (int i = 0; i < kRosterMirrorCount && i < (int)mbaacc::kCssSelectorCount; ++i) {
		CHECKM(mbaacc::kCssRoster[i].selector == kRosterMirror[i].selector && (int)mbaacc::kCssRoster[i].chara == kRosterMirror[i].chara &&
		       std::string(mbaacc::kCssRoster[i].name) == kRosterMirror[i].name, kRosterMirror[i].name);
	}
	std::printf("roster mirror checked against PovertyCaster MbaaccRoster.hpp (%u entries)\n", mbaacc::kCssSelectorCount);
#else
	std::printf("PovertyCaster MbaaccRoster.hpp not found at configure time: roster drift check skipped\n");
#endif
}

static bool Has(const std::vector<Problem>& v, wire::Status st, int slot = -2)
{
	for (const Problem& p : v) if (p.status == (int16_t)st && (slot == -2 || p.slot == slot)) return true;
	return false;
}

static void TestValidation()
{
	const std::vector<RosterChar> r = MirrorRoster(true);
	ValidateContext ctx;
	ctx.team4p = true;
	ctx.paletteCount = [](const std::string& f) { return f == "shiki" ? 4 : 36; };
	ctx.stageExists = [](int s) { return s != 55; };
	ctx.txtExists = [](const std::string& f, int m) { return !(f == "sion" && m == 2); };
	CHECK(Validate(SetupTag(), r, ctx).empty());
	CHECK(Validate(SetupTeam(), r, ctx).empty());
	CHECK(Has(Validate(SetupVersus(), r, ctx), wire::Status::MissingFile, 1));   // sion_2.txt "missing"
	Setup v = SetupVersus();
	v.force = true;
	CHECK(Validate(v, r, ctx).empty());                                           // force skips the preflight
	v.slot[2] = { 3, 0, 0 };
	CHECK(Has(Validate(v, r, ctx), wire::Status::BadArgs, 2));                     // 1v1 has no partners
	v = SetupVersus(); v.force = true; v.slot[1].chara = -1;
	CHECK(Has(Validate(v, r, ctx), wire::Status::BadArgs, 1));                     // a point is required
	v = SetupVersus(); v.force = true; v.slot[0].palette = 4;
	CHECK(Has(Validate(v, r, ctx), wire::Status::BadArgs, 0));                     // shiki.pal has 4
	v.slot[0].palette = 3;
	CHECK(Validate(v, r, ctx).empty());
	v.slot[0].moon = 3;
	CHECK(Has(Validate(v, r, ctx), wire::Status::BadArgs, 0));
	v = SetupVersus(); v.force = true; v.slot[0].chara = 4;                         // Maids in 1v1: fine
	CHECK(Validate(v, r, ctx).empty());
	Setup t = SetupTag();
	t.slot[2].chara = 4;                                                             // Maids in TAG: ineligible
	CHECK(Has(Validate(t, r, ctx), wire::Status::Ineligible, 2));
	t = SetupTag(); t.slot[2].chara = -1; t.slot[3].chara = -1;                      // TAG with solo sides is valid
	CHECK(Validate(t, r, ctx).empty());
	t = SetupTag(); t.scene = (uint8_t)wire::Scene::Training;
	CHECK(Has(Validate(t, r, ctx), wire::Status::Unsupported));
	t = SetupTag(); t.stage = 100;
	CHECK(Has(Validate(t, r, ctx), wire::Status::BadArgs, -1));
	t.stage = 55;
	CHECK(Has(Validate(t, r, ctx), wire::Status::BadArgs, -1));                     // no BgList entry
	t.stage = -1;
	CHECK(Validate(t, r, ctx).empty());
	t.assist[0][0] = 17;
	CHECK(Has(Validate(t, r, ctx), wire::Status::BadArgs, -1));
	Setup m = SetupTeam();
	m.slot[3].chara = -1;
	CHECK(Has(Validate(m, r, ctx), wire::Status::BadArgs, 3));                      // TEAM needs all four
	ctx.team4p = false;
	CHECK(Has(Validate(SetupTeam(), r, ctx), wire::Status::NeedsRestart));
	t = SetupTag(); t.slot[1].chara = 99;
	CHECK(Has(Validate(t, r, ctx), wire::Status::BadArgs, 1));                      // not in the roster
	t = SetupTag(); t.koRule = 2;
	CHECK(Has(Validate(t, r, ctx), wire::Status::BadArgs, -1));
}

static void TestFilesAndEnv()
{
	const std::vector<RosterChar> r = MirrorRoster(true);
	std::vector<OpenTarget> f = FilesToOpen(SetupTag(), r, "C:\\games\\mbaacc_dev");
	CHECK(f.size() == 4);
	CHECK(f[0].file == "shiki" && f[0].slot == 0 && f[0].point && f[0].txtPath == "C:\\games\\mbaacc_dev\\data\\shiki_0.txt");
	CHECK(f[1].file == "sion" && f[1].slot == 2 && !f[1].point);
	CHECK(f[2].file == "v_sion" && f[3].file == "miyako" && f[3].moon == 1 && f[3].txtPath.find("miyako_1.txt") != std::string::npos);
	Setup same = SetupTag();
	same.slot[1] = same.slot[0];   // mirror match: opened once
	CHECK(FilesToOpen(same, r, "C:\\g").size() == 3);
	Setup duo = SetupVersus();
	duo.slot[0] = { 4, 1, 0 };     // Maids point in 1v1: hisui_1 + kohaku_1
	f = FilesToOpen(duo, r, "C:\\g");
	CHECK(f.size() == 3 && f[0].file == "hisui" && f[1].file == "kohaku" && f[1].second && f[2].file == "sion");
	// the launch environment
	const auto env = LaunchEnv(SetupTag());
	auto get = [&env](const char* k) { for (const auto& kv : env) if (kv.first == k) return kv.second; return std::string("<none>"); };
	CHECK(get("PCHOST_MBAACC_AUTHORING") == "1" && get("PCHOST_MBAACC_LINK") == "1" && get("PCHOST_GAME") == "mbaacc");
	CHECK(get("PCHOST_MBAACC_AUTHORING_SETUP") == Strip(kGoldenTag) && get("PCHOST_LOG_TAG") == "authoring");
	CHECK(get("PCHOST_MBAACC_2V2") == "<none>");
	const auto envTeam = LaunchEnv(SetupTeam());
	bool team = false;
	for (const auto& kv : envTeam) team = team || (kv.first == "PCHOST_MBAACC_2V2" && kv.second == "1");
	CHECK(team);
	// the env block: the parent's variables survive, ours replace case-insensitively, drive entries stay first
	const wchar_t parent[] = L"=C:=C:\\x\0Path=C:\\Windows\0pchost_log_tag=old\0ZZ=1\0";
	const std::wstring b = BuildEnvBlock({ { "PCHOST_LOG_TAG", "authoring" }, { "PCHOST_GAME", "mbaacc" } }, parent);
	CHECK(b.compare(0, 8, L"=C:=C:\\x") == 0);
	CHECK(b.find(L"PCHOST_LOG_TAG=authoring") != std::wstring::npos && b.find(L"=old") == std::wstring::npos);
	CHECK(b.find(L"Path=C:\\Windows") != std::wstring::npos && b.find(L"ZZ=1") != std::wstring::npos);
	CHECK(b.size() >= 2 && b[b.size() - 1] == 0 && b[b.size() - 2] == 0);
	CHECK(b.find(L"Path") < b.find(L"PCHOST_GAME") && b.find(L"PCHOST_GAME") < b.find(L"PCHOST_LOG_TAG"));   // sorted, case-insensitive
	// palette count from a .pal
	char tmp[MAX_PATH];
	GetTempPathA(MAX_PATH, tmp);
	const std::string pal = std::string(tmp) + "hc_auth_test_" + std::to_string(GetCurrentProcessId()) + ".pal";
	{ std::ofstream o(pal, std::ios::binary); const uint32_t n = 36; o.write((const char*)&n, 4); }
	CHECK(PaletteCountOf(pal) == 36);
	{ std::ofstream o(pal, std::ios::binary); const uint32_t n = 999; o.write((const char*)&n, 4); }
	CHECK(PaletteCountOf(pal) == 256);
	std::remove(pal.c_str());
	CHECK(PaletteCountOf(pal) == -1);
	// the game dir checks and the tail reader
	const GameDirCheck c = CheckGameDir("");
	CHECK(!c.CanLaunch());
	const std::string log = std::string(tmp) + "hc_auth_test_" + std::to_string(GetCurrentProcessId()) + ".log";
	{ std::ofstream o(log, std::ios::binary); for (int i = 0; i < 50; ++i) o << "line " << i << "\r\n"; }
	const std::vector<std::string> tail = TailFile(log, 3);
	CHECK(tail.size() == 3 && tail[0] == "line 47" && tail[2] == "line 49");
	std::remove(log.c_str());
}

static void TestLibrary()
{
	SetupLibrary lib;
	Setup a = SetupTag();
	a.style = "Chaos";
	lib.Save(a, "shiki tag");
	lib.Save(SetupTeam(), "team test");
	lib.Save(SetupVersus(), "shiki tag");   // replaces
	CHECK(lib.named.size() == 2 && lib.Find("shiki tag")->mode == Mode::Versus);
	lib.PushRecent(SetupTag());
	lib.PushRecent(SetupTeam());
	lib.PushRecent(SetupTag());             // dedup: moves to the front
	CHECK(lib.recent.size() == 2 && lib.recent[0].mode == Mode::Tag);
	for (int i = 0; i < 20; ++i) { Setup s = SetupTag(); s.stage = i + 1; lib.PushRecent(s); }
	CHECK((int)lib.recent.size() == lib.recentCap && lib.recent[0].stage == 20);
	const std::string text = lib.Serialize();
	SetupLibrary back;
	CHECK(back.Parse(text));
	CHECK(back.named.size() == 2 && back.recent.size() == lib.recent.size());
	CHECK(back.Find("team test") && *back.Find("team test") == *lib.Find("team test"));
	CHECK(back.recent[0] == lib.recent[0] && back.recent.back() == lib.recent.back());
	Setup styled = SetupTag();
	styled.style = "Chaos";
	lib.Save(styled, "styled");
	back.Parse(lib.Serialize());
	CHECK(back.Find("styled") && back.Find("styled")->style == "Chaos");
	CHECK(lib.Remove("styled") && !lib.Remove("styled"));
	CHECK(Describe(SetupTag(), MirrorRoster(true)) == "TAG Tohno C3 + Sion C0 vs V.Sion C0 + Miyako F0, stage 16, assists");
}

int main()
{
	TestGolden();
	TestPcGolden();
	TestAgainstPcProto();
	TestStructs();
	TestRoster();
	TestValidation();
	TestFilesAndEnv();
	TestLibrary();
	std::printf(g_fail ? "authoring_model_test: %d of %d FAILED\n" : "authoring_model_test: all %d checks passed\n", g_fail ? g_fail : g_checks, g_checks);
	return g_fail ? 1 : 0;
}
