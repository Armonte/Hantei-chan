// sqtool: compile (and optionally run) French-Bread Squirrel scripts with the game's exact VM.
//
// The post-MBAACC French-Bread games (UNIB, DFCI, UNI2, MBTL, ...) embed Squirrel 2.2.4 stable
// (ANSI, 32-bit integers, 32-bit floats) bound with an early Sqrat. Scripts ship as plain CP932
// source in .txt files. See docs/squirrel/SCOPE.md.
//
//   sqtool check <gamedir> [--api <game>_api.json] [--run] [--report out.tsv] [--expect-all]
//     Compiles every Squirrel .txt under <gamedir>/script and <gamedir>/data with Squirrel 2.2.4,
//     exactly as the game does (raw bytes, sq_compilebuffer). With --run it also executes
//     script/Battle_Init.txt and script/Battle_Std.txt against native stubs generated from the
//     IDB-extracted API table, with a working __dofile__.
//   sqtool compile <file.txt>...   compile single files
//
// Exit code: 0 when every Squirrel file compiled (and --run succeeded), 1 otherwise,
// 77 when <gamedir> does not exist (ctest SKIP_RETURN_CODE).
//
// Output goes through quill (console sink); no printf.

#include <squirrel.h>
#include "sq_api.h"
#include "../framedata.h"
#include <windows.h>
#include <sqstdaux.h>
#include <sqstdblob.h>
#include <sqstdio.h>
#include <sqstdmath.h>
#include <sqstdstring.h>
#include <sqstdsystem.h>

#include <quill/Backend.h>
#include <quill/Frontend.h>
#include <quill/LogMacros.h>
#include <quill/Logger.h>
#include <quill/sinks/ConsoleSink.h>
#include <quill/sinks/FileSink.h>

#include <json/json.hpp>

#include <algorithm>
#include <cstdarg>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <regex>
#include <set>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using json = nlohmann::json;

static quill::Logger *g_log = nullptr;

#define LOGI(fmt, ...) QUILL_LOG_INFO(g_log, fmt, ##__VA_ARGS__)
#define LOGW(fmt, ...) QUILL_LOG_WARNING(g_log, fmt, ##__VA_ARGS__)
#define LOGE(fmt, ...) QUILL_LOG_ERROR(g_log, fmt, ##__VA_ARGS__)

static void InitLog()
{
	quill::BackendOptions opts;
	opts.thread_name = "sqtool_log";
	opts.sleep_duration = std::chrono::milliseconds(1);
	quill::Backend::start(opts);
	auto sink = quill::Frontend::create_or_get_sink<quill::ConsoleSink>("sqtool_console");
	quill::PatternFormatterOptions pattern{"%(message)"};
	g_log = quill::Frontend::create_or_get_logger("sqtool", std::move(sink), pattern);
}

// ---------------------------------------------------------------- file helpers

static bool ReadFile(const fs::path &p, std::string &out)
{
	std::ifstream f(p, std::ios::binary);
	if (!f)
		return false;
	out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
	return true;
}

// Script text is CP932. Log lines must stay valid UTF-8 for the console, so non-ASCII bytes are
// escaped (the editor will convert CP932 properly; this tool only reports).
static std::string Printable(const std::string &s)
{
	std::string o;
	for (unsigned char c : s) {
		if (c >= 0x20 && c < 0x7f)
			o += (char)c;
		else {
			char b[8];
			snprintf(b, sizeof b, "\\x%02X", c);
			o += b;
		}
	}
	return o;
}

// ---------------------------------------------------------------- VM

struct CompileError {
	std::string desc, source;
	int line = 0, column = 0;
	bool set = false;
};
static CompileError g_lastCompileError;
static std::string g_runtimeError;
static fs::path g_gameRoot;
static int g_dofileDepth = 0;

static void CompilerError(HSQUIRRELVM, const SQChar *desc, const SQChar *source, SQInteger line, SQInteger column)
{
	g_lastCompileError = {desc ? desc : "", source ? source : "", (int)line, (int)column, true};
}

