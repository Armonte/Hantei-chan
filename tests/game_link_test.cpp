// game_link_test: the file -> game-slot mapping behind "auto-reload on save" (gamelink::SlotMaskForFile), the
// file -> stage match behind the stage auto-reload (gamelink::StageFileMatches), and the wire-struct sizes the
// editor shares with PovertyCaster's pc-proto.
#include "game_link.h"
#include "game_link_mock.h"
#include "authoring/authoring_model.h"
#include "authoring/authoring_policy.h"
#include "tag_tuning/tag_sidecar.h"

#include <filesystem>

#include <windows.h>

#include <cstdio>
#include <cstring>
#include <string>

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

static gamelink::wire::State MakeState()
{
	gamelink::wire::State s{};
	const char* files[4] = { "SION", "V_SION", "KOHAKU", "KOHAKU_M" };
	for (int i = 0; i < 4; ++i) {
		s.actors[i].exists = 1;
		std::strncpy(s.actors[i].file, files[i], sizeof s.actors[i].file - 1);
	}
	return s;
}


// ================== [authoring] docs/HANTEI_AUTHORING_MODE.md §6.4 H4 ==================
namespace {
namespace wire = gamelink::wire;

std::string CopyFixture()
{
	namespace fs = std::filesystem;
	char tmp[MAX_PATH];
	GetTempPathA(MAX_PATH, tmp);
	const fs::path dst = fs::u8path(std::string(tmp) + "hc_link_test_" + std::to_string(GetCurrentProcessId()) + "\\tag");
	std::error_code ec;
	fs::remove_all(dst.parent_path(), ec);
	fs::create_directories(dst, ec);
	// file by file (mingw's recursive fs::copy into an existing directory is unreliable)
	const fs::path src = fs::u8path("tests/fixtures/authoring/tag");
	for (const auto& e : fs::recursive_directory_iterator(src, ec)) {
		const fs::path to = dst / fs::relative(e.path(), src, ec);
		if (e.is_directory(ec)) fs::create_directories(to, ec);
		else { fs::create_directories(to.parent_path(), ec); fs::copy_file(e.path(), to, fs::copy_options::overwrite_existing, ec); }
		if (ec) std::printf("fixture copy: %s: %s\n", e.path().u8string().c_str(), ec.message().c_str());
	}
	return dst.u8string();
}

void WaitFor(gamelink::Client& c, bool (*pred)(const gamelink::Snapshot&), int ms = 3000)
{
	for (int k = 0; k < ms / 10 && !pred(c.Get()); ++k) Sleep(10);
}

void TestAuthoring()
{
	const std::string root = CopyFixture();
	const int L_cwt = tagtune::FindLever("cancelWindowTicks"), L_tagIn = tagtune::FindLever("tagIn");
	// ---- 1. caps negotiation: rev-1 DLL (QueryCaps Unknown) -> Setup disabled, no LinkCommandEx ever sent ----
	{
		gamelink::MockDll::Options o;
		o.authoring = false;
		gamelink::MockDll mock(o);
		CHECK(mock.Start());
		gamelink::Client c;
		c.SetTargetPid(mock.Pid());
		c.SetPollHz(20);
		c.SetAuthoringPoll(true, true);
		c.Connect();
		CHECK(c.WaitCaps(3000));
		gamelink::Snapshot s = c.Get();
		CHECK(s.capsUnknown && !s.haveCaps && !s.Authoring());
		CHECK(c.SetMatchSetup(gamelink::MockDll::DefaultSetup()) == 0);   // refused locally
		const authoring::LinkPolicy p = authoring::DecidePolicy(s, false);
		CHECK(p.rev1 && !p.canLoadInGame && !p.canApplyTuning && p.canWriteFiles && !p.banner.empty());
		Sleep(100);
		CHECK(mock.ExDropped() == 0);
		c.Disconnect();
		mock.Stop();
	}
	// ---- 2. a new DLL: caps, roster, setup state, the setup state machine (refusal / hot / cold), ApplyTuning ----
	{
		gamelink::MockDll::Options o;
		o.tagRoot = root;
		o.hotMs = 700;
		o.coldMs = 1500;
		gamelink::MockDll mock(o);
		CHECK(mock.Start());
		gamelink::Client c;
		c.SetTargetPid(mock.Pid());
		c.SetPollHz(20);
		c.SetAuthoringPoll(true, false);
		c.Connect();
		CHECK(c.WaitCaps(3000));
		WaitFor(c, [](const gamelink::Snapshot& s) { return s.rosterComplete && s.haveSetup && s.haveTuning; });
		gamelink::Snapshot s = c.Get();
		CHECK(s.haveCaps && s.Authoring() && s.caps.revision == 1 && s.gameId == "mbaacc");
		CHECK(s.caps.leverTableHash == tagtune::LeverTableHash() && s.caps.leverCount == 59);
		CHECK(s.rosterComplete && s.roster.size() == 2);
		const std::vector<authoring::RosterChar> roster = authoring::RosterFromLink(s.roster);
		CHECK(roster.size() == 31 && authoring::FindChara(roster, 4) && authoring::FindChara(roster, 4)->Duo());
		CHECK(s.haveSetup && s.setup.authState == (uint8_t)wire::AuthState::Ready && s.setup.phase == (uint8_t)wire::Phase::Battle);
		CHECK(std::memcmp(&s.setup.inForce, &s.setup.requested, sizeof s.setup.inForce) == 0);
		authoring::LinkPolicy p = authoring::DecidePolicy(s, false);
		CHECK(p.authoring && p.canLoadInGame && p.canApplyTuning && p.canWriteFiles && p.leverTableOk && !p.sessionLocked);
		// the tuning the game resolved == HC's own resolution of the same files
		tagtune::SidecarWorkspace ws;
		ws.Open(root);
		const tagtune::GlobalResolution g = ws.Global();
		if (!(s.haveTuning && std::string(s.tuning.activeStyle) == "Classic"))
			std::printf("diag: root=%s haveTuning=%d style='%s' source=%d charFiles=%u cwt=%d exists=%d\n", root.c_str(), s.haveTuning,
			            s.tuning.activeStyle, s.tuning.source, (unsigned)s.tuning.charFiles, s.tuning.values[L_cwt], (int)ws.Exists());
		CHECK(s.haveTuning && std::string(s.tuning.activeStyle) == "Classic" && s.tuning.source == (uint8_t)wire::TuningSource::Sidecars);
		CHECK(s.tuning.values[L_cwt] == 3 && wire::MaskBit(s.tuning.tuningMask, L_cwt));
		CHECK(s.haveTuningSlot[0] && std::string(s.tuningSlot[0].file) == "shiki" && s.tuningSlot[0].moon == 0);
		CHECK(s.tuningSlot[0].values[L_cwt] == 6 && wire::MaskBit(s.tuningSlot[0].charMask, L_cwt));
		CHECK(s.tuningSlot[3].moon == 1 && s.tuningSlot[3].values[tagtune::FindLever("entryStyle")] == 2);   // miyako_full: arc
		CHECK(wire::MaskBit(s.tuningSlot[3].moonMask, tagtune::FindLever("entryStyle")));
		// the CSS assist choice on the P2 partner (5+FN1 = 236C)
		CHECK((s.tuningSlot[3].flags & wire::kTunSlotCssAssists) && wire::MaskBit(s.tuningSlot[3].cssMask, tagtune::FindLever("assist.5.motion")));
		for (size_t i = 0; i < tagtune::kLeverCount; ++i) if (tagtune::kLevers[i].scope != tagtune::LeverScope::CharOnly) CHECK(s.tuning.values[i] == g.values[i]);
		// refusal: a duo in TAG -> Ineligible, immediately, and nothing changes
		authoring::Setup bad = authoring::FromWire(gamelink::MockDll::DefaultSetup());
		bad.slot[2].chara = 4;
		uint16_t seq = c.SetMatchSetup(authoring::ToWire(bad));
		wire::Reply r{};
		CHECK(seq && c.WaitReply(seq, 2000, r) && r.status == (int16_t)wire::Status::Ineligible);
		// TEAM without 4 input slots -> NeedsRestart
		authoring::Setup team = bad;
		team.mode = authoring::Mode::Team;
		team.slot[2].chara = 3;
		seq = c.SetMatchSetup(authoring::ToWire(team));
		CHECK(c.WaitReply(seq, 2000, r) && r.status == (int16_t)wire::Status::NeedsRestart);
		// hot path: the P1 partner changes (Sion -> Akiha F)
		authoring::Setup hot = authoring::FromWire(gamelink::MockDll::DefaultSetup());
		hot.slot[2] = { 3, 1, 0 };
		seq = c.SetMatchSetup(authoring::ToWire(hot));
		Sleep(200);
		CHECK(!c.PeekReply(seq, r));    // no reply before it finishes
		WaitFor(c, [](const gamelink::Snapshot& x) { return x.setup.authState == (uint8_t)wire::AuthState::ApplyingHot; }, 600);
		CHECK(c.Get().setup.authState == (uint8_t)wire::AuthState::ApplyingHot);
		CHECK(c.WaitReply(seq, 3000, r) && r.status == 0 && std::string(r.message).compare(0, 4, "hot:") == 0);
		WaitFor(c, [](const gamelink::Snapshot& x) { return x.setup.lastPath == 1 && x.setup.authState == 3; });
		s = c.Get();
		CHECK(s.setup.inForce.slot[2].chara == 3 && s.setup.inForce.slot[2].moon == 1 && s.setup.lastPath == 1);
		CHECK(mock.SetupsFinished() == 1);
		// Busy while it runs; cold path for a mode change (TAG -> 1v1 Training)
		authoring::Setup vs = hot;
		vs.mode = authoring::Mode::Versus;
		vs.scene = (uint8_t)wire::Scene::Training;
		vs.slot[2].chara = -1; vs.slot[3].chara = -1;
		seq = c.SetMatchSetup(authoring::ToWire(vs));
		WaitFor(c, [](const gamelink::Snapshot& x) { return x.setup.authState == (uint8_t)wire::AuthState::Rebuilding; }, 1000);
		CHECK(authoring::DecidePolicy(c.Get(), false).setupBusy && !authoring::DecidePolicy(c.Get(), false).canLoadInGame);
		const uint16_t busy = c.SetMatchSetup(authoring::ToWire(hot));
		CHECK(c.WaitReply(busy, 2000, r) && r.status == (int16_t)wire::Status::Busy);
		CHECK(c.WaitReply(seq, 3000, r) && r.status == 0 && std::string(r.message).compare(0, 5, "cold:") == 0);
		WaitFor(c, [](const gamelink::Snapshot& x) { return x.setup.lastPath == 2 && x.setup.authState == 3; });
		s = c.Get();
		CHECK(s.setup.inForce.mode == 0 && s.setup.gameModeKind == 0x1010 && s.setup.lastPath == 2);
		// hot only + a mode change -> Unsupported
		authoring::Setup ho = authoring::FromWire(gamelink::MockDll::DefaultSetup());
		ho.hotOnly = true;
		seq = c.SetMatchSetup(authoring::ToWire(ho));
		CHECK(c.WaitReply(seq, 2000, r) && r.status == (int16_t)wire::Status::Unsupported);
		// ApplyTuning + QueryAfter: edit the local shiki.ini, apply, and the game's slot 0 follows
		tagtune::SidecarDoc& d = ws.Doc(tagtune::CharDoc(tagtune::Layer::Local, "shiki"));
		tagtune::SetDocLever(d.ini, tagtune::DocKind::CharShared, L_cwt, 9);
		CHECK(ws.SaveDirty().ok);
		gamelink::Snapshot t;
		uint32_t since = c.Get().tuningSerial;
		seq = c.ApplyTuning(wire::kFlagQueryAfter);
		CHECK(c.WaitReply(seq, 2000, r) && r.status == 0 && std::string(r.message).find("sidecars") == 0);
		CHECK(c.WaitTuning(2000, t, since) && t.tuningSlot[0].values[L_cwt] == 9);
		CHECK(mock.Applies() == 1);
		// the 1v1 setup in force now: slot 0 is Shiki crescent, slots 2/3 empty
		CHECK(!t.tuningSlot[2].exists && t.tuningSlot[2].values[L_cwt] == t.tuning.values[L_cwt]);
		// a moon file: Shiki Full's tagIn
		CHECK(t.tuningSlot[0].values[L_tagIn] == 0);
		// QueryTuning with a slot mask sends the global + only those slots
		since = c.Get().tuningSerial;
		seq = c.QueryTuning(0x1);
		CHECK(c.WaitTuning(2000, t, since));
		// EndAuthoring
		seq = c.EndAuthoring();
		CHECK(c.WaitReply(seq, 2000, r) && r.status == 0);
		WaitFor(c, [](const gamelink::Snapshot& x) { return x.setup.authState == 0; });
		CHECK(c.Get().setup.authState == (uint8_t)wire::AuthState::Idle);
		c.Disconnect();
		mock.Stop();
	}
	// ---- 3. session lock (client), refusals; then the host between rounds (§9.3) ----
	for (int host = 0; host < 2; ++host) {
		gamelink::MockDll::Options o;
		o.tagRoot = root;
		o.session = true;
		o.role = host ? 1 : 2;
		o.betweenRounds = host != 0;
		gamelink::MockDll mock(o);
		CHECK(mock.Start());
		gamelink::Client c;
		c.SetTargetPid(mock.Pid());
		c.SetPollHz(20);
		c.SetAuthoringPoll(true, false);
		c.Connect();
		CHECK(c.WaitCaps(3000));
		WaitFor(c, [](const gamelink::Snapshot& s) { return s.haveSetup && s.haveTuning; });
		const gamelink::Snapshot s = c.Get();
		CHECK(s.SessionLive() && s.HostBetweenRounds() == (host != 0));
		const authoring::LinkPolicy p = authoring::DecidePolicy(s, false);
		wire::Reply r{};
		const uint16_t a = c.ApplyTuning(wire::kFlagQueryAfter);
		CHECK(c.WaitReply(a, 2000, r));
		if (!host) {
			CHECK(p.sessionLocked && !p.canWriteFiles && !p.canApplyTuning && !p.canLoadInGame && p.banner.find("SESSION") == 0);
			CHECK(authoring::DecidePolicy(s, true).canWriteFiles && !authoring::DecidePolicy(s, true).canApplyTuning);   // unlocked: saves only
			CHECK(r.status == (int16_t)wire::Status::RefusedSession);
			CHECK(s.tuning.source == (uint8_t)wire::TuningSource::Adopted && (s.tuningSlot[0].flags & wire::kTunSlotFromHost));
		} else {
			CHECK(!p.sessionLocked && p.hostBetweenRounds && p.canWriteFiles && p.canApplyTuning && !p.canLoadInGame);
			CHECK(r.status == 0 && std::string(r.message).find("queued for the next round") != std::string::npos);
		}
		const uint16_t sq = c.SetMatchSetup(gamelink::MockDll::DefaultSetup());
		CHECK(c.WaitReply(sq, 2000, r) && r.status == (int16_t)wire::Status::RefusedSession);
		c.Disconnect();
		mock.Stop();
	}
	// ---- 4. a foreign lever table: the banner, and nothing may be written ----
	{
		gamelink::MockDll::Options o;
		o.tagRoot = root;
		o.leverHash = 0x12345678u;
		gamelink::MockDll mock(o);
		CHECK(mock.Start());
		gamelink::Client c;
		c.SetTargetPid(mock.Pid());
		c.Connect();
		CHECK(c.WaitCaps(3000));
		const authoring::LinkPolicy p = authoring::DecidePolicy(c.Get(), false);
		CHECK(!p.leverTableOk && !p.canWriteFiles && !p.canApplyTuning && p.banner.find("lever table differs") != std::string::npos);
		CHECK(p.banner.find("12345678") != std::string::npos);
		c.Disconnect();
		mock.Stop();
	}
	// not linked
	CHECK(authoring::DecidePolicy(gamelink::Snapshot{}, false).canWriteFiles && !authoring::DecidePolicy(gamelink::Snapshot{}, false).canLoadInGame);
	std::error_code ec;
	std::filesystem::remove_all(std::filesystem::u8path(root).parent_path(), ec);
}
} // namespace

