#include "script_vm.h"

#include <squirrel.h>
#include <sqstdaux.h>
#include <sqstdblob.h>
#include <sqstdmath.h>
#include <sqstdstring.h>
#include <sqstdsystem.h>

#include <json/json.hpp>

#include <algorithm>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <regex>
#include <set>
#include <vector>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace sqedit {

struct ScriptVM::Impl {
	HSQUIRRELVM v = nullptr;
	fs::path root;
	LogFn log;
	Diagnostic compileErr;
	Diagnostic runtimeErr;
	int dofileDepth = 0;
	int natives = 0;
	int blocked = 0;
	bool bootstrapped = false;
	Diagnostic bootDiag;
	// Permissive return objects (same model as sqtool): fields read 0, known native method names read as stubs.
	std::set<std::string> methodNames;
	HSQOBJECT dlgFields, dlgMethods;
	bool haveDlg = false;

	void say(const std::string& s) { if (log) log(s); }
};

namespace {

ScriptVM::Impl* Ctx(HSQUIRRELVM v) { return static_cast<ScriptVM::Impl*>(sq_getforeignptr(v)); }

bool ReadFile(const fs::path& p, std::string& out)
{
	std::ifstream f(p, std::ios::binary);
	if (!f) return false;
	out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
	return true;
}

void CompilerError(HSQUIRRELVM v, const SQChar* desc, const SQChar* source, SQInteger line, SQInteger column)
{
	auto* c = Ctx(v);
	c->compileErr.ok = false;
	c->compileErr.runtime = false;
	c->compileErr.message = desc ? desc : "";
	c->compileErr.source = source ? source : "";
	c->compileErr.line = static_cast<int>(line);
	c->compileErr.column = static_cast<int>(column);
}

void PrintFunc(HSQUIRRELVM v, const SQChar* fmt, ...)
{
	char buf[1024];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf, sizeof buf, fmt, ap);
	va_end(ap);
	Ctx(v)->say(std::string("[script] ") + buf);
}

SQInteger RuntimeErrorHandler(HSQUIRRELVM v)
{
	auto* c = Ctx(v);
	const SQChar* err = "?";
	if (sq_gettop(v) >= 2) sq_getstring(v, 2, &err);
	c->runtimeErr.ok = false;
	c->runtimeErr.runtime = true;
	c->runtimeErr.message = err ? err : "?";
	c->runtimeErr.line = 0;
	SQStackInfos si;
	if (SQ_SUCCEEDED(sq_stackinfos(v, 1, &si))) {
		c->runtimeErr.source = si.source ? si.source : "";
		c->runtimeErr.line = static_cast<int>(si.line);
		if (si.funcname) c->runtimeErr.message += std::string(" (in ") + si.funcname + ")";
	}
	return 0;
}

// Resolves a script path below the game root; refuses anything that escapes it.
bool ResolveInRoot(const fs::path& root, const std::string& rel, fs::path& out)
{
	fs::path p = (root / fs::path(rel)).lexically_normal();
	auto r = root.lexically_normal().generic_string();
	if (p.generic_string().rfind(r, 0) != 0) return false;
	out = p;
	return true;
}

Diagnostic CompileIn(HSQUIRRELVM v, const std::string& src, const std::string& name)
{
	auto* c = Ctx(v);
	c->compileErr = {};
	Diagnostic d;
	if (SQ_FAILED(sq_compilebuffer(v, src.data(), static_cast<SQInteger>(src.size()), name.c_str(), SQTrue))) {
		d = c->compileErr;
		d.ok = false;
		if (d.message.empty()) d.message = "compile failed";
	}
	return d;
}

Diagnostic RunCompiled(HSQUIRRELVM v)
{
	auto* c = Ctx(v);
	c->runtimeErr = {};
	sq_pushroottable(v);
	const bool ok = SQ_SUCCEEDED(sq_call(v, 1, SQFalse, SQTrue));
	sq_pop(v, 1); // closure
	Diagnostic d;
	if (!ok) {
		d = c->runtimeErr;
		d.ok = false;
		d.runtime = true;
		if (d.message.empty()) d.message = "runtime error";
	}
	return d;
}