static void PrintFunc(HSQUIRRELVM, const SQChar *fmt, ...)
{
	char buf[2048];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf, sizeof buf, fmt, ap);
	va_end(ap);
	std::string s = Printable(buf);
	if (!s.empty())
		LOGI("    [script] {}", s);
}

static SQInteger RuntimeErrorHandler(HSQUIRRELVM v)
{
	const SQChar *err = "?";
	if (sq_gettop(v) >= 2)
		sq_getstring(v, 2, &err);
	SQStackInfos si;
	std::string where;
	if (SQ_SUCCEEDED(sq_stackinfos(v, 1, &si)))
		where = std::string(si.source ? si.source : "?") + ":" + std::to_string(si.line) + " in " + (si.funcname ? si.funcname : "?");
	g_runtimeError = std::string(err ? err : "?") + " @ " + where;
	return 0;
}

// Compile a buffer the way the game does (Sq_CompileBufferAndRun in UNI2 1.42 / MBTL): the raw file
// bytes, no transcoding, raiseerror on.
static bool CompileBuffer(HSQUIRRELVM v, const std::string &src, const std::string &name)
{
	g_lastCompileError = {};
	return SQ_SUCCEEDED(sq_compilebuffer(v, src.data(), (SQInteger)src.size(), name.c_str(), SQTrue));
}

static bool RunFile(HSQUIRRELVM v, const fs::path &p, const std::string &name)
{
	std::string src;
	if (!ReadFile(p, src)) {
		LOGE("  cannot read {}", p.string());
		return false;
	}
	if (!CompileBuffer(v, src, name)) {
		LOGE("  compile error {}:{}:{} {}", name, g_lastCompileError.line, g_lastCompileError.column, Printable(g_lastCompileError.desc));
		return false;
	}
	sq_pushroottable(v);
	g_runtimeError.clear();
	bool ok = SQ_SUCCEEDED(sq_call(v, 1, SQFalse, SQTrue));
	sq_pop(v, 1);
	if (!ok)
		LOGE("  runtime error in {}: {}", name, Printable(g_runtimeError));
	return ok;
}

// __dofile__(path): the game resolves "./script/x.txt" against its data root (archive or loose).
static SQInteger Native_dofile(HSQUIRRELVM v)
{
	const SQChar *path = nullptr;
	if (SQ_FAILED(sq_getstring(v, 2, &path)))
		return sq_throwerror(v, "__dofile__: string expected");
	fs::path p = g_gameRoot / fs::path(path).lexically_normal();
	if (g_dofileDepth > 32)
		return sq_throwerror(v, "__dofile__: recursion too deep");
	++g_dofileDepth;
	bool ok = RunFile(v, p, path);
	--g_dofileDepth;
	sq_pushinteger(v, ok ? 1 : 0);
	return 1;
}

static SQInteger Native_LoadString(HSQUIRRELVM v)
{
	const SQChar *path = nullptr;
	if (SQ_FAILED(sq_getstring(v, 2, &path)))
		return sq_throwerror(v, "SQ_LoadString: string expected");
	std::string s;
	ReadFile(g_gameRoot / fs::path(path).lexically_normal(), s);
	sq_pushstring(v, s.c_str(), (SQInteger)s.size());
	return 1;
}

// Stub native (sq_api.h): records the call and returns the declared type's value (a real class instance for class returns).
static std::map<std::string, int> g_stubCalls;
static SQInteger Native_Stub(HSQUIRRELVM v)
{
	const std::string tag = sqedit::StubTag(v);
	g_stubCalls[tag.substr(tag.find('|') + 1)]++;
	if (const SQInteger r = sqedit::CallTracedNative(v, tag); r >= 0)
		return r;
	// File-existence probes answer truthfully (read-only, inside the game root): the bootstrap branches on them
	// (DFCI Battle_Init loads btl_Define_LocalDebug.txt only when BtlMvStd.IsFileAlive says it is there).
	if (tag.size() >= 12 && tag.compare(tag.size() - 12, 12, ".IsFileAlive") == 0) {
		const SQChar *path = nullptr;
		std::error_code ec;
		bool alive = false;
		if (SQ_SUCCEEDED(sq_getstring(v, 2, &path))) {
			const fs::path rel = fs::path(path).lexically_normal();
			alive = !rel.empty() && *rel.begin() != ".." && fs::exists(g_gameRoot / rel, ec);
		}
		sq_pushinteger(v, alive ? 1 : 0);
		return 1;
	}
	return sqedit::PushStubReturn(v, tag);
}