int main()
{
	using gamelink::SlotMaskForFile;
	const auto s = MakeState();
	// the character's .txt stem, and every file a save can touch
	CHECK(SlotMaskForFile("sion_0", s) == 0x1);
	CHECK(SlotMaskForFile("C:\\games\\mbaacc\\data\\sion_0_r.HA6", s) == 0x1);
	CHECK(SlotMaskForFile("C:/games/mbaacc/data/SION.pat", s) == 0x1);
	CHECK(SlotMaskForFile("sion_0_c.txt", s) == 0x1);
	// a prefix is not a match unless followed by '_'
	CHECK(SlotMaskForFile("sionx.ha6", s) == 0);
	CHECK(SlotMaskForFile("v_sion_0", s) == 0x2);
	// the longest matching slot name wins, so KOHAKU_M files do not also reload KOHAKU
	CHECK(SlotMaskForFile("kohaku_m_0.txt", s) == 0x8);
	CHECK(SlotMaskForFile("kohaku_0.txt", s) == 0x4);
	// two slots on the same data both reload
	auto t = s; std::strcpy(t.actors[2].file, "SION");
	CHECK(SlotMaskForFile("sion_1.txt", t) == 0x5);
	// empty slots never match
	t.actors[0].exists = 0;
	CHECK(SlotMaskForFile("sion_1.txt", t) == 0x4);
	CHECK(SlotMaskForFile("akiha_0.txt", s) == 0);
	CHECK(sizeof(gamelink::wire::State) == 276 && sizeof(gamelink::wire::Command) == 24);

	// ---- stage auto-reload: which saved files concern the stage the game shows ----
	using gamelink::StageFileMatches;
	using gamelink::StageStemOf;
	gamelink::wire::Stage st{};
	st.loaded = 28;
	std::strcpy(st.dataFile, "bg28");
	bool list = true;
	CHECK(StageFileMatches("C:\\games\\mbaacc\\Bg\\bg28.dat", st, &list) && !list);
	CHECK(StageFileMatches("C:/games/mbaacc/bg/BG28Info.txt", st));      // case-insensitive
	CHECK(StageFileMatches("bg28light.txt", st));
	CHECK(StageFileMatches("bg28_s.dat", st));                          // MBAC short variant of the same stage
	CHECK(!StageFileMatches("bg29.dat", st));
	CHECK(!StageFileMatches("bg280.dat", st));                          // a prefix is not the stage
	CHECK(!StageFileMatches("bg2.dat", st));
	CHECK(StageFileMatches("C:\\x\\Bg\\BgList.ini", st, &list) && list);   // the list concerns every stage
	gamelink::wire::Stage none{};
	none.loaded = -1;
	CHECK(!StageFileMatches("bg28.dat", none));                         // no stage on screen: nothing to reload
	CHECK(!StageFileMatches("BgList.ini", none));
	CHECK(StageStemOf("C:\\x\\bg28Info.txt") == "bg28");
	CHECK(StageStemOf("bg16light.txt") == "bg16");
	CHECK(StageStemOf("BgList.ini").empty());
	st.valid[3] = 1u << (28 & 7);                                      // 28 = byte 3, bit 4
	CHECK(st.IsValid(28) && !st.IsValid(27) && !st.IsValid(0) && !st.IsValid(100));
	CHECK(sizeof(gamelink::wire::Stage) == 64);
	CHECK((int)gamelink::wire::Op::SetStage == 5 && (int)gamelink::wire::Op::QueryStage == 7);
	CHECK((int)gamelink::wire::Kind::LinkStage == 0x103);
	// [tag-panel] the proposed QueryTag, its Unknown fallback and the gate probe, against a mock of the DLL side
	CHECK(sizeof(gamelink::wire::Tag) == 180 && sizeof(gamelink::wire::TagTeam) == 40);
	CHECK((int)gamelink::wire::Op::QueryTag == 8 && (int)gamelink::wire::Kind::LinkTag == 0x104);
	for (int mode = 0; mode < 3; ++mode) {   // 0 = tag ops + offline, 1 = no tag ops (today's DLL), 2 = session
		gamelink::MockDll::Options o;
		o.tagOps = mode != 1;
		o.session = mode == 2;
		gamelink::MockDll mock(o);
		CHECK(mock.Start());
		{
			gamelink::Client c;
			c.SetTargetPid(mock.Pid());
			c.SetPollHz(50);
			c.SetTagQuery(true);
			c.Connect();
			gamelink::wire::State st{};
			CHECK(c.WaitState(3000, st));
			for (int k = 0; k < 100 && !(c.Get().haveTag || c.Get().tagUnsupported); ++k) Sleep(20);
			const gamelink::Snapshot snap = c.Get();
			CHECK(snap.connected);
			if (mode == 1) CHECK(snap.tagUnsupported && !snap.haveTag);
			else {
				CHECK(snap.haveTag && !snap.tagUnsupported);
				CHECK(snap.tag.team[0].tagRequest == 301 && snap.tag.team[0].assistPattern == 455);
				CHECK(snap.tag.team[1].cooldownLeft == 80 && std::string(snap.tag.activeStyle) == "Classic");
				CHECK((snap.tag.sessionFlags != 0) == (mode == 2));
				CHECK((snap.tag.tagConfig & gamelink::wire::kTagCfgTag) && ((snap.tag.tagConfig & gamelink::wire::kTagCfgFromHost) != 0) == (mode == 2));
			}
			const uint16_t seq = c.ProbeGate();
			gamelink::wire::Reply r{};
			CHECK(c.WaitReply(seq, 3000, r));
			CHECK(r.status == (int16_t)(mode == 2 ? gamelink::wire::Status::RefusedSession : gamelink::wire::Status::Ok));
			gamelink::wire::Reply p{};
			CHECK(c.PeekReply(seq, p) && p.status == r.status);
			CHECK(mock.Probes() == 1);
			c.Disconnect();
		}
		mock.Stop();
	}
	TestAuthoring();
	std::printf(g_fail ? "game_link_test: %d FAILED\n" : "game_link_test: all passed\n", g_fail);
	return g_fail ? 1 : 0;
}
