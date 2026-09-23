// game_link_test: the file -> game-slot mapping behind "auto-reload on save" (gamelink::SlotMaskForFile), and
// the wire-struct sizes the editor shares with PovertyCaster's pc-proto.
#include "game_link.h"

#include <cstdio>
#include <cstring>

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
	std::printf(g_fail ? "game_link_test: %d FAILED\n" : "game_link_test: all passed\n", g_fail);
	return g_fail ? 1 : 0;
}