static void RegisterGlobal(HSQUIRRELVM v, const char *name, SQFUNCTION f)
{
	sq_pushroottable(v);
	sq_pushstring(v, name, -1);
	sq_newclosure(v, f, 0);
	sq_createslot(v, -3);
	sq_pop(v, 1);
}

// Same library set as CSquirrel_ctor in UNI2 1.42 / MBTL: blob, io, system, math, string + error handlers.
static HSQUIRRELVM OpenGameVM(const json *api, sqedit::ApiInstallStats *st)
{
	HSQUIRRELVM v = sq_open(1024);
	sq_setprintfunc(v, PrintFunc);
	sq_setcompilererrorhandler(v, CompilerError);
	sq_pushroottable(v);
	sqstd_register_bloblib(v);
	sqstd_register_iolib(v);
	sqstd_register_systemlib(v);
	sqstd_register_mathlib(v);
	sqstd_register_stringlib(v);
	sq_pop(v, 1);
	sq_newclosure(v, RuntimeErrorHandler, 0);
	sq_seterrorhandler(v);
	RegisterGlobal(v, "__dofile__", Native_dofile);
	// UNI[st]/[cl-r] loads its defines with the stdlib dofile(".\\script\\x.txt"): resolve it against the game root too
	// (the stdlib one would read relative to the current directory).
	RegisterGlobal(v, "dofile", Native_dofile);
	RegisterGlobal(v, "SQ_LoadString", Native_LoadString);
	if (api && st)
		*st = sqedit::InstallSqratApi(v, *api, Native_Stub);
	return v;
}

// ---------------------------------------------------------------- file classification

// Which .txt files go through the Squirrel loader. From the exe path formats (UNI2 1.42 / MBTL):
// "%s/%s_mv_%d.txt", "_sc_", "_cmd_", "_com_", "_com_ranking_0", se_list (uses __dofile__), and
// every file under script/. Other .txt (BattleInfo, VectorTable, chrNNN.txt, csel_, edit_, ...)
// are not Squirrel; they are compiled too but only reported as "other".
static bool IsSquirrelScript(const fs::path &rel)
{
	std::string s = rel.generic_string();
	std::string f = rel.filename().string();
	if (s.rfind("script/", 0) == 0)
		return true;
	// UNI2/MBTL: chr000_mv_0.txt, npc093_mv.txt, chr000_se_list.txt. UNIB/DFCI: Aka_0_mv.txt, Acr_s_0_cmd.txt.
	static const std::regex re(R"(^(chr|npc)\d+_(mv|sc|cmd|com|com_ranking)(_\d+)?\.txt$|^(chr|npc)\d+_se_list\.txt$)"
							   R"(|^[A-Za-z]+(_s)?_\d+_(mv|sc|cmd|com)\.txt$)",
		std::regex::icase);
	return std::regex_match(f, re);
}

struct Result {
	std::string rel;
	bool squirrel = false, ok = false;
	CompileError err;
	size_t bytes = 0;
};

// Root-table slot type ("" when missing).
static std::string RootSlotType(HSQUIRRELVM v, const std::string &name)
{
	sq_pushroottable(v);
	sq_pushstring(v, name.c_str(), -1);
	std::string t;
	if (SQ_SUCCEEDED(sq_get(v, -2))) {
		switch (sq_gettype(v, -1)) {
		case OT_TABLE: t = "table"; break;
		case OT_ARRAY: t = "array"; break;
		case OT_NULL: t = "null"; break;
		default: t = "other"; break;
		}
	}
	sq_settop(v, 0);
	return t;
}

