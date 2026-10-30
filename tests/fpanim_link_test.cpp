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
	// System effects (data/effect.txt and what it loads) -> reloadsyseff (BootySama #110).
	CHECK(FpanimIsSystemEffectFile("data/effect.HA6") && FpanimIsSystemEffectFile("./data/effect.txt"));
	CHECK(FpanimIsSystemEffectFile("data\\effect.cg") && FpanimIsSystemEffectFile("data/sys_effect.pat"));
	CHECK(FpanimIsSystemEffectFile("data/effect_temp.ha6") && FpanimIsSystemEffectFile("data/effect.LST"));
	CHECK(!FpanimIsSystemEffectFile("data/chr000/effect.ha6") && !FpanimIsSystemEffectFile("data/effects.ha6"));
	CHECK(!FpanimIsSystemEffectFile("data/effect") && !FpanimIsSystemEffectFile("script/effect.txt"));
	CHECK(FpanimParseReloadReply("OK reloadsyseff: re-pointed 1 player(s), 3 effect(s)").sysEffect);
	CHECK(!FpanimParseReloadReply("ERROR unknown request (ping | reloadsyseff | ...)").sysEffect);
	// System/ files -> reloadsys System/<file> (BootySama #112).
	CHECK(FpanimIsSystemFile("System/cmd_chr000.txt") && FpanimIsSystemFile("./system\\BtlCharaTbl.txt"));
	CHECK(!FpanimIsSystemFile("System/str_jp/x.csv") && !FpanimIsSystemFile("data/System/x.txt") && !FpanimIsSystemFile("System/"));
	CHECK(FpanimSysReloadRequest("system/cmd_Okita.txt") == "reloadsys System/cmd_Okita.txt");
	{
		auto i = FpanimParseReloadReply("OK reloadsys: training command list of chara 0 (chr000): ..., 56 -> 54 entries");
		CHECK(i.sysReload && !i.sysEffect && !i.stdReload);
		CHECK(!FpanimParseReloadReply("OK reloadsyseff: ./data/effect.txt: 1 record(s)").sysReload);
	}
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

	// Main-editor saves (HA6 / .pat / .cg / .pal) -> the IM mod folder and the reload request (FpanimPlanSave).
	const std::string uniDir = "C:\\Program Files (x86)\\Steam\\steamapps\\common\\UNDER NIGHT IN-BIRTH II Sys Celes";
	const std::string uniMods = "C:/Program Files (x86)/Steam/steamapps/common/UNDER NIGHT IN-BIRTH II Sys Celes/UNI2-IM/Mods";
	CHECK(FpanimImModsRoot("uni2", uniDir) == uniMods);
	CHECK(FpanimImModsRoot("mbtl", "D:\\g\\") == "D:/g/MBTL-IM/Mods");
	CHECK(FpanimImModsRoot("mbaacc", "D:\\g").empty() && FpanimImModsRoot("uni2", "").empty());
	CHECK(FpanimGameKey("C:\\Users\\u\\AppData\\Local\\Temp\\hantei_archive\\uni2_d\\data\\chr003\\chr003.ha6") == "data/chr003/chr003.ha6");
	CHECK(FpanimGameKey("C:/work/uni2/data/chr000/chr000.pat") == "data/chr000/chr000.pat");
	CHECK(FpanimGameKey("data/chr000/chr000.cg") == "data/chr000/chr000.cg");
	CHECK(FpanimGameKey("C:/g/script/btl_MvFunc.txt") == "script/btl_MvFunc.txt");
	CHECK(FpanimGameKey("C:/x/chr000.ha6").empty());
	const char* kinds[] = {"chr003.ha6", "chr003.pat", "chr003.cg", "chr003.pal", "chr003_temp.ha6"};
	for (const char* k : kinds) {
		const std::string src = "C:\\Temp\\hantei_archive\\uni2_d\\data\\chr003\\" + std::string(k);
		auto p = FpanimPlanSave("uni2", uniDir, src, true, true, false);
		CHECK(p.key == "data/chr003/" + std::string(k));
		CHECK(p.modsDst == uniMods + "/data/chr003/" + k);
		CHECK(!p.alreadyInMods && p.chr == "chr003" && !p.shared);
		CHECK(p.request == "reloadchara chr003" && p.pipe == "fpan-im-uni2");
	}
	// Custom-named UNI2-IM characters (data/Okita/Okita.ha6) reload like chrNNN ones; fpan-im finds them by folder.
	CHECK(FpanimCharaFolder("data/chr003/chr003.ha6") == "chr003" && FpanimCharaFolder("data/chr000/chr000_mv_0.txt") == "chr000");
	CHECK(FpanimCharaFolder("data/Okita/Okita.ha6") == "okita" && FpanimCharaFolder("data/Okita/Okita_0.txt") == "okita");
	CHECK(FpanimCharaFolder("data/Okita/okita_cmd_0.txt") == "okita");
	CHECK(FpanimCharaFolder("data/_csel/csel.ha6").empty() && FpanimCharaFolder("data/effect.HA6").empty());
	CHECK(FpanimCharaFolder("data/chr003/chr0030.ha6").empty() && FpanimCharaFolder("data/chr003/sub/chr003.ha6").empty());
	CHECK(FpanimCharaFolder("data/Oki ta/Oki ta.ha6").empty() && FpanimCharaFolder("script/btl_MvFunc.txt").empty());
	{   // BootySama's case: a custom character edited in place inside an IM pack, no game folder set
		const std::string src = "E:\\SteamLibrary\\steamapps\\common\\UNDER NIGHT IN-BIRTH II Sys Celes\\UNI2-IM\\Packs\\___UniTM\\data\\Okita\\Okita.ha6";
		CHECK(FpanimInImFolder("uni2", src) && !FpanimInImFolder("mbtl", src));
		auto p = FpanimPlanSave("uni2", "", src, true, true, false);
		CHECK(p.key == "data/Okita/Okita.ha6" && p.chr == "okita" && !p.shared);
		CHECK(p.alreadyInMods && p.modsDst == "E:/SteamLibrary/steamapps/common/UNDER NIGHT IN-BIRTH II Sys Celes/UNI2-IM/Packs/___UniTM/data/Okita/Okita.ha6");
		CHECK(p.request == "reloadchara okita");
		// with the game folder set and "save into the IM mod folder" on, it is still not copied into Mods (it would shadow the pack)
		auto q = FpanimPlanSave("uni2", "E:\\SteamLibrary\\steamapps\\common\\UNDER NIGHT IN-BIRTH II Sys Celes", src, true, true, true);
		CHECK(q.alreadyInMods && q.modsDst == p.modsDst && q.request == "reloadchara okita live");
	}
	CHECK(!FpanimInImFolder("uni2", "C:/g/UNI2-IM/Packs/") && !FpanimInImFolder("uni2", "C:/g/UNI2-IM/x/data/chr000/chr000.ha6"));
	CHECK(FpanimInImFolder("mbtl", "D:/MBTL/MBTL-IM/Mods/data/chr001/chr001.ha6"));
	{   // MBTL, live mode
		auto p = FpanimPlanSave("mbtl", "C:\\MBTL", "C:/w/mbtl_data000/data/chr012/chr012.pal", true, true, true);
		CHECK(p.modsDst == "C:/MBTL/MBTL-IM/Mods/data/chr012/chr012.pal" && p.request == "reloadchara chr012 live" && p.pipe == "fpan-im-mbtl");
	}
	{   // the setting off: no mod-folder copy (the plain save still happened), the request is still named
		auto p = FpanimPlanSave("uni2", uniDir, "C:/w/data/chr000/chr000.ha6", false, true, false);
		CHECK(p.modsDst.empty() && p.request == "reloadchara chr000");
	}
	{   // hot reload off: copy, no request
		auto p = FpanimPlanSave("uni2", uniDir, "C:/w/data/chr000/chr000.ha6", true, false, false);
		CHECK(!p.modsDst.empty() && p.request.empty());
	}
	{   // the CG manager's "Save to mod folder" wrote the mod-folder file itself
		auto p = FpanimPlanSave("uni2", uniDir, uniDir + "\\UNI2-IM\\Mods\\data\\chr000\\chr000.cg", false, true, false);
		CHECK(p.alreadyInMods && p.modsDst == uniMods + "/data/chr000/chr000.cg" && p.request == "reloadchara chr000");
	}
	{   // shared files -> reloadstd; other data files: copied, not reloaded
		auto p = FpanimPlanSave("uni2", uniDir, "C:/w/data/VectorTable.txt", true, true, true);
		CHECK(p.shared && p.chr.empty() && p.request == "reloadstd live" && p.modsDst == uniMods + "/data/VectorTable.txt");
		auto q = FpanimPlanSave("mbtl", "C:/M", "C:/w/script/btl_StdMoveTbl.txt", true, true, false);
		CHECK(q.shared && q.request == "reloadstd" && q.pipe == "fpan-im-mbtl");
		auto r = FpanimPlanSave("uni2", uniDir, "C:/w/data/effect.ha6", true, true, false);
		CHECK(!r.shared && r.sysEffect && r.chr.empty() && r.request == "reloadsyseff" && !r.modsDst.empty());
		auto rl = FpanimPlanSave("mbtl", "C:/M", "C:/w/data/sys_effect.pat", true, true, true);
		CHECK(rl.sysEffect && rl.request == "reloadsyseff" && rl.pipe == "fpan-im-mbtl");
		auto rs = FpanimPlanSave("uni2", uniDir, "C:/w/System/cmd_chr000.txt", true, true, false);
		CHECK(rs.system && rs.key == "System/cmd_chr000.txt" && rs.request == "reloadsys System/cmd_chr000.txt" && rs.chr.empty());
		CHECK(rs.modsDst == uniMods + "/System/cmd_chr000.txt");
		auto ro = FpanimPlanSave("uni2", uniDir, "C:/w/data/_csel/csel.ha6", true, true, false);
		CHECK(!ro.sysEffect && ro.chr.empty() && ro.request.empty());
		auto t = FpanimPlanSave("uni2", uniDir, "C:/w/data/chr000/sub/x.ha6", true, true, false);
		CHECK(t.chr.empty() && t.request.empty());
	}
	{   // other games, files outside data/ or script/, and ".." are left alone
		CHECK(FpanimPlanSave("mbaacc", "C:/g", "C:/w/data/chr000/chr000.ha6", true, true, false).key.empty());
		auto p = FpanimPlanSave("uni2", uniDir, "C:/w/chr000.ha6", true, true, false);
		CHECK(p.key.empty() && p.modsDst.empty() && p.request.empty());
		auto q = FpanimPlanSave("uni2", uniDir, "C:/w/data/../../x/chr000.ha6", true, true, false);
		CHECK(q.modsDst.empty() && q.request.empty());
		auto r = FpanimPlanSave("uni2", "", "C:/w/data/chr000/chr000.ha6", true, true, false);
		CHECK(r.modsDst.empty() && r.request == "reloadchara chr000");
	}

	if (g_fail) return 1;
	std::printf("fpanim_link_test: ok\n");
	return 0;
}
