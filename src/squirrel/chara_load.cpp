#include "chara_load.h"

#include <map>
#include <regex>

namespace sqedit {

namespace {

bool RunSnippet(HSQUIRRELVM v, const std::string& src, const char* name)
{
	const SQInteger top = sq_gettop(v);
	bool ok = SQ_SUCCEEDED(sq_compilebuffer(v, src.c_str(), (SQInteger)src.size(), name, SQFalse));
	if (ok) {
		sq_pushroottable(v);
		ok = SQ_SUCCEEDED(sq_call(v, 1, SQFalse, SQFalse));
	}
	sq_settop(v, top);
	return ok;
}

std::string RootSlotType(HSQUIRRELVM v, const std::string& name)
{
	const SQInteger top = sq_gettop(v);
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
	sq_settop(v, top);
	return t;
}

std::set<std::string> CommandMoveNames(HSQUIRRELVM v, const std::string& table)
{
	std::set<std::string> out;
	const SQInteger top = sq_gettop(v);
	sq_pushroottable(v);
	sq_pushstring(v, table.c_str(), -1);
	if (SQ_SUCCEEDED(sq_get(v, -2)) && sq_gettype(v, -1) == OT_ARRAY) {
		sq_pushnull(v);
		while (SQ_SUCCEEDED(sq_next(v, -2))) {
			const SQInteger entry = sq_gettop(v);
			if (sq_gettype(v, entry) == OT_TABLE) {
				sq_pushstring(v, "UpdateTable", -1);
				if (SQ_SUCCEEDED(sq_get(v, entry)) && sq_gettype(v, -1) == OT_TABLE) {
					sq_pushstring(v, "name", -1);
					const SQChar* s = nullptr;
					if (SQ_SUCCEEDED(sq_get(v, -2)) && SQ_SUCCEEDED(sq_getstring(v, -1, &s)) && s) out.insert(s);
				}
			}
			sq_settop(v, entry - 2);   // drop key/value, keep the iterator
		}
	}
	sq_settop(v, top);
	return out;
}

std::set<std::string> TableKeys(HSQUIRRELVM v, const std::string& table)
{
	std::set<std::string> out;
	const SQInteger top = sq_gettop(v);
	sq_pushroottable(v);
	sq_pushstring(v, table.c_str(), -1);
	if (SQ_SUCCEEDED(sq_get(v, -2)) && sq_gettype(v, -1) == OT_TABLE) {
		sq_pushnull(v);
		while (SQ_SUCCEEDED(sq_next(v, -2))) {
			const SQChar* k = nullptr;
			if (SQ_SUCCEEDED(sq_getstring(v, -2, &k)) && k) out.insert(k);
			sq_pop(v, 2);
		}
	}
	sq_settop(v, top);
	return out;
}

std::vector<std::string> TakeDevWarnings(HSQUIRRELVM v)
{
	std::vector<std::string> out;
	const SQInteger top = sq_gettop(v);
	sq_pushroottable(v);
	sq_pushstring(v, "__hc_devwarn", -1);
	if (SQ_SUCCEEDED(sq_get(v, -2)) && sq_gettype(v, -1) == OT_ARRAY) {
		sq_pushnull(v);
		while (SQ_SUCCEEDED(sq_next(v, -2))) {
			const SQChar* s = nullptr;
			if (SQ_SUCCEEDED(sq_getstring(v, -1, &s)) && s) {
				std::string w = s;
				while (!w.empty() && (w.front() == '\n' || w.front() == ' ')) w.erase(0, 1);
				out.push_back(w);
			}
			sq_pop(v, 2);
		}
	}
	sq_settop(v, top);
	RunSnippet(v, "if (\"__hc_devwarn\" in getroottable()) ::__hc_devwarn.clear();", "__hc_devwarn_clear");
	return out;
}

} // namespace

bool CharaLoadResult::ok() const
{
	for (const auto& i : issues)
		if (i.kind == CharaLoadIssue::RuntimeError || i.kind == CharaLoadIssue::MissingTable) return false;
	return true;
}

void InstallDevWarningObserver(HSQUIRRELVM v)
{
	RunSnippet(v,
		"if (\"btl_debError_CharacterMake\" in getroottable()) { ::__hc_devwarn <- []; local o = ::btl_debError_CharacterMake;"
		" ::btl_debError_CharacterMake <- function(s) : (o) { ::__hc_devwarn.append(s.tostring()); o(s); } }",
		"__hc_hook");
}

CharaLoadResult LoadCharacterScripts(HSQUIRRELVM v, const std::string& chrDirRel, LoadContext& ctx, const RunScriptFn& run,
	const std::function<bool(const std::string& rel)>& exists, const std::function<std::string(const std::string& rel)>& readFile)
{
	CharaLoadResult r;
	const size_t sl = chrDirRel.find_last_of('/');
	const std::string chr = sl == std::string::npos ? chrDirRel : chrDirRel.substr(sl + 1);
	std::string CHR = chr;
	for (auto& c : CHR) c = (char)toupper((unsigned char)c);
	const std::string base = chrDirRel + "/" + chr;

	struct Miss { std::string code; std::vector<SourceRange> inits; };
	std::map<std::string, Miss> missing;   // move -> what its template looked up
	auto userMissing = ctx.onMissingPattern;
	ctx.onMissingPattern = [&](int dt, const std::string& code, const std::string& move, const std::vector<SourceRange>& inits) {
		if (!move.empty()) missing.emplace(move, Miss{(dt ? "effect:" : "") + code, inits});
		if (userMissing) userMissing(dt, code, move, inits);
	};
	// Does any of the move's own Init functions call SetPattern with something other than -1? (its own pattern then wins
	// over the template's SetPattern(-1)). Returns the first such line ("" when none).
	std::map<std::string, std::vector<std::string>> fileLines;
	auto setsPattern = [&](const std::vector<SourceRange>& inits, std::string* where) {
		static const std::regex sp(R"(\bSetPattern\s*\(\s*([^)]*)\))");
		for (const auto& r : inits) {
			std::string rel = r.source.rfind("./", 0) == 0 ? r.source.substr(2) : r.source;
			auto it = fileLines.find(rel);
			if (it == fileLines.end()) {
				std::vector<std::string> lines;
				const std::string bytes = readFile(rel);
				size_t s0 = 0;
				for (size_t e; (e = bytes.find('\n', s0)) != std::string::npos; s0 = e + 1) lines.push_back(bytes.substr(s0, e - s0));
				lines.push_back(bytes.substr(s0));
				it = fileLines.emplace(rel, std::move(lines)).first;
			}
			for (int l = r.first; l >= 1 && l <= r.last && l <= (int)it->second.size(); ++l) {
				std::string line = it->second[(size_t)l - 1];
				const size_t c = line.find("//");
				if (c != std::string::npos) line.resize(c);
				std::smatch m;
				if (std::regex_search(line, m, sp)) {
					std::string arg = m[1].str();
					while (!arg.empty() && arg.back() == ' ') arg.pop_back();
					if (arg == "-1") continue;
					*where = rel + ":" + std::to_string(l) + " (" + r.slot + ")";
					return true;
				}
			}
		}
		return false;
	};
	ctx.objectPushed = false;
	SetLoadContext(v, &ctx);
	auto step = [&](const std::string& suffix) -> bool {
		const std::string rel = base + suffix;
		if (!exists(rel)) return false;
		std::string err, errFile;
		int line = 0;
		r.filesRun++;
		if (!run(rel, &err, &errFile, &line))
			r.issues.push_back({CharaLoadIssue::RuntimeError, errFile.empty() ? rel : errFile, line, "", err});
		return true;
	};
	if (step("_sc_0.txt")) {
		if (RootSlotType(v, "Battle_Status") != "table")
			r.issues.push_back({CharaLoadIssue::MissingTable, base + "_sc_0.txt", 0, "",
				"the sc file does not define the root table Battle_Status that the engine reads the character's status from"});
		RunSnippet(v, "if( \"Battle_Status\" in getroottable() ) { delete Battle_Status; }", "__DeleteRootInTable");
	}
	ctx.objectPushed = true;   // the loader pushes the character before LoadCommandList
	if (step("_cmd_0.txt")) {
		const std::string t = RootSlotType(v, CHR + "_CommandTable");
		if (t != "array")
			r.issues.push_back({CharaLoadIssue::MissingTable, base + "_cmd_0.txt", 0, "",
				CHR + "_CommandTable is " + (t.empty() ? "not defined" : t) + " after the cmd file: the engine reads it as an array, "
				"so the character has no commands and cannot act"});
	}
	if (step("_mv_0.txt")) {
		const std::string t = RootSlotType(v, CHR + "_MoveTable");
		if (t != "table")
			r.issues.push_back({CharaLoadIssue::MissingTable, base + "_mv_0.txt", 0, "",
				CHR + "_MoveTable is " + (t.empty() ? "not defined" : t) + " after the mv file: the engine has no moves for the character"});
	}
	step("_com_0.txt");
	SetLoadContext(v, nullptr);
	ctx.onMissingPattern = userMissing;

	r.commandMoves = CommandMoveNames(v, CHR + "_CommandTable");
	r.moves = TableKeys(v, CHR + "_MoveTable");
	for (const auto& [move, miss] : missing) {
		if (!r.commandMoves.count(move)) continue;   // standard-move probes and unreachable templates are by design
		std::string where;
		if (setsPattern(miss.inits, &where)) continue;   // the move's own Init sets its pattern afterwards
		const std::string& code = miss.code;
		const bool eff = code.rfind("effect:", 0) == 0;
		r.issues.push_back({CharaLoadIssue::MissingPattern, base + "_mv_0.txt", 0, move,
			move + " (a command starts it) uses pattern \"" + (eff ? code.substr(7) : code) + "\", which is not in the " +
				(eff ? "effect" : "character's") + " HA6: entering the move keeps the current pattern and restarts it at frame 0."});
	}
	for (const auto& w : TakeDevWarnings(v))
		r.issues.push_back({CharaLoadIssue::DevWarning, "", 0, "", w});
	return r;
}

} // namespace sqedit