Diagnostic RunFile(HSQUIRRELVM v, const fs::path& p, const std::string& name)
{
	std::string src;
	if (!ReadFile(p, src)) {
		Diagnostic d;
		d.ok = false;
		d.message = "cannot read " + p.string();
		d.source = name;
		return d;
	}
	Diagnostic d = CompileIn(v, src, name);
	if (!d.ok) return d;
	return RunCompiled(v);
}

SQInteger Native_dofile(HSQUIRRELVM v)
{
	auto* c = Ctx(v);
	const SQChar* path = nullptr;
	if (SQ_FAILED(sq_getstring(v, 2, &path))) return sq_throwerror(v, "__dofile__: string expected");
	fs::path p;
	if (!ResolveInRoot(c->root, path, p)) return sq_throwerror(v, "__dofile__: path outside the game root");
	if (c->dofileDepth > 32) return sq_throwerror(v, "__dofile__: recursion too deep");
	++c->dofileDepth;
	Diagnostic d = RunFile(v, p, path);
	--c->dofileDepth;
	if (!d.ok) c->say("__dofile__ " + std::string(path) + ": " + d.message);
	sq_pushinteger(v, d.ok ? 1 : 0);
	return 1;
}

SQInteger Native_LoadString(HSQUIRRELVM v)
{
	auto* c = Ctx(v);
	const SQChar* path = nullptr;
	if (SQ_FAILED(sq_getstring(v, 2, &path))) return sq_throwerror(v, "SQ_LoadString: string expected");
	std::string s;
	fs::path p;
	if (ResolveInRoot(c->root, path, p)) ReadFile(p, s);
	sq_pushstring(v, s.c_str(), static_cast<SQInteger>(s.size()));
	return 1;
}

// Sandbox stub for io/system natives: logs the call and its string arguments, returns null.
SQInteger Native_Blocked(HSQUIRRELVM v)
{
	auto* c = Ctx(v);
	const SQChar* name = "?";
	const SQInteger top = sq_gettop(v);
	sq_getstring(v, top, &name); // free variable
	std::string msg = std::string("sandbox: blocked ") + name + "(";
	for (SQInteger i = 2; i < top; ++i) {
		const SQChar* s = nullptr;
		if (i > 2) msg += ", ";
		if (sq_gettype(v, i) == OT_STRING && SQ_SUCCEEDED(sq_getstring(v, i, &s))) msg += std::string("\"") + s + "\"";
		else msg += "<arg>";
	}
	msg += ")";
	++c->blocked;
	c->say(msg);
	sq_pushnull(v);
	return 1;
}

SQInteger Native_Stub(HSQUIRRELVM v);

SQInteger PushStubFn(HSQUIRRELVM v, const std::string& name, bool methods)
{
	// "valuem": a method of a class the API table does not list returns another such class (CCom.GetSkill -> PlusMove)
	const std::string tag = (methods ? "valuem|?." : "value|?.") + name;
	sq_pushstring(v, tag.c_str(), -1);
	sq_newclosure(v, Native_Stub, 1);
	return 1;
}
SQInteger Native_GetField(HSQUIRRELVM v)
{
	const SQChar* key = nullptr;
	if (SQ_SUCCEEDED(sq_getstring(v, 2, &key)) && Ctx(v)->methodNames.count(key)) return PushStubFn(v, key, false);
	sq_pushinteger(v, 0);
	return 1;
}
SQInteger Native_GetMethod(HSQUIRRELVM v)
{
	const SQChar* key = "";
	sq_getstring(v, 2, &key);
	return PushStubFn(v, key, true);
}
SQInteger Native_SetSlot(HSQUIRRELVM v)
{
	sq_push(v, 2);
	sq_push(v, 3);
	sq_rawset(v, 1);
	return 0;
}
void PushPermissive(HSQUIRRELVM v, bool methods)
{
	sq_newtable(v);
	auto* c = Ctx(v);
	if (!c->haveDlg) return;
	sq_pushobject(v, methods ? c->dlgMethods : c->dlgFields);
	sq_setdelegate(v, -2);
}
SQInteger Native_PermissiveCtor(HSQUIRRELVM v)
{
	PushPermissive(v, true);
	return 1;
}
void MakeDelegates(HSQUIRRELVM v)
{
	auto* c = Ctx(v);
	for (int i = 0; i < 2; ++i) {
		HSQOBJECT& o = i ? c->dlgMethods : c->dlgFields;
		sq_newtable(v);
		sq_pushstring(v, "_get", -1);
		sq_newclosure(v, i ? Native_GetMethod : Native_GetField, 0);
		sq_createslot(v, -3);
		sq_pushstring(v, "_set", -1);
		sq_newclosure(v, Native_SetSlot, 0);
		sq_createslot(v, -3);
		sq_resetobject(&o);
		sq_getstackobj(v, -1, &o);
		sq_addref(v, &o);
		sq_pop(v, 1);
	}
	c->haveDlg = true;
}

