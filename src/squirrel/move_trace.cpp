#include "move_trace.h"
#include "sq_api.h"

#include <algorithm>
#include <cstring>

namespace sqedit {

namespace {

struct Active {
	MoveTrace* trace = nullptr;
	const LoadContext* ctx = nullptr;
	std::string slot;
};
Active g_active;

bool IsPatternSetter(const std::string& q)
{
	const size_t dot = q.rfind('.');
	const std::string f = dot == std::string::npos ? q : q.substr(dot + 1);
	// JumpFrameID: a move that keeps the current pattern and jumps to a frame ID in it (templates with UpdateFrameID).
	return f == "SetPattern" || f == "SetPattern_Reserve" || f == "JumpFrameID" || f.find("VorpalPattern") != std::string::npos;
}

// Argument 2 of the native: an integer, a string code, or a table {pat=..., datatype=...}.
void ReadPatternArg(HSQUIRRELVM v, PatternSet& ps)
{
	SQInteger i = -1, dt = 0;
	const SQChar* s = nullptr;
	switch (sq_gettype(v, 2)) {
	case OT_INTEGER: sq_getinteger(v, 2, &i); ps.num = (int)i; return;
	case OT_FLOAT: { SQFloat f = 0; sq_getfloat(v, 2, &f); ps.num = (int)f; return; }
	case OT_STRING: sq_getstring(v, 2, &s); ps.code = s ? s : ""; break;
	case OT_TABLE:
		sq_pushstring(v, "datatype", -1);
		if (SQ_SUCCEEDED(sq_get(v, 2))) { sq_getinteger(v, -1, &dt); sq_pop(v, 1); }
		sq_pushstring(v, "pat", -1);
		if (SQ_SUCCEEDED(sq_get(v, 2))) {
			if (sq_gettype(v, -1) == OT_STRING) { sq_getstring(v, -1, &s); ps.code = s ? s : ""; }
			else if (sq_gettype(v, -1) == OT_INTEGER) { sq_getinteger(v, -1, &i); ps.num = (int)i; }
			sq_pop(v, 1);
		}
		break;
	default: return;
	}
	if (!ps.code.empty() && g_active.ctx && g_active.ctx->patternByCode) ps.num = g_active.ctx->patternByCode((int)dt, ps.code);
}

bool GetSlot(HSQUIRRELVM v, SQInteger idx, const char* key)
{
	sq_pushstring(v, key, -1);
	if (SQ_SUCCEEDED(sq_get(v, idx))) return true;
	return false;
}

// Stand-in for a native's `value` return (an engine object the API table has no class for: enemy data, positions): reading
// any field gives 0, push()/pop()/isdone() return 0 ("no such object"). Also stands in for BMvCore.CCharaData, whose
// IsDone field the API extraction does not list. Only used while a trace runs.
const char* kAnySrc =
	"::__hc_any <- delegate { _get = function(k) { if (k == \"push\" || k == \"pop\" || k == \"isdone\") return function(...) { return 0; }; return 0; } } : {};";

void EnsureAny(HSQUIRRELVM v)
{
	const SQInteger top = sq_gettop(v);
	if (SQ_SUCCEEDED(sq_compilebuffer(v, kAnySrc, (SQInteger)strlen(kAnySrc), "__hc_any", SQFalse))) {
		sq_pushroottable(v);
		sq_call(v, 1, SQFalse, SQFalse);
	}
	sq_settop(v, top);
}

} // namespace

bool MoveTrace_PushAny(HSQUIRRELVM v)
{
	if (!g_active.trace) return false;
	sq_pushroottable(v);
	sq_pushstring(v, "__hc_any", -1);
	if (SQ_FAILED(sq_get(v, -2))) { sq_pop(v, 1); return false; }
	sq_remove(v, -2);
	return true;
}

void MoveTrace_OnNative(HSQUIRRELVM v, const std::string& q)
{
	if (!g_active.trace || !IsPatternSetter(q)) return;
	PatternSet ps;
	ps.slot = g_active.slot;
	ps.native = q;
	if (sq_gettop(v) >= 3) ReadPatternArg(v, ps);   // 1 = this, 2 = arg, top = the stub's tag
	g_active.trace->sets.push_back(ps);
}

std::vector<MoveTrace> TraceMovePatterns(HSQUIRRELVM v, const std::string& chrUpper, LoadContext& ctx, std::string (*lastError)())
{
	std::vector<MoveTrace> out;
	const SQInteger top = sq_gettop(v);
	sq_pushroottable(v);
	if (!GetSlot(v, sq_gettop(v), (chrUpper + "_MoveTable").c_str()) || sq_gettype(v, -1) != OT_TABLE) {
		sq_settop(v, top);
		return out;
	}
	const SQInteger tbl = sq_gettop(v);
	std::vector<std::string> names;
	sq_pushnull(v);
	while (SQ_SUCCEEDED(sq_next(v, tbl))) {
		const SQChar* k = nullptr;
		if (SQ_SUCCEEDED(sq_getstring(v, -2, &k)) && k && sq_gettype(v, -1) == OT_TABLE) names.push_back(k);
		sq_pop(v, 2);
	}
	sq_pop(v, 1);
	std::sort(names.begin(), names.end());
	EnsureAny(v);
	const bool pushed = ctx.objectPushed;
	ctx.objectPushed = true;
	SetLoadContext(v, &ctx);
	g_active.ctx = &ctx;
	for (const auto& name : names) {
		MoveTrace t;
		t.move = name;
		if (!GetSlot(v, tbl, name.c_str())) continue;
		const SQInteger mv = sq_gettop(v);
		sq_pushnull(v);
		while (SQ_SUCCEEDED(sq_next(v, mv))) {
			const SQChar* k = nullptr;
			if (SQ_SUCCEEDED(sq_getstring(v, -2, &k)) && k && sq_gettype(v, -1) == OT_CLOSURE) t.slots.push_back(k);
			sq_pop(v, 2);
		}
		std::sort(t.slots.begin(), t.slots.end());
		for (const char* slot : {"Init", "FrameUpdate"}) {
			if (!GetSlot(v, mv, slot)) continue;
			if (sq_gettype(v, -1) != OT_CLOSURE) { sq_pop(v, 1); continue; }
			g_active.trace = &t;
			g_active.slot = slot;
			sq_push(v, mv);   // this = the move's table, as the engine calls it
			lastError();
			if (SQ_FAILED(sq_call(v, 1, SQFalse, SQTrue))) t.errors.push_back(std::string(slot) + ": " + lastError());
			g_active.trace = nullptr;
			sq_settop(v, mv);
		}
		sq_settop(v, tbl);
		out.push_back(std::move(t));
	}
	g_active = {};
	SetLoadContext(v, nullptr);
	ctx.objectPushed = pushed;
	sq_settop(v, top);
	return out;
}

} // namespace sqedit
