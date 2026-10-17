// Script index (src/squirrel/script_index): definitions, search, usages and the move checks over every shipped game's
// scripts (vanilla data must report no missing move tables), plus the "create the missing Mv_ state" stub.
#include "squirrel/script_index.h"

#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace sqedit;
static int g_fail = 0;
#define CHECK(x) do { if (!(x)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); ++g_fail; } } while (0)

static std::vector<std::string> ScriptFiles(const fs::path& root)
{
	std::vector<std::string> out;
	std::error_code ec;
	for (const char* sub : {"script", "data"}) {
		for (auto it = fs::recursive_directory_iterator(root / sub, ec); it != fs::recursive_directory_iterator(); it.increment(ec)) {
			if (ec) break;
			if (!it->is_regular_file(ec) || it->path().extension() != ".txt") continue;
			const std::string rel = it->path().lexically_relative(root).generic_u8string();
			const std::string n = it->path().filename().u8string();
			// the Squirrel files (script/ and the per-character _mv/_cmd/_sc/_com/_se files), not ini-style project lists
			if (rel.rfind("script/", 0) == 0 || n.find("_mv") != std::string::npos || n.find("_cmd") != std::string::npos ||
				n.find("_sc") != std::string::npos || n.find("_com") != std::string::npos || n.find("_se_") != std::string::npos)
				out.push_back(rel);
		}
	}
	return out;
}

static void Synthetic()
{
	const std::string mv = "local t = {};\r\nt.Mv_A <-\r\n{\r\n\tfunction Init_After()\r\n\t{\r\n\t}\r\n}\r\n\r\nCHR_MoveTable <- Battle_Std.MakeMoveTable( t, CHR_CommandTable, 1 );\r\n";
	int line = -1;
	const std::string out = ScriptIndex::InsertMoveTableStub(mv, "Mv_Standby", &line);
	CHECK(line == 8);
	CHECK(out.find("t.Mv_Standby <-\r\n{\r\n}\r\n\r\nCHR_MoveTable") != std::string::npos);
	ScriptIndex ix;
	ix.updateFile("data/chr/chr_mv_0.txt", mv);
	ix.updateFile("data/chr/chr_cmd_0.txt", "local t =\r\n{\r\n\tSkill_A = { num = 1, UpdateTable = { name = \"Mv_A\" } },\r\n\tSkill_B =\r\n\t{\r\n\t\tUpdateTable = { name = \"Mv_Misspelt\", }\r\n\t},\r\n}\r\n");
	CHECK(!ix.find("Mv_A").empty() && ix.find("Mv_A")[0]->kind == ScriptDef::MoveTable);
	auto fn = ix.find("Init_After");
	CHECK(!fn.empty() && fn[0]->owner == "Mv_A");
	auto cmd = ix.find("Skill_B");
	CHECK(!cmd.empty() && cmd[0]->kind == ScriptDef::CmdEntry && cmd[0]->value == "Mv_Misspelt");
	auto probs = ix.checkMoves("data/chr");
	CHECK(probs.size() == 1 && probs[0].fixArg == "Mv_Misspelt" && probs[0].fix == ScriptProblem::CreateMoveTable && probs[0].fixFile == "data/chr/chr_mv_0.txt");
	ix.updateFile("data/chr/chr_mv_0.txt", out.substr(0, out.find("CHR_MoveTable")) + "t.Mv_Misspelt <-\r\n{\r\n}\r\n");
	CHECK(ix.checkMoves("data/chr").empty());
	ix.updateFile("script/Battle_Std.txt", "//I don't use it because it's old\r\nBattle_Std.GetHaimenGamenHajiDistance <- function(tbl={})\r\n{\r\n\tlocal tp = BMvTbl.GetPosition( 0 ); //now\r\n}\r\n");
	auto lib = ix.find("GetHaimenGamenHajiDistance");
	CHECK(lib.size() == 1 && lib[0]->kind == ScriptDef::Function && lib[0]->owner == "Battle_Std" && lib[0]->params == "tbl={}");
	CHECK(lib.size() == 1 && lib[0]->doc == "I don't use it because it's old");
	CHECK(ix.members("Battle_Std").size() == 1);
	SearchOptions o; o.wholeWord = true;
	CHECK(ix.search("Mv_A", o).size() >= 2);
	CHECK(ix.usages("Mv_A").size() >= 2);
}