// Each character folder in the game's own load order (MBTL Chara_LoadBattleScripts 0x607650), variant 0:
//   <chr>_sc_0.txt  -> the engine reads root Battle_Status, then deletes it (CharaDatSq_LoadBattleStatus 0x802A20)
//   <chr>_cmd_0.txt -> the engine reads root <CHR>_CommandTable as an array; missing = the character cannot act
//   <chr>_mv_0.txt  -> the engine reads root <CHR>_MoveTable
//   <chr>_com_0.txt (CPU AI, on top of data/_combase.txt)
// UpdateTable.name of every entry of root array `table` (the moves a command can start).
static std::set<std::string> CommandMoveNames(HSQUIRRELVM v, const std::string &table)
{
	std::set<std::string> out;
	sq_pushroottable(v);
	sq_pushstring(v, table.c_str(), -1);
	if (SQ_SUCCEEDED(sq_get(v, -2)) && sq_gettype(v, -1) == OT_ARRAY) {
		sq_pushnull(v);
		while (SQ_SUCCEEDED(sq_next(v, -2))) {
			if (sq_gettype(v, -1) == OT_TABLE) {
				sq_pushstring(v, "UpdateTable", -1);
				if (SQ_SUCCEEDED(sq_get(v, -2))) {
					sq_pushstring(v, "name", -1);
					const SQChar *s = nullptr;
					if (SQ_SUCCEEDED(sq_get(v, -2)) && SQ_SUCCEEDED(sq_getstring(v, -1, &s)) && s) out.insert(s);
					sq_settop(v, sq_gettop(v) - 1);
					sq_pop(v, 1);
				}
			}
			sq_pop(v, 2);
		}
		sq_pop(v, 1);
	}
	sq_settop(v, 0);
	return out;
}

// Load-time context settings (sqtool check ... --side N --mode M --rapidbeat T).
static int g_side = 0, g_gameMode = 1, g_gameModeSub = 0, g_rapidBeat = 0;

// PTCN -> first pattern index of a project .txt's [DataFile] HA6 stack, loaded the way the editor and the game stack it
// (files after the project's own file only fill empty slots).
static std::map<std::string, int> LoadPatternCodes(const fs::path &txt)
{
	std::map<std::string, int> out;
	const std::string t = txt.string();
	const int n = GetPrivateProfileIntA("DataFile", "FileNum", 0, t.c_str());
	std::vector<std::string> names;
	for (int i = 0; i < n; ++i) {
		char key[16], val[260] = {};
		snprintf(key, sizeof key, i >= 100 ? "File%03d" : "File%02d", i);
		GetPrivateProfileStringA("DataFile", key, "", val, sizeof val, t.c_str());
		names.push_back(val);
	}
	if (names.empty()) return out;
	const int own = FrameData::StackSaveTarget(names);
	FrameData fd;
	bool any = false;
	for (int i = 0; i < (int)names.size(); ++i) {
		if (names[i].empty()) continue;
		const fs::path p = txt.parent_path() / names[i];
		std::error_code ec;
		if (!fs::exists(p, ec)) continue;
		if (fd.load(p.string().c_str(), any, own >= 0 && i > own)) any = true;
	}
	for (int i = 0; i < (int)fd.get_sequence_count(); ++i) {
		const Sequence *s = fd.get_sequence(i);
		if (s && !s->codeName.empty()) out.emplace(std::string(s->codeName.c_str()), i);
	}
	return out;
}

