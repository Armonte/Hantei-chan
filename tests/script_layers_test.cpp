// Script panel merged file list (src/squirrel/script_layers.h): a temp tree shaped like a UNI2 install with UNI2-IM Mods /
// Packs and fpan-im mods / packs. New data scripts must appear, the fpan-im VFS priority must pick the winner, every copy is
// tagged, and a pack / mod / data folder chosen as the "game folder" resolves to the pack and the install above it.
#include "squirrel/script_layers.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;
static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)

static void Put(const fs::path& p, const std::string& text)
{
	fs::create_directories(p.parent_path());
	std::ofstream(p, std::ios::binary) << text;
}

using namespace sqedit;

static const ScriptFile* Find(const std::vector<ScriptFile>& v, const std::string& rel)
{
	for (const auto& f : v) if (f.rel == rel) return &f;
	return nullptr;
}
static std::string WinTag(const ScriptFile* f, const std::vector<ScriptLayer>& L)
{
	return f && f->winner >= 0 ? L[f->copies[f->winner].layer].tag : "<none>";
}

int main(int argc, char** argv)
{
	const fs::path g = argc > 1 ? fs::u8path(argv[1]) : fs::temp_directory_path() / "hc_script_layers_test" / "UNDER NIGHT IN-BIRTH II Sys Celes";
	std::error_code ec;
	fs::remove_all(g.parent_path(), ec);
	const std::string sq = "t <- {}\nfunction F() { return 1; }\n";
	// base install: loose script/ and data/
	Put(g / "uni2.exe", "MZ");
	Put(g / "script/Battle_Std.txt", sq);
	Put(g / "script/VanillaPlusConfig.txt", sq);
	Put(g / "data/chr001/chr001_mv_0.txt", sq);
	Put(g / "data/chr001/chr001.txt", "chr001.ha6\n");   // not Squirrel
	// System/: training command lists are Squirrel, the replay header is ini-style (BootySama #112)
	Put(g / "System/cmd_chr001.txt", "Training_cmdList <- { list = [] };\n");
	Put(g / "System/HeaderReplayVS.txt", "[Header]\nName = x\n");
	Put(g / "UNI2-IM/Packs/___UniTM/System/cmd_chr001.txt", "Training_cmdList <- { list = [] };\n");
	// UNI2-IM: Mods + two packs, ___UniTM first by the ini Order, zzOff disabled
	Put(g / "UNI2-IM/UNI2_IM.ini", "[Mods]\nOrder=___UniTM|aaPack\nOff=zzOff\n");
	Put(g / "UNI2-IM/Mods/data/chr001/chr001_mv_0.txt", sq);
	Put(g / "UNI2-IM/Packs/___UniTM/data/chr001/chr001_mv_0.txt", sq);
	Put(g / "UNI2-IM/Packs/___UniTM/data/chr001/chr001_tm_extra.txt", sq);   // brand-new data script
	Put(g / "UNI2-IM/Packs/___UniTM/data/VectorTable.txt", "VectorTable <- [ [0, 1] ];\n");
	Put(g / "UNI2-IM/Packs/___UniTM/data/_combase.txt", sq);
	Put(g / "UNI2-IM/Packs/___UniTM/data/notes.txt", "just some notes\n");   // not Squirrel
	Put(g / "UNI2-IM/Packs/aaPack/data/BattleInfo.txt", "BattleInfo <- {};\n");
	Put(g / "UNI2-IM/Packs/aaPack/data/chr001/chr001_tm_extra.txt", sq);
	Put(g / "UNI2-IM/Packs/zzOff/data/BoundEff.txt", "BoundEff <- {};\n");
	// fpan-im: mods + one pack
	Put(g / "fpan-im/fpan-im.ini", "[packs]\norder=fp1\n");
	Put(g / "fpan-im/packs/fp1/script/btl_New.txt", sq);
	Put(g / "fpan-im/packs/fp1/data/BattleInfo.txt", "BattleInfo <- {};\n");
	Put(g / "fpan-im/mods/data/chr002/chr002_newthing.txt", sq);

	// ---- layers: fpan-im mods > fpan-im packs > IM Mods > IM Packs (ini order) > loose > archive
	std::vector<ScriptLayer> L = ScriptLayersFor(g.u8string(), "uni2");
	std::vector<std::string> tags;
	for (const auto& l : L) tags.push_back(l.tag);
	CHECK(tags.size() == 7);
	if (tags.size() == 7) {
		CHECK(tags[0] == "fpan-im mods");
		CHECK(tags[1] == "fpan-im pack fp1");
		CHECK(tags[2] == "UNI2-IM Mods");
		CHECK(tags[3] == "pack ___UniTM");
		CHECK(tags[4] == "pack aaPack");
		CHECK(tags[5] == "loose");
		CHECK(tags[6] == "pack zzOff (off)");
	}
	const std::vector<std::string> archive = {"data/chr001/chr001_mv_0.txt", "data/chr001/chr001_sc_0.txt", "data/VectorTable.txt",
		"data/BoundEff.txt", "data/chr001/chr001.ha6", "script/Battle_Init.txt", "System/BtlCharaTbl.txt",
		"System/str_jp/CharaData.csv", "System/HeaderRepDummy.txt"};
	const std::vector<ScriptFile> files = ListScriptFiles(L, archive);
	CHECK(L.back().tag == "archive");

	// new loose script/ and new data/ scripts from every layer appear
	CHECK(Find(files, "script/VanillaPlusConfig.txt") && WinTag(Find(files, "script/VanillaPlusConfig.txt"), L) == "loose");
	CHECK(WinTag(Find(files, "data/chr001/chr001_tm_extra.txt"), L) == "pack ___UniTM");
	CHECK(Find(files, "data/chr001/chr001_tm_extra.txt") && Find(files, "data/chr001/chr001_tm_extra.txt")->copies.size() == 2);
	CHECK(WinTag(Find(files, "data/chr002/chr002_newthing.txt"), L) == "fpan-im mods");
	CHECK(WinTag(Find(files, "script/btl_New.txt"), L) == "fpan-im pack fp1");
	CHECK(!Find(files, "data/chr001/chr001.txt") && !Find(files, "data/notes.txt") && !Find(files, "data/chr001/chr001.ha6"));
	// System/ scripts: the pack copy wins over the loose one; ini-style and csv files are left out
	CHECK(WinTag(Find(files, "system/cmd_chr001.txt"), L) == "pack ___UniTM");
	CHECK(Find(files, "system/cmd_chr001.txt") && Find(files, "system/cmd_chr001.txt")->copies.size() == 2);
	CHECK(WinTag(Find(files, "system/BtlCharaTbl.txt"), L) == "archive");
	CHECK(!Find(files, "system/HeaderReplayVS.txt") && !Find(files, "system/HeaderRepDummy.txt") && !Find(files, "system/str_jp/CharaData.csv"));
	// archive-only entries
	CHECK(WinTag(Find(files, "data/chr001/chr001_sc_0.txt"), L) == "archive");
	CHECK(WinTag(Find(files, "script/Battle_Init.txt"), L) == "archive");
	// shared data-root files (reloadstd), from every layer
	CHECK(WinTag(Find(files, "data/VectorTable.txt"), L) == "pack ___UniTM");
	CHECK(Find(files, "data/VectorTable.txt") && Find(files, "data/VectorTable.txt")->copies.size() == 2);
	CHECK(WinTag(Find(files, "data/_combase.txt"), L) == "pack ___UniTM");
	CHECK(WinTag(Find(files, "data/BattleInfo.txt"), L) == "fpan-im pack fp1");
	CHECK(Find(files, "data/BattleInfo.txt") && Find(files, "data/BattleInfo.txt")->copies.size() == 2);
	// BoundEff: the off pack is listed but never wins; the archive copy does
	CHECK(WinTag(Find(files, "data/BoundEff.txt"), L) == "archive");
	CHECK(Find(files, "data/BoundEff.txt") && Find(files, "data/BoundEff.txt")->copies.size() == 2);
	// priority: chr001_mv_0 is in IM Mods, ___UniTM, loose and the archive: IM Mods wins, all four listed in order
	{
		const ScriptFile* f = Find(files, "data/chr001/chr001_mv_0.txt");
		CHECK(f && f->copies.size() == 4);
		if (f && f->copies.size() == 4) {
			CHECK(L[f->copies[0].layer].tag == "UNI2-IM Mods" && f->winner == 0);
			CHECK(L[f->copies[1].layer].tag == "pack ___UniTM");
			CHECK(L[f->copies[2].layer].tag == "loose");
			CHECK(L[f->copies[3].layer].tag == "archive");
			CHECK(ScriptCopyLabel(*f, f->copies[1], L) == "data/chr001/chr001_mv_0.txt  [pack ___UniTM]");
		}
	}
	// fpan-im mods vs UNI2-IM Mods: the newer file wins
	{
		Put(g / "fpan-im/mods/data/chr001/chr001_mv_0.txt", sq);
		fs::last_write_time(g / "fpan-im/mods/data/chr001/chr001_mv_0.txt", fs::file_time_type::clock::now() - std::chrono::hours(48));
		std::vector<ScriptLayer> L2 = ScriptLayersFor(g.u8string(), "uni2");
		const auto f2 = ListScriptFiles(L2, {});
		CHECK(WinTag(Find(f2, "data/chr001/chr001_mv_0.txt"), L2) == "UNI2-IM Mods");
		fs::last_write_time(g / "fpan-im/mods/data/chr001/chr001_mv_0.txt", fs::file_time_type::clock::now() + std::chrono::hours(1));
		std::vector<ScriptLayer> L3 = ScriptLayersFor(g.u8string(), "uni2");
		const auto f3 = ListScriptFiles(L3, {});
		CHECK(WinTag(Find(f3, "data/chr001/chr001_mv_0.txt"), L3) == "fpan-im mods");
	}

	// ---- classification
	CHECK(IsSquirrelScriptRel("data/VectorTable.txt") && IsSquirrelScriptRel("data/BattleInfo.txt"));
	CHECK(IsSquirrelScriptRel("data/BoundEff.txt") && IsSquirrelScriptRel("data/_combase.txt"));
	CHECK(IsSquirrelScriptRel("script/anything.txt") && !IsSquirrelScriptRel("data/effect.txt"));
	CHECK(IsSquirrelScriptRel("System/cmd__common.txt") && IsSquirrelScriptRel("system\\PlayBonusTbl.txt"));
	CHECK(!IsSquirrelScriptRel("System/HeaderReplayVS.txt") && !IsSquirrelScriptRel("System/str_jp/a.txt"));
	const std::string ini = "name=foo\n";
	CHECK(!IsSquirrelScriptRel("data/chr009/foo.txt", &ini) && IsSquirrelScriptRel("data/chr009/foo.txt", &sq));

	// ---- choosing a pack / mod / data folder
	const std::string gs = g.generic_u8string();
	{
		auto r = ResolveScriptRoot(gs + "/UNI2-IM/Packs/___UniTM/data");   // BootySama's pick
		CHECK(r.error.empty() && r.gameDir == gs && r.editRoot == gs + "/UNI2-IM/Packs/___UniTM" && r.editTag == "pack ___UniTM");
		auto r2 = ResolveScriptRoot(gs + "\\UNI2-IM\\Packs\\___UniTM\\");
		CHECK(r2.error.empty() && r2.gameDir == gs && r2.editTag == "pack ___UniTM");
		auto r3 = ResolveScriptRoot(gs + "/UNI2-IM/Mods/script");
		CHECK(r3.gameDir == gs && r3.editRoot == gs + "/UNI2-IM/Mods" && r3.editTag == "UNI2-IM Mods");
		auto r4 = ResolveScriptRoot(gs + "/fpan-im/mods");
		CHECK(r4.gameDir == gs && r4.editTag == "fpan-im mods");
		auto r5 = ResolveScriptRoot(gs + "/fpan-im/packs/fp1/data");
		CHECK(r5.gameDir == gs && r5.editTag == "fpan-im pack fp1");
		auto r6 = ResolveScriptRoot(gs + "/data");
		CHECK(r6.error.empty() && r6.gameDir == gs && r6.editRoot.empty());
		auto r7 = ResolveScriptRoot(gs);
		CHECK(r7.error.empty() && r7.gameDir == gs && r7.editRoot.empty());
		auto r8 = ResolveScriptRoot(gs + "/UNI2-IM");
		CHECK(r8.gameDir == gs && r8.editRoot.empty());
		auto bad = ResolveScriptRoot((g.parent_path() / "nothing_here").generic_u8string());
		CHECK(!bad.error.empty());
	}
	if (argc <= 1) fs::remove_all(g.parent_path(), ec);
	if (g_fail) std::fprintf(stderr, "%d failure(s)\n", g_fail);
	else std::printf("script_layers: all checks passed\n");
	return g_fail ? 1 : 0;
}
