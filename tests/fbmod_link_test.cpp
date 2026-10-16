// fbmod pipe reply parsing (src/squirrel/fbmod_link.h): reloadchara engine rebuild / live / refusals.
#include "squirrel/fbmod_link.h"

#include <cstdio>

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)

int main()
{
	using namespace sqedit;
	CHECK(FbmodReloadRequest("chr000", false) == "reloadchara chr000");
	CHECK(FbmodReloadRequest("chr012", true) == "reloadchara chr012 live");

	// Engine rebuild, success (fbmod core/hotreload.cpp format).
	auto a = FbmodParseReloadReply("OK engine rebuild chr000: chars 0/0 stage 3 quick 1; ha6 root 1A2B3C4D -> 0BADF00D; back in battle after 1840 ms");
	CHECK(a.engine && !a.refused && a.hasRoots && a.backInBattle);
	CHECK(a.rootBefore == 0x1A2B3C4Du && a.rootAfter == 0x0BADF00Du && a.ms == 1840);

	// Engine rebuild that did not come back.
	auto b = FbmodParseReloadReply("ERROR engine rebuild chr000: chars 0/0; ha6 root 1A2B3C4D -> 00000000; NOT back in battle after 120003 ms");
	CHECK(b.engine && !b.refused && b.hasRoots && !b.backInBattle && b.ms == 120003);

	// Refusals outside a training battle.
	CHECK(FbmodParseReloadReply("ERROR reloadchara refused in session Offline: the engine rebuild is the training menu's character switch (training battle only)").refused);
	CHECK(FbmodParseReloadReply("ERROR not in a battle (start training first)").refused);
	CHECK(FbmodParseReloadReply("ERROR timeout: no battle frame within 5 s (is a battle running?)").refused);
	CHECK(!FbmodParseReloadReply("ERROR bad character folder name").refused);

	// Live reload reply: no roots, no timing.
	auto c = FbmodParseReloadReply("OK resources: chr000 P1 (slot 0): ha6 cg pat pal | scripts: OK reloaded chr000");
	CHECK(!c.engine && !c.refused && !c.hasRoots && c.ms == -1);

	// Shared battle files -> reloadstd.
	CHECK(FbmodIsSharedFile("script/btl_StdMoveTbl.txt") && FbmodIsSharedFile("script/btl_MvFunc.txt"));
	CHECK(FbmodIsSharedFile("script/Battle_Std.txt") && FbmodIsSharedFile("./script/sq_stagebgm.txt"));
	CHECK(FbmodIsSharedFile("data/VectorTable.txt") && FbmodIsSharedFile("data/BattleInfo.txt"));
	CHECK(FbmodIsSharedFile("data/BoundEff.txt") && FbmodIsSharedFile("data\\_combase.txt"));
	CHECK(!FbmodIsSharedFile("data/chr000/chr000_mv_0.txt") && !FbmodIsSharedFile("data/effect.txt"));
	CHECK(!FbmodIsSharedFile("script/sub/x.txt") && !FbmodIsSharedFile("script/readme.md"));
	CHECK(FbmodStdReloadRequest(false) == "reloadstd" && FbmodStdReloadRequest(true) == "reloadstd live");
	auto d = FbmodParseReloadReply("OK reloadstd: Battle_Init + Battle_Std (btl_*, cmn_SetPosition, sq_stagebgm) + _combase re-run; VectorTable, BattleInfo, BoundEff reparsed | engine rebuild: chars 0/0 stage 1 quick; back in battle after 1141 ms");
	CHECK(d.stdReload && d.engine && d.backInBattle && d.ms == 1141 && !d.refused);
	auto e = FbmodParseReloadReply("OK reloadstd live: Battle_Init + Battle_Std ... reparsed | chr000 sc: OK chr000 P1 (slot 0): sc ./data/chr000/chr000_sc_0.txt");
	CHECK(e.stdReload && !e.engine && e.ms == -1);
	CHECK(FbmodParseReloadReply("ERROR reloadstd refused in session Offline").refused);
	CHECK(!FbmodParseReloadReply("OK engine rebuild chr000: x; back in battle after 5 ms").stdReload);

	// MBTL: the same shared files, routed to the MBTL pipe and the same relative path under its mods folder.
	CHECK(FbmodPipeForGame("mbtl") == "fbmod-mbtl" && FbmodPipeForGame("uni2") == "fbmod-uni2");
	CHECK(FbmodPipeForGame("") == "fbmod-uni2");
	CHECK(FbmodIsSharedFile("script/btl_StdMoveTbl.txt") && FbmodIsSharedFile("data/BoundEff.txt"));
	const std::string mbtlMods = "C:\\Program Files (x86)\\Steam\\steamapps\\common\\MELTY BLOOD TYPE LUMINA\\fbmod\\mods";
	CHECK(FbmodModsTarget(mbtlMods, "script/btl_StdMoveTbl.txt") ==
	      "C:/Program Files (x86)/Steam/steamapps/common/MELTY BLOOD TYPE LUMINA/fbmod/mods/script/btl_StdMoveTbl.txt");
	CHECK(FbmodModsTarget(mbtlMods + "\\", "./data\\VectorTable.txt") ==
	      "C:/Program Files (x86)/Steam/steamapps/common/MELTY BLOOD TYPE LUMINA/fbmod/mods/data/VectorTable.txt");
	CHECK(FbmodModsTarget("m", "data/chr000/chr000_sc_0.txt") == "m/data/chr000/chr000_sc_0.txt");
	CHECK(FbmodModsTarget("m", "../x.txt").empty() && FbmodModsTarget("m", "C:/x.txt").empty() && FbmodModsTarget("m", "/x").empty());
	CHECK(FbmodModsTarget("m", "data/../../x.txt").empty());
	CHECK(FbmodParseReloadReply("ERROR reloadstd is not available on mbtl").stdReload);

	if (g_fail) return 1;
	std::printf("fbmod_link_test: ok\n");
	return 0;
}