static bool RunCharacters(HSQUIRRELVM v, const fs::path &root)
{
	const auto effectCodes = LoadPatternCodes(root / "data" / "effect.txt");
	int clean = 0, bad = 0;
	std::error_code ec;
	std::vector<fs::path> dirs;
	for (auto &it : fs::directory_iterator(root / "data", ec))
		if (it.is_directory() && std::regex_match(it.path().filename().string(), std::regex(R"(chr\d+)")))
			dirs.push_back(it.path());
	std::sort(dirs.begin(), dirs.end());
	for (auto &d : dirs) {
		const std::string chr = d.filename().string();
		std::string CHR = chr;
		for (auto &c : CHR) c = (char)toupper((unsigned char)c);
		const std::string rel = "./data/" + chr + "/" + chr;
		bool ok = true;
		const auto codes = LoadPatternCodes(d / (chr + "_0.txt"));
		sqedit::LoadContext ctx;
		ctx.charaNo = ctx.vsCharaNo = std::atoi(chr.c_str() + 3);
		ctx.side = g_side;
		ctx.gameMode = g_gameMode;
		ctx.gameModeSub = g_gameModeSub;
		ctx.rapidBeatType = g_rapidBeat;
		ctx.patternByCode = [&](int dt, const std::string &code) {
			const auto &m = dt == 1 ? effectCodes : codes;
			const auto it = m.find(code);
			return it == m.end() ? -1 : it->second;
		};
		std::map<std::string, std::string> missing;   // move -> pattern code its template looked up and did not find
		ctx.onMissingPattern = [&](int dt, const std::string &code, const std::string &move) {
			if (!move.empty()) missing.emplace(move, (dt ? "effect:" : "") + code);
		};
		sqedit::SetLoadContext(v, &ctx);
		auto run = [&](const std::string &suffix) -> bool {
			const fs::path p = d / (chr + suffix);
			if (!fs::exists(p, ec)) return false;
			if (!RunFile(v, p, rel + suffix)) { ok = false; LOGE("sqtool: chars: {}{} FAILED", chr, suffix); }
			return true;
		};
		if (run("_sc_0.txt")) {
			if (RootSlotType(v, "Battle_Status") != "table") { ok = false; LOGE("sqtool: chars: {}_sc_0.txt does not define the Battle_Status table the engine reads", chr); }
			std::string del = "if( \"Battle_Status\" in getroottable() ) { delete Battle_Status; }";
			if (CompileBuffer(v, del, "__DeleteRootInTable")) { sq_pushroottable(v); sq_call(v, 1, SQFalse, SQTrue); sq_settop(v, 0); }
		}
		ctx.objectPushed = true;   // the loader pushes the character before LoadCommandList (0x607650)
		if (run("_cmd_0.txt")) {
			const std::string t = RootSlotType(v, CHR + "_CommandTable");
			if (t != "array") { ok = false; LOGE("sqtool: chars: {}_cmd_0.txt leaves {}_CommandTable {}: the engine reads it as an array, the character cannot act", chr, CHR, t.empty() ? "undefined" : t); }
		}
		if (run("_mv_0.txt")) {
			const std::string t = RootSlotType(v, CHR + "_MoveTable");
			if (t != "table") { ok = false; LOGE("sqtool: chars: {}_mv_0.txt leaves {}_MoveTable {}", chr, CHR, t.empty() ? "undefined" : t); }
		}
		run("_com_0.txt");
		sqedit::SetLoadContext(v, nullptr);
		// Only moves this character's command table can start (UpdateTable.name) matter: Battle_Std builds template moves for
		// every name, and a template whose pattern is absent keeps the current pattern (SetPattern(-1) -> Object_SetPatternFrame
		// pattern -1 = keep, frame 0).
		const std::set<std::string> reachable = CommandMoveNames(v, CHR + "_CommandTable");
		LOGI("sqtool: chars: {}: {} moves startable by commands, {} template pattern lookups missed", chr, reachable.size(), missing.size());
		for (const auto &[move, code] : missing) {
			if (!reachable.count(move)) continue;
			const bool eff = code.rfind("effect:", 0) == 0;
			LOGW("sqtool: chars: {}: {} (a command starts it) looks up pattern \"{}\", which is not in the {} HA6: entering it keeps the "
				"current pattern and restarts it at frame 0", chr, move, eff ? code.substr(7) : code, eff ? "effect" : "character's");
		}
		// The developers' own load warnings (btl_debError_CharacterMake), silenced in the release build.
		if (CompileBuffer(v, "if (\"__hc_devwarn\" in getroottable()) { foreach (w in ::__hc_devwarn) print(\"__HCWARN \" + w + \"\\n\"); ::__hc_devwarn.clear(); }", "__hc")) {
			sq_pushroottable(v); sq_call(v, 1, SQFalse, SQTrue); sq_settop(v, 0);
		}
		(ok ? clean : bad)++;
	}
	LOGI("sqtool: chars: {} character folders load clean, {} with errors", clean, bad);
	return bad == 0;
}

