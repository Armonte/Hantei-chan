// fpan-im pipe reply parsing (src/squirrel/fpanim_link.h): reloadchara engine rebuild / live / refusals.
#include "squirrel/fpanim_link.h"

#include <cstdio>

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)

int main()
{
	using namespace sqedit;
	CHECK(FpanimReloadRequest("chr000", false) == "reloadchara chr000");
	CHECK(FpanimReloadRequest("chr012", true) == "reloadchara chr012 live");

	// Engine rebuild, success (fpan-im core/hotreload.cpp format).
	auto a = FpanimParseReloadReply("OK engine rebuild chr000: chars 0/0 stage 3 quick 1; ha6 root 1A2B3C4D -> 0BADF00D; back in battle after 1840 ms");
	CHECK(a.engine && !a.refused && a.hasRoots && a.backInBattle);
	CHECK(a.rootBefore == 0x1A2B3C4Du && a.rootAfter == 0x0BADF00Du && a.ms == 1840);

	// Engine rebuild that did not come back.
	auto b = FpanimParseReloadReply("ERROR engine rebuild chr000: chars 0/0; ha6 root 1A2B3C4D -> 00000000; NOT back in battle after 120003 ms");
	CHECK(b.engine && !b.refused && b.hasRoots && !b.backInBattle && b.ms == 120003);

	// Refusals outside a training battle.
	CHECK(FpanimParseReloadReply("ERROR reloadchara refused in session Offline: the engine rebuild is the training menu's character switch (training battle only)").refused);
	CHECK(FpanimParseReloadReply("ERROR not in a battle (start training first)").refused);
	CHECK(FpanimParseReloadReply("ERROR timeout: no battle frame within 5 s (is a battle running?)").refused);
	CHECK(!FpanimParseReloadReply("ERROR bad character folder name").refused);

	// Live reload reply: no roots, no timing.
	auto c = FpanimParseReloadReply("OK resources: chr000 P1 (slot 0): ha6 cg pat pal | scripts: OK reloaded chr000");
	CHECK(!c.engine && !c.refused && !c.hasRoots && c.ms == -1);

	// Shared battle files -> reloadstd.
	CHECK(FpanimIsSharedFile("script/btl_StdMoveTbl.txt") && FpanimIsSharedFile("script/btl_MvFunc.txt"));
	CHECK(FpanimIsSharedFile("script/Battle_Std.txt") && FpanimIsSharedFile("./script/sq_stagebgm.txt"));
	CHECK(FpanimIsSharedFile("data/VectorTable.txt") && FpanimIsSharedFile("data/BattleInfo.txt"));
	CHECK(FpanimIsSharedFile("data/BoundEff.txt") && FpanimIsSharedFile("data\\_combase.txt"));
	CHECK(!FpanimIsSharedFile("data/chr000/chr000_mv_0.txt") && !FpanimIsSharedFile("data/effect.txt"));
	CHECK(!FpanimIsSharedFile("script/sub/x.txt") && !FpanimIsSharedFile("script/readme.md"));
	CHECK(FpanimStdReloadRequest(false) == "reloadstd" && FpanimStdReloadRequest(true) == "reloadstd live");
	auto d = FpanimParseReloadReply("OK reloadstd: Battle_Init + Battle_Std (btl_*, cmn_SetPosition, sq_stagebgm) + _combase re-run; VectorTable, BattleInfo, BoundEff reparsed | engine rebuild: chars 0/0 stage 1 quick; back in battle after 1141 ms");
	CHECK(d.stdReload && d.engine && d.backInBattle && d.ms == 1141 && !d.refused);
	auto e = FpanimParseReloadReply("OK reloadstd live: Battle_Init + Battle_Std ... reparsed | chr000 sc: OK chr000 P1 (slot 0): sc ./data/chr000/chr000_sc_0.txt");
	CHECK(e.stdReload && !e.engine && e.ms == -1);
	CHECK(FpanimParseReloadReply("ERROR reloadstd refused in session Offline").refused);
	CHECK(!FpanimParseReloadReply("OK engine rebuild chr000: x; back in battle after 5 ms").stdReload);

	// MBTL: the same shared files, routed to the MBTL pipe and the same relative path under its mods folder.
	CHECK(FpanimPipeForGame("mbtl") == "fpan-im-mbtl" && FpanimPipeForGame("uni2") == "fpan-im-uni2");
	CHECK(FpanimPipeForGame("") == "fpan-im-uni2");
	CHECK(FpanimLegacyPipeName("fpan-im-uni2") == "fbmod-uni2" && FpanimLegacyPipeName("fpan-im-mbtl") == "fbmod-mbtl");
	CHECK(FpanimLegacyPipeName("\\\\.\\pipe\\fpan-im-1234") == "\\\\.\\pipe\\fbmod-1234");
	CHECK(FpanimLegacyPipeName("fbmod-uni2").empty() && FpanimLegacyPipeName("other").empty());
	CHECK(FpanimIsSharedFile("script/btl_StdMoveTbl.txt") && FpanimIsSharedFile("data/BoundEff.txt"));
	const std::string mbtlMods = "C:\\Program Files (x86)\\Steam\\steamapps\\common\\MELTY BLOOD TYPE LUMINA\\fpan-im\\mods";
	CHECK(FpanimModsTarget(mbtlMods, "script/btl_StdMoveTbl.txt") ==
	      "C:/Program Files (x86)/Steam/steamapps/common/MELTY BLOOD TYPE LUMINA/fpan-im/mods/script/btl_StdMoveTbl.txt");
	CHECK(FpanimModsTarget(mbtlMods + "\\", "./data\\VectorTable.txt") ==
	      "C:/Program Files (x86)/Steam/steamapps/common/MELTY BLOOD TYPE LUMINA/fpan-im/mods/data/VectorTable.txt");
	CHECK(FpanimModsTarget("m", "data/chr000/chr000_sc_0.txt") == "m/data/chr000/chr000_sc_0.txt");
	CHECK(FpanimModsTarget("m", "../x.txt").empty() && FpanimModsTarget("m", "C:/x.txt").empty() && FpanimModsTarget("m", "/x").empty());
	CHECK(FpanimModsTarget("m", "data/../../x.txt").empty());
	CHECK(FpanimParseReloadReply("ERROR reloadstd is not available on mbtl").stdReload);

	if (g_fail) return 1;
	std::printf("fpanim_link_test: ok\n");
	return 0;
}
