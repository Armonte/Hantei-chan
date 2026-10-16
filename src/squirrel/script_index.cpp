#include "script_index.h"

#include "../cmdfile/cmd_io.h"   // cmdfile::Cp932ToUtf8

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <regex>
#include <set>
#include <sstream>

namespace fs = std::filesystem;

namespace sqedit {

std::vector<std::string> SplitScriptLines(const std::string& text)
{
	std::vector<std::string> out;
	size_t b = 0;
	while (b <= text.size()) {
		size_t e = text.find('\n', b);
		if (e == std::string::npos) e = text.size();
		size_t ce = e;
		if (ce > b && text[ce - 1] == '\r') ce--;
		out.push_back(text.substr(b, ce - b));
		if (e == text.size()) break;
		b = e + 1;
	}
	return out;
}

namespace {

bool IsWord(char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; }

std::string Lower(std::string s)
{
	std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return s;
}

bool IsMvName(const std::string& s)
{
	if (s.size() < 4 || s.compare(0, 3, "Mv_") != 0) return false;
	for (char c : s) if (!IsWord(c)) return false;
	return true;
}

int EditDistance(const std::string& a, const std::string& b)
{
	std::vector<int> prev(b.size() + 1), cur(b.size() + 1);
	for (size_t j = 0; j <= b.size(); ++j) prev[j] = static_cast<int>(j);
	for (size_t i = 1; i <= a.size(); ++i) {
		cur[0] = static_cast<int>(i);
		for (size_t j = 1; j <= b.size(); ++j)
			cur[j] = std::min({prev[j] + 1, cur[j - 1] + 1, prev[j - 1] + (std::tolower((unsigned char)a[i - 1]) == std::tolower((unsigned char)b[j - 1]) ? 0 : 1)});
		std::swap(prev, cur);
	}
	return prev[b.size()];
}

}  // namespace

static std::string DocFor(const std::vector<std::string>& lines, int line);

void ScriptIndex::build(const std::string& rootUtf8, const std::vector<std::string>& files)
{
	m_root = rootUtf8;
	m_files = files;
	m_data.clear();
	m_fileDefs.clear();
	for (const std::string& rel : files) {
		std::ifstream f(fs::u8path(rootUtf8) / fs::u8path(rel), std::ios::binary);
		if (!f) continue;
		std::ostringstream ss;
		ss << f.rdbuf();
		FileData fd;
		fd.lines = SplitScriptLines(cmdfile::Cp932ToUtf8(ss.str()));
		std::vector<ScriptDef> defs;
		parse(rel, fd, defs);
		for (auto& d : defs) d.doc = DocFor(fd.lines, d.line);
		m_data[rel] = std::move(fd);
		m_fileDefs[rel] = std::move(defs);
	}
	rebuildDefs();
}

void ScriptIndex::updateFile(const std::string& rel, const std::string& utf8Text)
{
	FileData fd;
	fd.lines = SplitScriptLines(utf8Text);
	std::vector<ScriptDef> defs;
	parse(rel, fd, defs);
	for (auto& d : defs) d.doc = DocFor(fd.lines, d.line);
	m_data[rel] = std::move(fd);
	m_fileDefs[rel] = std::move(defs);
	if (std::find(m_files.begin(), m_files.end(), rel) == m_files.end()) m_files.push_back(rel);
	rebuildDefs();
}

void ScriptIndex::rebuildDefs()
{
	m_defs.clear();
	for (const auto& [rel, d] : m_fileDefs) m_defs.insert(m_defs.end(), d.begin(), d.end());
}

// One pass per file: blank comments and string contents (strings are recorded), then match definitions on the code view
// while tracking brace depth and the slot / table each block belongs to.
void ScriptIndex::parse(const std::string& rel, FileData& fd, std::vector<ScriptDef>& out) const
{
	fd.code.clear();
	fd.strings.clear();
	bool block = false;
	for (size_t li = 0; li < fd.lines.size(); ++li) {
		const std::string& l = fd.lines[li];
		std::string c(l.size(), ' ');
		bool str = false;
		char q = 0;
		size_t strStart = 0;
		for (size_t k = 0; k < l.size(); ++k) {
			const char ch = l[k];
			if (block) { if (ch == '*' && k + 1 < l.size() && l[k + 1] == '/') { block = false; ++k; } continue; }
			if (str) {
				if (ch == '\\') { ++k; continue; }
				if (ch == q) { str = false; c[k] = ch; fd.strings.push_back({(int)li, (int)strStart + 1, l.substr(strStart + 1, k - strStart - 1)}); }
				continue;
			}
			if (ch == '"' || ch == '\'') { str = true; q = ch; strStart = k; c[k] = ch; continue; }
			if (ch == '/' && k + 1 < l.size() && l[k + 1] == '/') break;
			if (ch == '/' && k + 1 < l.size() && l[k + 1] == '*') { block = true; ++k; continue; }
			c[k] = ch;
		}
		fd.code.push_back(std::move(c));
	}

	static const std::regex reFunc(R"(^\s*function\s+([A-Za-z_][\w.:]*)\s*\(([^)]*)\))");
	static const std::regex reMemberFunc(R"(^\s*(?:local\s+)?([A-Za-z_]\w*)\s*=\s*function\s*\(([^)]*)\))");
	static const std::regex reSlot(R"(^\s*((?:[A-Za-z_]\w*\.)*)([A-Za-z_]\w*)\s*<-)");
	static const std::regex reConst(R"(\bconst\s+([A-Za-z_]\w*)\s*=\s*([^;]+?)\s*;?\s*$)");
	static const std::regex reMember(R"(^\s*([A-Za-z_]\w*)\s*=)");
	const bool cmdFile = rel.find("_cmd") != std::string::npos;

	struct Open { std::string owner; int depth; };
	std::vector<Open> owners;   // open slots / members whose block we are inside
	int depth = 0;
	ScriptDef* cmd = nullptr;
	int cmdDepth = -1;
	for (size_t li = 0; li < fd.code.size(); ++li) {
		const std::string& c = fd.code[li];
		std::smatch m;
		const std::string owner = owners.empty() ? "" : owners.back().owner;
		std::string pendingOwner;
		if (std::regex_search(c, m, reFunc)) {
			std::string q = m[1].str();
			const size_t dot = q.find_last_of(".:");
			ScriptDef d;
			d.kind = ScriptDef::Function;
			d.name = dot == std::string::npos ? q : q.substr(dot + 1);
			d.owner = dot == std::string::npos ? owner : q.substr(0, dot);
			d.params = m[2].str();
			d.file = rel; d.line = (int)li; d.col = (int)m.position(1);
			out.push_back(d);
		} else if (std::regex_search(c, m, reMemberFunc)) {
			ScriptDef d;
			d.kind = ScriptDef::Function;
			d.name = m[1].str(); d.owner = owner; d.params = m[2].str();
			d.file = rel; d.line = (int)li; d.col = (int)m.position(1);
			out.push_back(d);
		} else if (std::regex_search(c, m, reSlot)) {
			ScriptDef d;
			std::string pre = m[1].str();
			if (!pre.empty()) pre.pop_back();
			d.name = m[2].str();
			d.owner = pre;
			d.kind = IsMvName(d.name) ? ScriptDef::MoveTable : ScriptDef::Slot;
			// Battle_Std.Name <- function(tbl={}) : a library function stored in a table slot
			static const std::regex slotFunc(R"(<-\s*function\s*\(((?:[^()]|\([^()]*\))*)\))");
			std::smatch fm;
			const std::string rest = c.substr(m.position(0) + m.length(0) - 2);
			if (d.kind == ScriptDef::Slot && std::regex_search(rest, fm, slotFunc)) {
				d.kind = ScriptDef::Function;
				d.params = fd.lines[li].substr(m.position(0) + m.length(0) - 2 + fm.position(1), fm.length(1));
			}
			d.file = rel; d.line = (int)li; d.col = (int)m.position(2);
			out.push_back(d);
			pendingOwner = d.name;
		} else if (std::regex_search(c, m, reConst)) {
			ScriptDef d;
			d.kind = ScriptDef::Const;
			d.name = m[1].str();
			d.value = fd.lines[li].substr(m.position(2), m.length(2));
			d.file = rel; d.line = (int)li; d.col = (int)m.position(1);
			out.push_back(d);
		} else if (cmdFile && depth == 1 && std::regex_search(c, m, reMember)) {
			ScriptDef d;
			d.kind = ScriptDef::CmdEntry;
			d.name = m[1].str(); d.owner = owner;
			d.file = rel; d.line = (int)li; d.col = (int)m.position(1);
			out.push_back(d);
			cmd = &out.back();
			cmdDepth = depth;
			pendingOwner = d.name;
		} else if (std::regex_search(c, m, reMember) && c.find('{') != std::string::npos) {
			pendingOwner = m[1].str();   // a nested table member: its functions belong to it
		}
		if (cmd) {
			// the Mv_ a command runs: UpdateTable = { name = "Mv_..." }
			for (const auto& s : fd.strings)
				if (s.line == (int)li && IsMvName(s.text) && cmd->value.empty() && c.find("name") != std::string::npos) cmd->value = s.text;
		}
		for (size_t k = 0; k < c.size(); ++k) {
			if (c[k] == '{') {
				++depth;
				if (!pendingOwner.empty()) { owners.push_back({pendingOwner, depth}); pendingOwner.clear(); }
			} else if (c[k] == '}') {
				while (!owners.empty() && owners.back().depth >= depth) owners.pop_back();
				--depth;
				if (cmd && depth <= cmdDepth) { cmd = nullptr; cmdDepth = -1; }
			}
		}
		// "X <-" with its "{" on the next line (the vanilla move-table layout)
		if (!pendingOwner.empty()) {
			for (size_t n = li + 1; n < fd.code.size() && n <= li + 2; ++n) {
				const std::string& nx = fd.code[n];
				const size_t first = nx.find_first_not_of(" \t");
				if (first == std::string::npos) continue;
				if (nx[first] == '{') {
					// the next line opens it: remember the owner for when depth rises there
					owners.push_back({pendingOwner, depth + 1});
				}
				break;
			}
		}
	}
}

// The // comment block right above a definition, plus the comment trailing its own line.
static std::string DocFor(const std::vector<std::string>& lines, int line)
{
	std::vector<std::string> doc;
	for (int i = line - 1; i >= 0 && i >= line - 8; --i) {
		const std::string& l = lines[i];
		const size_t f = l.find_first_not_of(" \t");
		if (f == std::string::npos || l.compare(f, 2, "//") != 0) break;
		std::string t = l.substr(f + 2);
		while (!t.empty() && (t[0] == '/' || t[0] == ' ' || t[0] == '-')) t.erase(0, 1);
		if (!t.empty()) doc.insert(doc.begin(), t);
	}
	const size_t tc = lines[line].find("//");
	if (tc != std::string::npos) {
		std::string t = lines[line].substr(tc + 2);
		while (!t.empty() && t[0] == ' ') t.erase(0, 1);
		if (!t.empty()) doc.push_back(t);
	}
	std::string out;
	for (const auto& d : doc) out += (out.empty() ? "" : "\n") + d;
	return out;
}

std::vector<const ScriptDef*> ScriptIndex::find(const std::string& name) const
{
	std::vector<const ScriptDef*> out;
	for (const auto& d : m_defs) if (d.name == name) out.push_back(&d);
	return out;
}

std::vector<const ScriptDef*> ScriptIndex::members(const std::string& owner) const
{
	std::vector<const ScriptDef*> out;
	for (const auto& d : m_defs) {
		if (d.owner == owner || (d.owner.size() > owner.size() && d.owner.compare(d.owner.size() - owner.size(), owner.size(), owner) == 0 &&
				d.owner[d.owner.size() - owner.size() - 1] == '.'))
			out.push_back(&d);
	}
	return out;
}

const std::vector<std::string>* ScriptIndex::lines(const std::string& rel) const
{
	auto it = m_data.find(rel);
	return it == m_data.end() ? nullptr : &it->second.lines;
}

std::vector<ScriptHit> ScriptIndex::search(const std::string& query, const SearchOptions& opt) const
{
	std::vector<ScriptHit> out;
	if (query.empty()) return out;
	std::regex re;
	if (opt.regex) {
		try {
			re = std::regex(opt.wholeWord ? "\\b(?:" + query + ")\\b" : query, opt.matchCase ? std::regex::ECMAScript : std::regex::ECMAScript | std::regex::icase);
		} catch (const std::regex_error&) { return out; }
	}
	const std::string needle = opt.matchCase ? query : Lower(query);
	for (const std::string& rel : m_files) {
		auto it = m_data.find(rel);
		if (it == m_data.end()) continue;
		const FileData& fd = it->second;
		for (size_t li = 0; li < fd.lines.size(); ++li) {
			// codeOnly: search the line with comments blanked but string text restored (Mv_ names live in strings)
			std::string hay = fd.lines[li];
			if (opt.codeOnly) {
				std::string c = fd.code[li];
				for (const auto& s : fd.strings) if (s.line == (int)li) c.replace(s.col, s.text.size(), s.text);
				hay = c;
			}
			if (opt.regex) {
				for (std::sregex_iterator m(hay.begin(), hay.end(), re), e; m != e; ++m) {
					if (m->length() == 0) break;
					out.push_back({rel, (int)li, (int)m->position(), (int)m->length(), fd.lines[li]});
					if (out.size() >= opt.maxHits) return out;
				}
				continue;
			}
			const std::string h = opt.matchCase ? hay : Lower(hay);
			for (size_t p = h.find(needle); p != std::string::npos; p = h.find(needle, p + 1)) {
				if (opt.wholeWord && ((p > 0 && IsWord(h[p - 1])) || (p + needle.size() < h.size() && IsWord(h[p + needle.size()])))) continue;
				out.push_back({rel, (int)li, (int)p, (int)needle.size(), fd.lines[li]});
				if (out.size() >= opt.maxHits) return out;
			}
		}
	}
	return out;
}

std::vector<ScriptHit> ScriptIndex::usages(const std::string& name) const
{
	SearchOptions o;
	o.wholeWord = true;
	o.matchCase = true;
	o.codeOnly = true;
	return search(name, o);
}

std::string ScriptIndex::nearestMove(const std::string& name, const std::string& charDir) const
{
	std::string best;
	int bestD = 1 << 30;
	for (const auto& d : m_defs) {
		if (d.kind != ScriptDef::MoveTable) continue;
		if (d.file.rfind(charDir, 0) != 0 && d.file.rfind("script/", 0) != 0) continue;
		const int e = EditDistance(name, d.name);
		if (e < bestD) { bestD = e; best = d.name; }
	}
	return bestD <= std::max(2, (int)name.size() / 4) ? best : std::string();
}

std::vector<ScriptProblem> ScriptIndex::checkMoves(const std::string& charDir) const
{
	std::vector<ScriptProblem> out;
	std::set<std::string> defined;
	std::string mvFile;
	for (const auto& d : m_defs) {
		if (d.kind != ScriptDef::MoveTable) continue;
		if (d.file.rfind(charDir + "/", 0) == 0 || d.file.rfind("script/", 0) == 0) defined.insert(d.name);
	}
	std::set<std::string> anywhere;   // move tables of every folder: objects can be spawned with another character's
	for (const auto& d : m_defs) if (d.kind == ScriptDef::MoveTable) anywhere.insert(d.name);
	for (const std::string& rel : m_files)
		if (rel.rfind(charDir + "/", 0) == 0 && rel.find("_mv") != std::string::npos && mvFile.empty()) mvFile = rel;
	for (const std::string& rel : m_files) {
		if (rel.rfind(charDir + "/", 0) != 0) continue;
		auto it = m_data.find(rel);
		if (it == m_data.end()) continue;
		std::set<std::pair<int, std::string>> seen;
		for (const auto& s : it->second.strings) {
			if (!IsMvName(s.text) || defined.count(s.text)) continue;
			// strings that are not move names: "Mv_" prefixes in TypeSE ids ("Mv_Atk_StdBandC_Init") are SE labels
			const std::string& code = it->second.code[s.line];
			const std::string before = code.substr(0, (size_t)std::max(0, s.col - 1));
			// Only references that SWITCH to a move matter (a missing one blocks the move and drops to neutral):
			//   UpdateTable = { name = "Mv_..." }, mvname = "Mv_...", ChangeMv / SetNextMoveTable / ...("Mv_...").
			// Conditions (MvCheck Only/Ignore lists, IsMatchMvNameArray, == comparisons) and SE keys never switch.
			static const std::regex transition(R"((?:\b(?:name|mvname)\s*=\s*|\b\w*(?:Mv|MoveTable)\w*\s*\(\s*)$)");
			if (!std::regex_search(before, transition)) continue;
			if (before.find("MvCheck") != std::string::npos || before.find("IsMatch") != std::string::npos ||
				before.find("CheckMoveName") != std::string::npos) continue;
			static const std::regex objSpawn(R"(\bmvname\s*=\s*$)");
			if (anywhere.count(s.text) && std::regex_search(before, objSpawn)) continue;
			if (!seen.insert({s.line, s.text}).second) continue;
			ScriptProblem p;
			p.severity = ScriptProblem::Warning;
			p.file = rel; p.line = s.line; p.col = s.col;
			const std::string near = nearestMove(s.text, charDir);
			// Traced (docs/squirrel/RE_MBTL_LOAD.md): a command's UpdateTable.name always gets a move from Battle_Std's
			// template (pattern = the name minus its prefix; missing pattern = SetPattern(-1), the current pattern keeps
			// playing); any other switch to an undefined name runs no script functions (MoveList_CallMoveFunc skips it).
			const bool cmdName = std::regex_search(before, std::regex(R"(\bname\s*=\s*$)")) && rel.find("_cmd") != std::string::npos;
			p.message = s.text + " is not defined in this character's scripts or the shared library: " +
				(cmdName ? std::string("the game builds a bare template move for it, which shows its own pattern only if the HA6 has one "
					"(otherwise the current pattern keeps playing).")
					: std::string("switching to it runs none of its script functions (Init, FrameUpdate, ...).")) +
				(near.empty() ? "" : " Did you mean " + near + "?");
			if (!mvFile.empty()) { p.fix = ScriptProblem::CreateMoveTable; p.fixArg = s.text; p.fixFile = mvFile; }
			out.push_back(p);
		}
	}
	return out;
}

std::string ScriptIndex::InsertMoveTableStub(const std::string& utf8Text, const std::string& name, int* lineOut)
{
	const bool crlf = utf8Text.find("\r\n") != std::string::npos;
	const std::string eol = crlf ? "\r\n" : "\n";
	std::vector<std::string> lines = SplitScriptLines(utf8Text);
	static const std::regex reMake(R"(MakeMoveTable\s*\(\s*([A-Za-z_]\w*))");
	std::string var = "t";
	size_t at = lines.size();
	for (size_t i = 0; i < lines.size(); ++i) {
		std::smatch m;
		const std::string code = lines[i].substr(0, lines[i].find("//"));
		if (std::regex_search(code, m, reMake)) { var = m[1].str(); at = i; break; }
	}
	// keep one blank line between the previous table and the stub, as the vanilla files do
	std::vector<std::string> block = {var + "." + name + " <-", "{", "}", ""};
	if (at == lines.size() && !lines.empty() && lines.back().empty()) at = lines.size() - 1;
	lines.insert(lines.begin() + (long)at, block.begin(), block.end());
	if (lineOut) *lineOut = (int)at;
	std::string out;
	for (size_t i = 0; i < lines.size(); ++i) {
		out += lines[i];
		if (i + 1 < lines.size()) out += eol;
	}
	return out;
}

}  // namespace sqedit
