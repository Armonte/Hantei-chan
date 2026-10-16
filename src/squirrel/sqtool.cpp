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

// Stub native: records the call and returns a default for the declared return type.
static std::map<std::string, int> g_stubCalls;
static SQInteger Native_Stub(HSQUIRRELVM v)
{
	// one free variable "ret|Qualified.Name" (the last stack slot)
	const SQChar *fv = "void|?";
	sq_getstring(v, sq_gettop(v), &fv);
	std::string tag = fv, r = tag.substr(0, tag.find('|'));
	g_stubCalls[tag.substr(tag.find('|') + 1)]++;
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
	if (r == "int" || r == "bool") {
		sq_pushinteger(v, 0);
		return 1;
	}
	if (r == "float") {
		sq_pushfloat(v, 0.0f);
		return 1;
	}
	if (r == "string") {
		sq_pushstring(v, "", 0);
		return 1;
	}
	if (r == "value" || r == "object" || r == "instance" || r == "array") {
		sq_newtable(v);
		return 1;
	}
	return 0;
}

static bool ParseConst(const std::string &raw, SQInteger &out)
{
	std::string s = raw;
	// IDB text forms: "1", "0x8000", "&unk_1000000", "(_DWORD *)1", "-1"
	s = std::regex_replace(s, std::regex(R"(\([^)]*\))"), "");
	s.erase(std::remove(s.begin(), s.end(), ' '), s.end());
	if (s.rfind("&unk_", 0) == 0)
		s = "0x" + s.substr(5);
	if (s.empty())
		return false;
	try {
		size_t used = 0;
		long long v = std::stoll(s, &used, 0);
		if (used != s.size())
			return false;
		out = (SQInteger)(int32_t)(uint32_t)v;
		return true;
	} catch (...) {
		return false;
	}
}

// Get-or-create a table slot `name` in the table at stack top; leaves it pushed.
static void PushSubTable(HSQUIRRELVM v, const std::string &name)
{
	sq_pushstring(v, name.c_str(), -1);
	if (SQ_SUCCEEDED(sq_get(v, -2)))
		return;
	sq_pushstring(v, name.c_str(), -1);
	sq_newtable(v);
	sq_createslot(v, -3);
	sq_pushstring(v, name.c_str(), -1);
	sq_get(v, -2);
}

struct StubStats {
	int tables = 0, funcs = 0, consts = 0, skipped = 0;
};

// Builds the game's native surface from <game>_api.json (generated by tools/squirrel from the IDB).
// Nested tables (a class bound into BMvTbl, ...) become nested tables; class vars are not modelled.
static StubStats InstallApi(HSQUIRRELVM v, const json &api)
{
	StubStats st;
	std::map<std::string, std::string> parent;
	for (auto &e : api)
		if (e.value("kind", "") == "table")
			parent[e.value("name", "")] = e.value("table", "");
	auto pathOf = [&](std::string t) {
		std::vector<std::string> chain;
		std::set<std::string> seen;
		while (!t.empty() && t[0] != '(' && !seen.count(t)) {
			seen.insert(t);
			chain.insert(chain.begin(), t);
			auto it = parent.find(t);
			t = it == parent.end() ? "" : it->second;
		}
		return chain;
	};
	std::set<std::string> madeTables;
	for (auto &e : api) {
		std::string kind = e.value("kind", ""), name = e.value("name", ""), table = e.value("table", "");
		bool constTable = table == "(consttable)";
		if (kind == "table" || kind == "var" || table.empty() || (table[0] == '(' && !constTable)) {
			if (kind != "table")
				st.skipped++;
			continue;
		}
		auto chain = constTable ? std::vector<std::string>{} : pathOf(table);
		// Sqrat::ConstTable entries (_DevWindows, _PosState_*, ...) are compile-time constants.
		if (constTable)
			sq_pushconsttable(v);
		else
			sq_pushroottable(v);
		std::string q;
		for (auto &t : chain) {
			PushSubTable(v, t);
			q += t + ".";
		}
		if (madeTables.insert(q).second)
			st.tables++;
		if (kind == "func" || kind == "method") {
			sq_pushstring(v, name.c_str(), -1);
			std::string ret = e.value("ret", "void");
			std::string tag = ret + "|" + q + name;
			sq_pushstring(v, tag.c_str(), -1);
			sq_newclosure(v, Native_Stub, 1);
			sq_createslot(v, -3);
			st.funcs++;
		} else if (kind == "const") {
			SQInteger val = 0;
			std::string raw = e["value"].is_string() ? e["value"].get<std::string>() : "";
			if (ParseConst(raw, val)) {
				sq_pushstring(v, name.c_str(), -1);
				sq_pushinteger(v, val);
				sq_createslot(v, -3);
				// The extractor cannot yet tell Sqrat::ConstTable from a named Enumeration (both bind under
				// e.g. "BMvConst"); scripts use most constants bare (_PosState_Air), some qualified
				// (WinMesTableConst.X). Install both ways until the profile records the container kind.
				if (!constTable) {
					sq_pushconsttable(v);
					sq_pushstring(v, name.c_str(), -1);
					sq_pushinteger(v, val);
					sq_createslot(v, -3);
					sq_pop(v, 1);
				}
				st.consts++;
			} else
				st.skipped++;
		}
		sq_settop(v, 0);
	}
	return st;
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
static HSQUIRRELVM OpenGameVM(const json *api, StubStats *st)
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
		*st = InstallApi(v, *api);
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

static int CmdCheck(const fs::path &root, const std::string &apiPath, bool run, const std::string &reportPath)
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
		StubStats st;
		HSQUIRRELVM rv = OpenGameVM(haveApi ? &api : nullptr, &st);
		LOGI("sqtool: run: stubs {} tables, {} natives, {} consts ({} entries skipped)", st.tables, st.funcs, st.consts,
			st.skipped);
		for (const char *entry : {"script/Battle_Init.txt", "script/Battle_Std.txt"}) {
			if (!fs::exists(root / entry))
				continue;
			bool ok = RunFile(rv, root / entry, std::string("./") + entry);
			LOGI("sqtool: run {} -> {}", entry, ok ? "ok" : "FAILED");
			runOk = runOk && ok;
		}
		std::vector<std::pair<int, std::string>> top;
		for (auto &kv : g_stubCalls)
			top.push_back({kv.second, kv.first});
		std::sort(top.rbegin(), top.rend());
		for (size_t i = 0; i < top.size() && i < 8; ++i)
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
		bool run = false;
		for (size_t i = 2; i < args.size(); ++i) {
			if (args[i] == "--api" && i + 1 < args.size())
				api = args[++i];
			else if (args[i] == "--report" && i + 1 < args.size())
				report = args[++i];
			else if (args[i] == "--run")
				run = true;
		}
		rc = CmdCheck(args[1], api, run, report);
	} else if (args.size() >= 2 && args[0] == "compile") {
		rc = CmdCompile(std::vector<std::string>(args.begin() + 1, args.end()));
	} else {
		LOGI("usage: sqtool check <gamedir> [--api <game>_api.json] [--run] [--report out.tsv]");
		LOGI("       sqtool compile <file.txt>...");
	}
	g_log->flush_log();
	quill::Backend::stop();
	return rc;
}