SQInteger Native_Stub(HSQUIRRELVM v)
{
	const SQChar* fv = "void|?";
	sq_getstring(v, sq_gettop(v), &fv);
	const std::string tag = fv, r = tag.substr(0, tag.find('|'));
	// File-existence probes answer truthfully (read-only, inside the game root, like __dofile__): the bootstrap branches on
	// them (DFCI Battle_Init loads btl_Define_LocalDebug.txt only when BtlMvStd.IsFileAlive says it is there).
	if (tag.size() >= 12 && tag.compare(tag.size() - 12, 12, ".IsFileAlive") == 0) {
		const SQChar* path = nullptr;
		fs::path p;
		std::error_code ec;
		const bool alive = SQ_SUCCEEDED(sq_getstring(v, 2, &path)) && ResolveInRoot(Ctx(v)->root, path, p) && fs::exists(p, ec);
		sq_pushinteger(v, alive ? 1 : 0);
		return 1;
	}
	if (r == "int" || r == "bool") { sq_pushinteger(v, 0); return 1; }
	if (r == "float") { sq_pushfloat(v, 0.0f); return 1; }
	if (r == "string") { sq_pushstring(v, "", 0); return 1; }
	if (r == "array") { sq_newarray(v, 0); return 1; }
	if (r == "value" || r == "object" || r == "instance" || r == "valuem") { PushPermissive(v, r == "valuem"); return 1; }
	return 0;
}

void SetClosure(HSQUIRRELVM v, const char* name, SQFUNCTION f, const char* freeVar)
{
	sq_pushstring(v, name, -1);
	if (freeVar) {
		sq_pushstring(v, freeVar, -1);
		sq_newclosure(v, f, 1);
	} else
		sq_newclosure(v, f, 0);
	sq_createslot(v, -3);
}

bool ParseConst(const std::string& raw, SQInteger& out)
{
	std::string s = std::regex_replace(raw, std::regex(R"(\([^)]*\))"), "");
	s.erase(std::remove(s.begin(), s.end(), ' '), s.end());
	if (s.rfind("&unk_", 0) == 0) s = "0x" + s.substr(5);
	if (s.empty()) return false;
	try {
		std::size_t used = 0;
		const long long val = std::stoll(s, &used, 0);
		if (used != s.size()) return false;
		out = static_cast<SQInteger>(static_cast<int32_t>(static_cast<uint32_t>(val)));
		return true;
	} catch (...) {
		return false;
	}
}

void PushSubTable(HSQUIRRELVM v, const std::string& name)
{
	sq_pushstring(v, name.c_str(), -1);
	if (SQ_SUCCEEDED(sq_get(v, -2))) return;
	sq_pushstring(v, name.c_str(), -1);
	sq_newtable(v);
	sq_createslot(v, -3);
	sq_pushstring(v, name.c_str(), -1);
	sq_get(v, -2);
}