static int CmdCheck(const fs::path &root, const std::string &apiPath, bool run, bool chars, bool calls, const std::string &reportPath)
{
	if (!fs::exists(root)) {
		LOGW("sqtool: {} not found, skipping", root.string());
		return 77;
	}
	g_gameRoot = root;
	json api;
	bool haveApi = false;
	if (!apiPath.empty()) {
		std::ifstream f(apiPath);
		if (f) {
			api = json::parse(f, nullptr, false);
			haveApi = !api.is_discarded();
		}
		if (!haveApi)
			LOGW("sqtool: cannot load API table {}", apiPath);
	}

	std::vector<fs::path> files;
	for (const char *sub : {"script", "data"}) {
		fs::path d = root / sub;
		if (!fs::exists(d))
			continue;
		for (auto &it : fs::recursive_directory_iterator(d))
			if (it.is_regular_file() && it.path().extension() == ".txt")
				files.push_back(fs::relative(it.path(), root));
	}
	std::sort(files.begin(), files.end());

	HSQUIRRELVM v = OpenGameVM(nullptr, nullptr);
	std::vector<Result> results;
	int sqOk = 0, sqFail = 0, otherOk = 0, otherFail = 0;
	size_t sqBytes = 0;
	for (auto &rel : files) {
		Result r;
		r.rel = rel.generic_string();
		r.squirrel = IsSquirrelScript(rel);
		std::string src;
		ReadFile(root / rel, src);
		r.bytes = src.size();
		r.ok = CompileBuffer(v, src, r.rel);
		if (r.ok)
			sq_pop(v, 1);
		r.err = g_lastCompileError;
		if (r.squirrel) {
			(r.ok ? sqOk : sqFail)++;
			sqBytes += r.bytes;
			if (!r.ok)
				LOGE("FAIL {}:{}:{} {}", r.rel, r.err.line, r.err.column, Printable(r.err.desc));
		} else
			(r.ok ? otherOk : otherFail)++;
		results.push_back(r);
	}
	sq_close(v);

	LOGI("sqtool: Squirrel 2.2.4 (SQ_FB_COMPAT: int {} bytes, float {} bytes, char {} byte)", (int)sizeof(SQInteger),
		(int)sizeof(SQFloat), (int)sizeof(SQChar));
	LOGI("sqtool: {} squirrel scripts: {} compiled, {} failed ({} bytes)", sqOk + sqFail, sqOk, sqFail, sqBytes);
	LOGI("sqtool: {} other .txt (not loaded by the Squirrel VM): {} happen to compile, {} do not", otherOk + otherFail,
		otherOk, otherFail);

	bool runOk = true;
	if (run) {
		sqedit::ApiInstallStats st;
		HSQUIRRELVM rv = OpenGameVM(haveApi ? &api : nullptr, &st);
		LOGI("sqtool: run: stubs {} tables, {} natives, {} consts, {} classes ({} fields, {} methods), {} entries skipped", st.tables,
			st.natives, st.consts, st.classes, st.fields, st.methods, st.skipped);
		// Battle VM setup (MBTL Sq_InitBattleVM 0x7F4710): Battle_Init, Battle_Std, then data/_combase.txt (COM classes).
		for (const char *entry : {"script/Battle_Init.txt", "script/Battle_Std.txt", "data/_combase.txt"}) {
			if (!fs::exists(root / entry))
				continue;
			if (!chars && std::string(entry) == "data/_combase.txt")
				continue;
			bool ok = RunFile(rv, root / entry, std::string("./") + entry);
			LOGI("sqtool: run {} -> {}", entry, ok ? "ok" : "FAILED");
			runOk = runOk && ok;
		}
		if (chars && runOk) {
			// Observe the developers' character-table warnings (the release build prints nothing): record, then call the original.
			const char *hook = "if (\"btl_debError_CharacterMake\" in getroottable()) { ::__hc_devwarn <- []; local o = ::btl_debError_CharacterMake;"
				" ::btl_debError_CharacterMake <- function(s) : (o) { ::__hc_devwarn.append(s.tostring()); o(s); } }";
			if (CompileBuffer(rv, hook, "__hc_hook")) { sq_pushroottable(rv); sq_call(rv, 1, SQFalse, SQTrue); sq_settop(rv, 0); }
			runOk = RunCharacters(rv, root);
		}
		std::vector<std::pair<int, std::string>> top;
		for (auto &kv : g_stubCalls)
			top.push_back({kv.second, kv.first});
		std::sort(top.rbegin(), top.rend());
		for (size_t i = 0; i < top.size() && (calls || i < 8); ++i)
			LOGI("sqtool: run: native {} called {}x", top[i].second, top[i].first);
		sq_close(rv);
	}

	if (!reportPath.empty()) {
		std::ofstream o(reportPath, std::ios::binary);
		o << "file\tkind\tbytes\tresult\tline\tcol\terror\n";
		for (auto &r : results)
			o << r.rel << '\t' << (r.squirrel ? "squirrel" : "other") << '\t' << r.bytes << '\t' << (r.ok ? "ok" : "fail")
			  << '\t' << r.err.line << '\t' << r.err.column << '\t' << Printable(r.err.desc) << '\n';
		LOGI("sqtool: report written to {}", reportPath);
	}
	return (sqFail == 0 && sqOk > 0 && runOk) ? 0 : 1;
}

