// game_link_test: the file -> game-slot mapping behind "auto-reload on save" (gamelink::SlotMaskForFile), the
// file -> stage match behind the stage auto-reload (gamelink::StageFileMatches), and the wire-struct sizes the
// editor shares with PovertyCaster's pc-proto.
#include "game_link.h"
#include "game_link_mock.h"

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
	std::printf(g_fail ? "game_link_test: %d FAILED\n" : "game_link_test: all passed\n", g_fail);
	return g_fail ? 1 : 0;
}