// Same model as sqtool's InstallApi (SCOPE.md §2.2 "pure stubs").
int InstallApi(HSQUIRRELVM v, const json& api)
{
	int funcs = 0;
	std::map<std::string, std::string> parent;
	for (auto& e : api)
		if (e.value("kind", "") == "table") parent[e.value("name", "")] = e.value("table", "");
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
	for (auto& e : api) {
		const std::string kind = e.value("kind", ""), name = e.value("name", ""), table = e.value("table", "");
		const bool constTable = table == "(consttable)";
		if (kind == "table" || kind == "var" || table.empty() || (table[0] == '(' && !constTable)) continue;
		if (constTable) sq_pushconsttable(v);
		else sq_pushroottable(v);
		std::string q;
		if (!constTable)
			for (auto& t : pathOf(table)) { PushSubTable(v, t); q += t + "."; }
		if (kind == "func" || kind == "method") {
			const std::string tag = e.value("ret", "void") + "|" + q + name;
			SetClosure(v, name.c_str(), Native_Stub, tag.c_str());
			++funcs;
		} else if (kind == "const") {
			SQInteger val = 0;
			const std::string raw = e["value"].is_string() ? e["value"].get<std::string>() : "";
			if (ParseConst(raw, val)) {
				sq_pushstring(v, name.c_str(), -1);
				sq_pushinteger(v, val);
				sq_createslot(v, -3);
				if (!constTable) {
					sq_pushconsttable(v);
					sq_pushstring(v, name.c_str(), -1);
					sq_pushinteger(v, val);
					sq_createslot(v, -3);
					sq_pop(v, 1);
				}
			}
		}
		sq_settop(v, 0);
	}
	return funcs;
}

} // namespace

ScriptVM::ScriptVM() : m(std::make_unique<Impl>()) {}
ScriptVM::~ScriptVM() { close(); }
bool ScriptVM::isOpen() const { return m->v != nullptr; }
int ScriptVM::nativeCount() const { return m->natives; }
int ScriptVM::sandboxBlockedCount() const { return m->blocked; }

void ScriptVM::close()
{
	if (m->v) sq_close(m->v);
	m->v = nullptr;
	m->bootstrapped = false;
	m->haveDlg = false;
	m->methodNames.clear();
	m->natives = 0;
	m->blocked = 0;
}

bool ScriptVM::open(const std::string& gameRoot, const std::string& apiJsonPath, const LogFn& log, std::string* error)
{
	close();
	m->root = fs::path(gameRoot);
	m->log = log;
	json api;
	if (!apiJsonPath.empty()) {
		std::ifstream f(apiJsonPath);
		if (f) api = json::parse(f, nullptr, false);
		if (!f || api.is_discarded() || !api.is_array()) {
			if (error) *error = "cannot load API profile " + apiJsonPath;
			return false;
		}
	}
	HSQUIRRELVM v = sq_open(1024);
	m->v = v;
	sq_setforeignptr(v, m.get());
	sq_setprintfunc(v, PrintFunc);
	sq_setcompilererrorhandler(v, CompilerError);
	sq_pushroottable(v);
	sqstd_register_bloblib(v);
	sqstd_register_systemlib(v); // clock/time/date are harmless; the dangerous ones are replaced below
	sqstd_register_mathlib(v);
	sqstd_register_stringlib(v);
	// Sandbox: the game also registers iolib and the full systemlib (CSquirrel_ctor, UNI2 0x418640).
	for (const char* n : {"file", "loadfile", "writeclosuretofile", "system", "remove", "rename", "getenv"})
		SetClosure(v, n, Native_Blocked, n);
	SetClosure(v, "__dofile__", Native_dofile, nullptr);
	// UNI[st]/[cl-r] loads its defines with the stdlib dofile(".\\script\\x.txt"): same read-only loader as __dofile__
	// (inside the game root, never writes), so the bootstrap sees what the game sees.
	SetClosure(v, "dofile", Native_dofile, nullptr);
	SetClosure(v, "SQ_LoadString", Native_LoadString, nullptr);
	sq_pop(v, 1);
	sq_newclosure(v, RuntimeErrorHandler, 0);
	sq_seterrorhandler(v);
	if (api.is_array()) {
		m->natives = InstallApi(v, api);
		for (auto& e : api)
			if (e.value("kind", "") == "method" || e.value("kind", "") == "func") m->methodNames.insert(e.value("name", ""));
		MakeDelegates(v);
		// Classes the scripts construct that the API table does not list yet (UNI2's CCom AI builder).
		for (const char* cls : {"CCom"}) {
			sq_pushroottable(v);
			sq_pushstring(v, cls, -1);
			if (SQ_FAILED(sq_get(v, -2))) {
				sq_settop(v, 0);
				sq_pushroottable(v);
				SetClosure(v, cls, Native_PermissiveCtor, nullptr);
			}
			sq_settop(v, 0);
		}
	}
	return true;
}