static void Shipped(const char* label, const fs::path& root)
{
	std::error_code ec;
	if (!fs::is_directory(root / "data", ec)) { std::printf("skip %s\n", label); return; }
	ScriptIndex ix;
	ix.build(root.u8string(), ScriptFiles(root));
	int mvs = 0, funcs = 0, cmds = 0, problems = 0, chars = 0;
	for (const auto& d : ix.defs()) { mvs += d.kind == ScriptDef::MoveTable; funcs += d.kind == ScriptDef::Function; cmds += d.kind == ScriptDef::CmdEntry; }
	for (auto it = fs::directory_iterator(root / "data", ec); it != fs::directory_iterator(); it.increment(ec)) {
		if (!it->is_directory(ec)) continue;
		const std::string dir = "data/" + it->path().filename().u8string();
		auto p = ix.checkMoves(dir);
		if (!p.empty()) ++chars;
		for (size_t i = 0; i < p.size() && i < 3; ++i) std::printf("  %s %s:%d %s\n", label, p[i].file.c_str(), p[i].line + 1, p[i].fixArg.c_str());
		problems += (int)p.size();
	}
	std::printf("%s: %zu files, %d move tables, %d functions, %d cmd entries, %d missing-move reports in %d folders\n", label,
		ix.files().size(), mvs, funcs, cmds, problems, chars);
	CHECK(mvs > 50 && funcs > 50);
}

#ifdef _WIN32
#include <windows.h>
// Crash report for the intermittent fault this test has shown (exit 0xC0000005 / 0xC0000374): code, address and a
// backtrace relative to the exe base, to resolve with nm / addr2line.
static LONG WINAPI CrashReport(EXCEPTION_POINTERS* e)
{
	const auto base = (uintptr_t)GetModuleHandleW(nullptr);
	std::fprintf(stderr, "CRASH code %08lx at exe+%llx\n", e->ExceptionRecord->ExceptionCode,
		(unsigned long long)((uintptr_t)e->ExceptionRecord->ExceptionAddress - base));
	void* frames[48];
	const USHORT n = RtlCaptureStackBackTrace(0, 48, frames, nullptr);
	for (USHORT i = 0; i < n; ++i) std::fprintf(stderr, "  #%u exe+%llx\n", i, (unsigned long long)((uintptr_t)frames[i] - base));
	std::fflush(stderr);
	return EXCEPTION_CONTINUE_SEARCH;
}
#endif

int main(int argc, char** argv)
{
#ifdef _WIN32
	AddVectoredExceptionHandler(1, [](EXCEPTION_POINTERS* e) -> LONG {
		const DWORD c = e->ExceptionRecord->ExceptionCode;
		return c == 0xC0000005 || c == 0xC0000374 || c == 0xC00000FD ? CrashReport(e) : EXCEPTION_CONTINUE_SEARCH;
	});
	setvbuf(stdout, nullptr, _IONBF, 0);
#endif
	Synthetic();
	Shipped("uni2", "C:/dev/hantei-chan/gamedata/uni2");
	Shipped("mbtl", "C:/dev/hantei-chan/gamedata/mbtl");
	Shipped("dfci", "C:/dev/transmission/dfci+transmission");
	Shipped("unib", "C:/games/unib");
	std::printf(g_fail ? "squirrel_index_test: %d failures\n" : "squirrel_index_test: ok\n", g_fail);
	return g_fail ? 1 : 0;
}
