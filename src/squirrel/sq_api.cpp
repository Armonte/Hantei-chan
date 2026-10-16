#include "sq_api.h"


#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <regex>
#include <set>
#include <vector>

namespace sqedit {

// Source range of the closure at stack top ({} when it is not a script closure); sq_internals.cpp.
bool SqClosureSourceRange(HSQUIRRELVM v, SQInteger idx, std::string* source, int* first, int* last);

namespace {

using json = nlohmann::json;

bool ParseConst(const std::string& raw, SQInteger& out)
{
	// IDB text forms: "1", "0x8000", "&unk_1000000", "(_DWORD *)1", "-1"
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

// Get-or-create a table slot `name` in the table at stack top; leaves it pushed.
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

std::vector<std::string> Split(const std::string& q)
{
	std::vector<std::string> out;
	size_t s = 0;
	for (size_t e; (e = q.find('.', s)) != std::string::npos; s = e + 1) out.push_back(q.substr(s, e - s));
	out.push_back(q.substr(s));
	return out;
}

void PushFieldDefault(HSQUIRRELVM v, const std::string& type)
{
	if (type == "string") sq_pushstring(v, "", 0);
	else if (type == "float") sq_pushfloat(v, 0.0f);
	else if (type == "bool") sq_pushbool(v, SQFalse);
	else sq_pushinteger(v, 0);
}

// Pushes a stub closure for native `tag`; a class return carries the class object as a second free variable.
void PushStub(HSQUIRRELVM v, SQFUNCTION stub, const std::string& tag, const HSQOBJECT* cls)
{
	// sq_newclosure (2.2.4) pops the free variables top first and a call pushes them in that stored order, so the value
	// pushed LAST here ends up BELOW the one pushed first: push the tag first so it is the top slot at call time.
	sq_pushstring(v, tag.c_str(), -1);
	if (cls) sq_pushobject(v, *cls);
	sq_newclosure(v, stub, cls ? 2 : 1);
}

std::map<HSQUIRRELVM, const LoadContext*>& Contexts()
{
	static std::map<HSQUIRRELVM, const LoadContext*> m;
	return m;
}

// Reads `key` of the table at stack index idx: integer/float/string into the outputs; false when absent.
bool TableGet(HSQUIRRELVM v, SQInteger idx, const char* key, SQInteger* i, SQFloat* f, std::string* str)
{
	if (sq_gettype(v, idx) != OT_TABLE) return false;
	sq_push(v, idx);
	sq_pushstring(v, key, -1);
	bool ok = false;
	if (SQ_SUCCEEDED(sq_get(v, -2))) {
		const SQObjectType t = sq_gettype(v, -1);
		if (t == OT_INTEGER && i) { sq_getinteger(v, -1, i); if (f) *f = (SQFloat)*i; ok = true; }
		else if (t == OT_FLOAT && f) { sq_getfloat(v, -1, f); if (i) *i = (SQInteger)*f; ok = true; }
		else if (t == OT_STRING && str) { const SQChar* s = nullptr; sq_getstring(v, -1, &s); *str = s ? s : ""; ok = true; }
		else ok = t != OT_NULL;
		sq_pop(v, 1);
	}
	sq_pop(v, 1);
	return ok;
}

// A string local of the calling script function ("" when absent).
std::string CallerLocal(HSQUIRRELVM v, const char* name)
{
	for (SQUnsignedInteger level = 1; level <= 2; ++level)
		for (SQUnsignedInteger i = 0;; ++i) {
			const SQChar* n = sq_getlocal(v, level, i);
			if (!n) break;
			std::string out;
			if (std::string(n) == name && sq_gettype(v, -1) == OT_STRING) {
				const SQChar* s = nullptr;
				sq_getstring(v, -1, &s);
				out = s ? s : "";
			}
			sq_pop(v, 1);
			if (!out.empty()) return out;
		}
	return "";
}

SourceRange ClosureRange(HSQUIRRELVM v)
{
	SourceRange r;
	if (!SqClosureSourceRange(v, -1, &r.source, &r.first, &r.last)) r = {};
	return r;
}

// The move's own Init / Init_Before / Init_After: the calling frame's local table `t`, slot `move`.
std::vector<SourceRange> CallerMoveInits(HSQUIRRELVM v, const std::string& move)
{
	std::vector<SourceRange> out;
	if (move.empty()) return out;
	for (SQUnsignedInteger level = 1; level <= 2 && out.empty(); ++level)
		for (SQUnsignedInteger i = 0;; ++i) {
			const SQChar* n = sq_getlocal(v, level, i);
			if (!n) break;
			if (std::string(n) == "t" && sq_gettype(v, -1) == OT_TABLE) {
				sq_pushstring(v, move.c_str(), -1);
				if (SQ_SUCCEEDED(sq_get(v, -2)) && sq_gettype(v, -1) == OT_TABLE) {
					for (const char* slot : {"Init", "Init_Before", "Init_After"}) {
						sq_pushstring(v, slot, -1);
						if (SQ_SUCCEEDED(sq_get(v, -2))) {
							SourceRange r = ClosureRange(v);
							r.slot = slot;
							if (!r.source.empty()) out.push_back(r);
							sq_pop(v, 1);
						}
					}
					sq_pop(v, 1);
				}
			}
			sq_pop(v, 1);
			if (!out.empty()) break;
		}
	return out;
}

// Sets field `name` of the instance at stack top.
void SetField(HSQUIRRELVM v, const char* name, SQInteger val)
{
	sq_pushstring(v, name, -1);
	sq_pushinteger(v, val);
	sq_set(v, -3);
}

} // namespace

void SetLoadContext(HSQUIRRELVM v, const LoadContext* ctx)
{
	if (ctx) Contexts()[v] = ctx;
	else Contexts().erase(v);
}

SQInteger CallTracedNative(HSQUIRRELVM v, const std::string& tag)
{
	const std::string name = tag.substr(tag.find('|') + 1);
	const auto it = Contexts().find(v);
	const LoadContext* c = it == Contexts().end() ? nullptr : it->second;
	if (!c) return -1;
	// BtlMvStd.CheckHan6Data: the release exe's body is `return 0` (0x80B450).
	if (name == "BtlMvStd.CheckHan6Data") { sq_pushinteger(v, 0); return 1; }
	// BMvEff.GetPatternNum({pat, datatype=0}) 0x7FDAD0.
	if (name == "BMvEff.GetPatternNum") {
		SQInteger r = -1, dt = 0, pi = 0;
		std::string pat;
		TableGet(v, 2, "datatype", &dt, nullptr, nullptr);
		if (c->objectPushed) {
			if (TableGet(v, 2, "pat", &pi, nullptr, &pat)) {
				if (pat.empty()) r = pi;                                    // an integer pat is returned as is
				else if ((dt == 0 || dt == 1) && c->patternByCode) {
					r = c->patternByCode((int)dt, pat);
					if (r < 0 && c->onMissingPattern) {
						const std::string move = CallerLocal(v, "mvname");
						c->onMissingPattern((int)dt, pat, move, CallerMoveInits(v, move));
					}
				}
			}
		}
		sq_pushinteger(v, r);
		return 1;
	}
	if (name == "BMvCore.GetCharaNo") { sq_pushinteger(v, c->objectPushed ? c->charaNo : -1); return 1; }
	if (name == "BMvTbl.GetCharaNo") { sq_pushinteger(v, c->objectPushed ? c->charaNo : 0); return 1; }
	if (name == "BMvCore.VsScreen_GetCharaNo") { sq_pushinteger(v, c->vsCharaNo); return 1; }
	if (name == "BMvTbl.GetPlayerSide") { sq_pushinteger(v, c->objectPushed && c->side != 0 ? 1 : 0); return 1; }
	if (name == "BMvTbl.MvStageStatus.IsTrainingBattle") {
		int r = 0;
		if ((c->gameMode == 0 && c->gameModeSub == 1) || c->gameMode == 3) r = 1;
		else if (c->gameMode == 4) r = 2;
		else if (c->gameMode == 5) r = 3;
		sq_pushinteger(v, r);
		return 1;
	}
	if (name == "BMvTbl.GetRapidBeatType") {   // 0x8064C0
		SQInteger which = 0;
		sq_getinteger(v, 2, &which);
		static const int t0[4] = {0, 1, 2, 0}, t1[4] = {0, 1, 1, 1};
		int r = 0;
		if (c->objectPushed && !(c->gameMode == 4 || c->gameMode == 5) && c->rapidBeatType >= 0 && c->rapidBeatType <= 3)
			r = which == 0 ? t0[c->rapidBeatType] : which == 1 ? t1[c->rapidBeatType] : 0;
		sq_pushinteger(v, r);
		return 1;
	}
	if (name == "BMvTbl.GetMvStatus" && tag.rfind("instance:", 0) == 0) {   // 0x80A430: CharaNo = object +5
		PushStubReturn(v, tag);
		if (c->objectPushed && sq_gettype(v, -1) == OT_INSTANCE) SetField(v, "CharaNo", c->charaNo);
		return 1;
	}
	if (name == "BMvEff.GetVector_FromAngle" && tag.rfind("instance:", 0) == 0) {   // 0x7FBF60: angle in half turns
		SQFloat angle = 0;
		SQInteger speed = 100;
		TableGet(v, 2, "angle", nullptr, &angle, nullptr);
		TableGet(v, 2, "speed", &speed, nullptr, nullptr);
		PushStubReturn(v, tag);
		if (sq_gettype(v, -1) == OT_INSTANCE) {
			const float a = (float)angle * 3.14159274f;
			SetField(v, "x", (SQInteger)(std::sin(a) * (float)speed));
			SetField(v, "y", (SQInteger)(std::cos(a) * (float)-speed));
		}
		return 1;
	}
	return -1;
}

std::string StubTag(HSQUIRRELVM v)
{
	const SQChar* fv = "void|?";
	sq_getstring(v, sq_gettop(v), &fv);
	return fv;
}

SQInteger PushStubReturn(HSQUIRRELVM v, const std::string& tag)
{
	const std::string r = tag.substr(0, tag.find('|'));
	if (r.rfind("instance:", 0) == 0) {
		// the class object is the free variable below the tag
		sq_push(v, sq_gettop(v) - 1);
		if (SQ_FAILED(sq_createinstance(v, -1))) {
			sq_pop(v, 1);
			sq_pushnull(v);
			return 1;
		}
		sq_remove(v, -2);
		return 1;
	}
	if (r == "int" || r == "bool") { sq_pushinteger(v, 0); return 1; }
	if (r == "float") { sq_pushfloat(v, 0.0f); return 1; }
	if (r == "string") { sq_pushstring(v, "", 0); return 1; }
	if (r == "array") { sq_newarray(v, 0); return 1; }
	if (r == "value" || r == "object" || r == "instance") { sq_pushnull(v); return 1; }   // class not known: null, never a fake object
	return 0;
}

ApiInstallStats InstallSqratApi(HSQUIRRELVM v, const json& api, SQFUNCTION stub)
{
	ApiInstallStats st;
	// 1. Classes: created under their parent table with the exe's fields and methods.
	std::map<std::string, HSQOBJECT> classObj;   // qualified -> class
	std::set<std::string> classNames;
	for (const auto& e : api)
		if (e.value("kind", "") == "class") classNames.insert(e.value("name", ""));
	for (const auto& e : api) {
		if (e.value("kind", "") != "class") continue;
		const std::string name = e.value("name", ""), q = e.value("qualified", name);
		sq_pushroottable(v);
		const auto path = Split(q);
		for (size_t i = 0; i + 1 < path.size(); ++i) PushSubTable(v, path[i]);
		sq_pushstring(v, name.c_str(), -1);
		sq_newclass(v, SQFalse);
		for (const auto& f : api) {
			const std::string k = f.value("kind", "");
			if (k == "var" && f.value("qualified_table", "") == q) {
				sq_pushstring(v, f.value("name", "").c_str(), -1);
				PushFieldDefault(v, f.value("type", "int"));
				sq_newslot(v, -3, SQFalse);
				st.fields++;
			} else if ((k == "func" || k == "method") && f.value("table", "") == name) {
				sq_pushstring(v, f.value("name", "").c_str(), -1);
				PushStub(v, stub, f.value("ret", "void") + "|" + q + "." + f.value("name", ""), nullptr);
				sq_newslot(v, -3, SQFalse);
				st.methods++;
			}
		}
		HSQOBJECT o;
		sq_resetobject(&o);
		sq_getstackobj(v, -1, &o);
		sq_addref(v, &o);
		classObj[q] = o;
		sq_newslot(v, -3, SQFalse);
		sq_settop(v, 0);
		st.classes++;
	}
	// 2. Tables, natives and constants.
	std::map<std::string, std::string> parent;
	for (const auto& e : api)
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
	std::set<std::string> madeTables;
	for (const auto& e : api) {
		const std::string kind = e.value("kind", ""), name = e.value("name", ""), table = e.value("table", "");
		const bool constTable = table == "(consttable)";
		if (kind == "class" || kind == "table") continue;
		if (classNames.count(table)) continue;   // class members were installed with their class
		if (kind == "var" || table.empty() || (table[0] == '(' && !constTable)) { st.skipped++; continue; }
		if (constTable) sq_pushconsttable(v);
		else sq_pushroottable(v);
		std::string q;
		if (!constTable)
			for (const auto& t : pathOf(table)) { PushSubTable(v, t); q += t + "."; }
		if (madeTables.insert(q).second) st.tables++;
		if (kind == "func" || kind == "method") {
			const std::string cls = e.value("class", "");
			const auto co = classObj.find(cls);
			const std::string ret = co != classObj.end() ? "instance:" + cls : e.value("ret", "void");
			sq_pushstring(v, name.c_str(), -1);
			PushStub(v, stub, ret + "|" + q + name, co != classObj.end() ? &co->second : nullptr);
			sq_createslot(v, -3);
			st.natives++;
		} else if (kind == "const") {
			SQInteger val = 0;
			const std::string raw = e["value"].is_string() ? e["value"].get<std::string>() : "";
			if (ParseConst(raw, val)) {
				sq_pushstring(v, name.c_str(), -1);
				sq_pushinteger(v, val);
				sq_createslot(v, -3);
				// The extractor cannot yet tell Sqrat::ConstTable from a named Enumeration (both bind under e.g. "BMvConst");
				// scripts use most constants bare (_PosState_Air), some qualified (WinMesTableConst.X): install both.
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

} // namespace sqedit