Diagnostic ScriptVM::bootstrap()
{
	if (!m->v) { Diagnostic d; d.ok = false; d.message = "VM not open"; return d; }
	if (m->bootstrapped) return m->bootDiag;
	m->bootstrapped = true;
	m->bootDiag = {};
	for (const char* entry : {"script/Battle_Init.txt", "script/Battle_Std.txt"}) {
		const fs::path p = m->root / entry;
		if (!fs::exists(p)) continue;
		Diagnostic d = RunFile(m->v, p, std::string("./") + entry);
		sq_settop(m->v, 0);
		if (!d.ok) { m->bootDiag = d; break; }
	}
	return m->bootDiag;
}

Diagnostic ScriptVM::compile(const std::string& bytes, const std::string& name)
{
	if (!m->v) { Diagnostic d; d.ok = false; d.message = "VM not open"; return d; }
	Diagnostic d = CompileIn(m->v, bytes, name);
	sq_settop(m->v, 0);
	return d;
}

Diagnostic ScriptVM::runPrerequisites(const std::string& rel)
{
	Diagnostic ok;
	if (!m->v || rel.rfind("data/", 0) != 0) return ok;
	// The game's load order inside a character folder: cmd, then mv (MakeMoveTable needs the command table), then sc;
	// COM (CPU AI) files build on data/_combase.txt.
	auto rank = [](const std::string& f) {
		if (f.find("_cmd") != std::string::npos) return 0;
		if (f.find("_mv") != std::string::npos) return 1;
		if (f.find("_sc") != std::string::npos) return 2;
		if (f.find("_com") != std::string::npos) return 3;
		return 9;
	};
	const fs::path self = m->root / fs::u8path(rel);
	const int mine = rank(self.filename().u8string());
	if (mine == 9) return ok;
	std::vector<fs::path> before;
	if (mine == 3 && fs::exists(m->root / "data/_combase.txt")) before.push_back(m->root / "data/_combase.txt");
	std::error_code ec;
	std::vector<fs::path> sib;
	for (auto it = fs::directory_iterator(self.parent_path(), ec); it != fs::directory_iterator(); it.increment(ec)) {
		if (ec) break;
		const std::string f = it->path().filename().u8string();
		if (it->path().extension() != ".txt" || it->path() == self) continue;
		const int r = rank(f);
		if (r < mine && r < 3) sib.push_back(it->path());
	}
	std::sort(sib.begin(), sib.end(), [&](const fs::path& a, const fs::path& b) {
		const int ra = rank(a.filename().u8string()), rb = rank(b.filename().u8string());
		return ra != rb ? ra < rb : a < b;
	});
	before.insert(before.end(), sib.begin(), sib.end());
	for (const auto& p : before) {
		const std::string name = "./" + p.lexically_relative(m->root).generic_u8string();
		Diagnostic d = RunFile(m->v, p, name);
		sq_settop(m->v, 0);
		if (!d.ok) return d;
	}
	return ok;
}

Diagnostic ScriptVM::compileAndRun(const std::string& bytes, const std::string& name)
{
	if (!m->v) { Diagnostic d; d.ok = false; d.message = "VM not open"; return d; }
	Diagnostic d = CompileIn(m->v, bytes, name);
	if (d.ok) d = RunCompiled(m->v);
	sq_settop(m->v, 0);
	return d;
}

} // namespace sqedit
