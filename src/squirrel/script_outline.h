#pragma once
// Script panel outline (P5): command entries of a _cmd_ table, t.Mv_* move blocks, functions, and the entries of
// `X.append({...})` / `X.add({...})` / `X.push({...})` tables (VectorTable). Pure text in, rows out, so it is unit-tested (squirrel_doc_test).

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

namespace sqedit {

struct OutlineItem {
	std::string name;     // what the row shows
	std::string command;  // the command = [...] list of a cmd entry
	std::string move;     // UpdateTable name (Mv_*)
	std::string key;      // the entry's name = "..." (a VectorTable entry's label)
	std::string group;    // rows with the same group collapse under one header ("VectorSample.Tbl", or a repeated name)
	size_t line = 0;
	int depth = 0;
};

// lineCount lines, line(i) returns line i (UTF-8). Names that repeat more than groupThreshold times become a group.
std::vector<OutlineItem> BuildOutline(size_t lineCount, const std::function<std::string(size_t)>& line, size_t groupThreshold = 8);

// One displayed row of the outline: a group header (item = first item of the run, count = run length) or an item.
struct OutlineRow {
	size_t item = 0;
	size_t count = 0;     // > 0: a group header for items [item, item + count)
	bool child = false;   // an item under an open group header
};
// Rows to draw: runs of the same group fold under a header (open when isOpen(group, first item) says so);
// with a filter, the matching items flat.
std::vector<OutlineRow> OutlineRows(const std::vector<OutlineItem>& items, const std::string& filter,
	const std::function<bool(const std::string& group, size_t first)>& isOpen);

} // namespace sqedit
