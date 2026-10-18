#include "squirrel/script_outline.h"

#include <regex>
#include <unordered_map>

namespace sqedit {

std::vector<OutlineItem> BuildOutline(size_t n, const std::function<std::string(size_t)>& getLine, size_t groupThreshold)
{
	std::vector<OutlineItem> outline;
	int depth = 0;
	bool inBlockComment = false;
	static const std::regex member(R"(^\s*([A-Za-z_]\w*)\s*=\s*(\{|$))");
	static const std::regex slot(R"(^\s*(?:[A-Za-z_]\w*\.)+([A-Za-z_]\w*)\s*<-)");
	static const std::regex func(R"(^\s*function\s+([A-Za-z_][\w:.]*)\s*\()");
	static const std::regex append(R"(^\s*([A-Za-z_][\w.]*)\.(?:append|add|push)\s*\()");
	static const std::regex cmd(R"(\bcommand\s*=\s*\[([^\]]*)\])");
	static const std::regex mv(R"re(\bname\s*=\s*"(Mv_\w+)")re");
	static const std::regex key(R"re(\bname\s*=\s*"([^"]*)")re");
	size_t open = SIZE_MAX;     // index, not a pointer: push_back moves the vector
	int openDepth = -1;
	std::string pendingAppend;  // `X.append(` seen at depth 0; the next `{` opens one entry of X
	std::unordered_map<std::string, size_t> appendCount;
	for (size_t i = 0; i < n; i++) {
		const std::string line = getLine(i);
		// Strip comments and string contents for brace counting.
		std::string code;
		bool str = false;
		for (size_t k = 0; k < line.size(); k++) {
			const char c = line[k];
			if (inBlockComment) { if (c == '*' && k + 1 < line.size() && line[k + 1] == '/') { inBlockComment = false; k++; } continue; }
			if (str) { if (c == '\\') k++; else if (c == '"') str = false; continue; }
			if (c == '"') { str = true; code += c; continue; }
			if (c == '/' && k + 1 < line.size() && line[k + 1] == '/') break;
			if (c == '/' && k + 1 < line.size() && line[k + 1] == '*') { inBlockComment = true; k++; continue; }
			code += c;
		}
		std::smatch m;
		const int before = depth;
		const bool inAppendEntry = open != SIZE_MAX && !outline[open].group.empty() && outline[open].depth < 0;
		if (before == 0 && std::regex_search(code, m, append)) pendingAppend = m[1].str();
		if (!pendingAppend.empty() && before == 0 && code.find('{') != std::string::npos) {
			const size_t k = appendCount[pendingAppend]++;
			OutlineItem it;
			it.name = pendingAppend + "[" + std::to_string(k) + "]";
			it.group = pendingAppend;
			it.line = i;
			it.depth = -1;   // marks an append entry while open; reset to 0 below
			outline.push_back(it);
			open = outline.size() - 1; openDepth = before;
			pendingAppend.clear();
		} else if (inAppendEntry) {
			// fields of an append entry are not rows of their own
		} else if (std::regex_search(code, m, slot)) {
			outline.push_back({m[1].str(), "", "", "", "", i, 0});
			open = outline.size() - 1; openDepth = before;
		} else if (std::regex_search(code, m, func) && before <= 2) {
			outline.push_back({m[1].str() + "()", "", "", "", "", i, before});
		} else if (before == 1 && std::regex_search(code, m, member)) {
			outline.push_back({m[1].str(), "", "", "", "", i, 0});
			open = outline.size() - 1; openDepth = before;
		}
		if (open != SIZE_MAX) {
			OutlineItem& o = outline[open];
			if (o.command.empty() && std::regex_search(line, m, cmd)) o.command = m[1].str();
			if (o.move.empty() && std::regex_search(line, m, mv)) o.move = m[1].str();
			if (o.key.empty() && std::regex_search(line, m, key)) o.key = m[1].str();
		}
		for (char c : code) depth += c == '{' ? 1 : c == '}' ? -1 : 0;
		if (open != SIZE_MAX && depth <= openDepth && i > outline[open].line) {
			if (outline[open].depth < 0) outline[open].depth = 0;
			open = SIZE_MAX;
		}
	}
	if (open != SIZE_MAX && outline[open].depth < 0) outline[open].depth = 0;
	// A name that repeats many times (a table of `tbl = {...}` entries) is one group, labelled by its key or line.
	std::unordered_map<std::string, size_t> count;
	for (const auto& o : outline) if (o.group.empty()) ++count[o.name];
	for (auto& o : outline)
		if (o.group.empty() && count[o.name] > groupThreshold) o.group = o.name;
	for (auto& o : outline)
		if (!o.key.empty() && o.key != o.move) o.name += "  " + o.key;
	return outline;
}

std::vector<OutlineRow> OutlineRows(const std::vector<OutlineItem>& items, const std::string& filter,
	const std::function<bool(const std::string&, size_t)>& isOpen)
{
	std::vector<OutlineRow> rows;
	if (!filter.empty()) {
		for (size_t i = 0; i < items.size(); i++)
			if (items[i].name.find(filter) != std::string::npos || items[i].move.find(filter) != std::string::npos) rows.push_back({i, 0, false});
		return rows;
	}
	for (size_t i = 0; i < items.size();) {
		size_t j = i + 1;
		if (!items[i].group.empty())
			while (j < items.size() && items[j].group == items[i].group) ++j;
		if (j - i > 1) {
			rows.push_back({i, j - i, false});
			if (isOpen(items[i].group, i))
				for (size_t k = i; k < j; k++) rows.push_back({k, 0, true});
		} else rows.push_back({i, 0, false});
		i = j;
	}
	return rows;
}

} // namespace sqedit