static int CmdCompile(const std::vector<std::string> &paths)
{
	HSQUIRRELVM v = OpenGameVM(nullptr, nullptr);
	int fails = 0;
	for (auto &p : paths) {
		std::string src;
		if (!ReadFile(p, src)) {
			LOGE("cannot read {}", p);
			++fails;
			continue;
		}
		if (CompileBuffer(v, src, p)) {
			sq_pop(v, 1);
			LOGI("ok   {} ({} bytes)", p, src.size());
		} else {
			++fails;
			LOGE("FAIL {}:{}:{} {}", p, g_lastCompileError.line, g_lastCompileError.column, Printable(g_lastCompileError.desc));
		}
	}
	sq_close(v);
	return fails ? 1 : 0;
}

int main(int argc, char **argv)
{
	InitLog();
	int rc = 2;
	std::vector<std::string> args(argv + 1, argv + argc);
	if (args.size() >= 2 && args[0] == "check") {
		std::string api, report;
		bool run = false, chars = false, calls = false;
		for (size_t i = 2; i < args.size(); ++i) {
			if (args[i] == "--api" && i + 1 < args.size())
				api = args[++i];
			else if (args[i] == "--report" && i + 1 < args.size())
				report = args[++i];
			else if (args[i] == "--run")
				run = true;
			else if (args[i] == "--chars")
				run = chars = true;
			else if (args[i] == "--calls")
				calls = true;
			else if (args[i] == "--side" && i + 1 < args.size())
				g_side = std::atoi(args[++i].c_str());
			else if (args[i] == "--mode" && i + 1 < args.size())
				g_gameMode = std::atoi(args[++i].c_str());
			else if (args[i] == "--training")
				g_gameMode = 3;
			else if (args[i] == "--rapidbeat" && i + 1 < args.size())
				g_rapidBeat = std::atoi(args[++i].c_str());
		}
		rc = CmdCheck(args[1], api, run, chars, calls, report);
	} else if (args.size() >= 2 && args[0] == "compile") {
		rc = CmdCompile(std::vector<std::string>(args.begin() + 1, args.end()));
	} else {
		LOGI("usage: sqtool check <gamedir> [--api <game>_api.json] [--run] [--chars] [--calls] [--side 0|1] [--mode N|--training] [--rapidbeat 0-3] [--report out.tsv]");
		LOGI("       sqtool compile <file.txt>...");
	}
	g_log->flush_log();
	quill::Backend::stop();
	return rc;
}
