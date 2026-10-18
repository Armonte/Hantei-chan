// script_big_test: the Squirrel Script panel on large files (fix/script-panel-big-files).
//
// A UNI2 pack's VectorTable.txt (438917 bytes, 14230 lines) opened to a blank editor: the TextEditor's matching-
// bracket guides submitted one line per document line of every multi-line { } pair, visible or not (~4 vertices a
// line), so the editor's draw list passed 65535 vertices and the 16-bit draw indices wrapped (the backend runs its
// ES2 path, no VtxOffset). Here the real TextEditor renders VectorTable-shaped text headless at the 10k boundary and
// beyond, and the test asserts, every frame: the line count, the visible row range, and the editor draw list's
// vertex count (bounded by the visible rows, far under the 16-bit budget). The outline must fold the table entries.
// Output goes through quill.

#include "squirrel/script_document.h"
#include "squirrel/script_outline.h"

#include <ImGuiColorTextEdit/TextEditor.h>
#include <imgui.h>

#include <quill/Backend.h>
#include <quill/Frontend.h>
#include <quill/LogMacros.h>
#include <quill/Logger.h>
#include <quill/sinks/ConsoleSink.h>

#include <cstring>
#include <string>
#include <vector>

static quill::Logger* g_log = nullptr;
static int g_fail = 0;
static int g_asserts = 0;

void CmdfileUiSmokeAssert(const char* expr, const char* file, int line)
{
	++g_asserts;
	LOG_ERROR(g_log, "ImGui assertion {} at {}:{}", expr, file, line);
}

#define CHECK(c) do { if (!(c)) { ++g_fail; LOG_ERROR(g_log, "FAIL {}:{} {}", __FILE__, __LINE__, #c); } } while (0)

// VectorTable.txt shape: consts, then `VectorSample.Tbl.append(\n{\n\tname = "...",\n\thit_stand\t= { ... },\n ... })`.
static std::string VectorTableText(size_t lines, const char* eol, size_t* entries)
{
	std::string s;
	size_t n = 0, e = 0;
	auto add = [&](const std::string& l) { if (n < lines) { if (n) s += eol; s += l; ++n; } };
	add("//vectorlist");
	add("const def_VL_PrintDebugMessage\t= 0;\t// debug");
	add("");
	static const char* fields[] = {"hit_stand", "hit_air\t", "hit_crouch", "guard_stand", "guard_air", "guard_crouch"};
	while (n < lines) {
		const bool open = n + 3 < lines;   // the last entry may be cut short; that's fine for the editor
		if (!open) { add(""); continue; }
		add("VectorSample.Tbl.append(");
		add("{");
		add("\tname = \"vec_" + std::to_string(e) + "\",");
		for (const char* f : fields) add(std::string("\t") + f + "\t= { num=\"vec_" + std::to_string(e) + "\"\t, reverse=0, novec=0 },");
		add("})");
		add("");
		++e;
	}
	*entries = e;
	return s;
}

static void Frame(TextEditor& ed, int* vtx, size_t* first, size_t* last)
{
	ImGuiIO& io = ImGui::GetIO();
	io.DeltaTime = 1.0f / 60.0f;
	ImGui::NewFrame();
	ImGui::SetNextWindowPos(ImVec2(0, 0));
	ImGui::SetNextWindowSize(ImVec2(1000, 700));
	ImGui::Begin("Script");
	ed.Render("##sqtext", ImVec2(-1, -1));
	ImGui::End();
	ImGui::Render();
	*vtx = 0;
	for (const ImDrawList* dl : ImGui::GetDrawData()->CmdLists)
		if (dl->_OwnerName && strstr(dl->_OwnerName, "##sqtext")) *vtx = dl->VtxBuffer.Size;
	*first = ed.GetFirstVisibleRow();
	*last = ed.GetLastVisibleRow();
}

static void EditorCase(size_t lines, const char* eol)
{
	size_t entries = 0;
	const std::string text = VectorTableText(lines, eol, &entries);
	// through the CP932 document, as the panel opens a file (ASCII text: the bytes are the same)
	sqedit::ScriptDocument doc;
	doc.loadBytes(text);
	CHECK(doc.lineCount() == lines);

	TextEditor ed;
	ed.SetLanguage(TextEditor::Language::C());
	ed.SetLineFoldingEnabled(true);
	ed.SetText(doc.utf8Text());
	CHECK(ed.GetLineCount() == lines);

	int maxVtx = 0;
	size_t maxRows = 0;
	const size_t targets[] = {0, lines / 2, lines - 1};
	for (size_t t : targets) {
		ed.SetCursor(TextEditor::DocPos(t, 0));
		ed.ScrollToLine(t, TextEditor::Scroll::alignMiddle);
		for (int f = 0; f < 4; ++f) {
			int vtx; size_t first, last;
			Frame(ed, &vtx, &first, &last);
			if (vtx > maxVtx) maxVtx = vtx;
			if (last - first + 1 > maxRows) maxRows = last - first + 1;
			CHECK(vtx > 0);           // something was drawn
			CHECK(last < lines);
			if (f == 3) CHECK(first <= t && t <= last);   // the target line is on screen
		}
	}
	// ~40 visible rows of ~30 glyphs: a few thousand vertices. The old path grew ~10 vertices per document line.
	CHECK(maxRows < 80);
	CHECK(maxVtx < 20000);
	CHECK(maxVtx < 65536);

	// outline: one row per table entry, folded under one header
	const auto items = sqedit::BuildOutline(ed.GetLineCount(), [&](size_t i) { return ed.GetLineText(i); });
	size_t tbl = 0;
	for (const auto& o : items) if (o.group == "VectorSample.Tbl") ++tbl;
	CHECK(tbl == entries);
	CHECK(!items.empty() && items.front().key == "vec_0");
	const auto folded = sqedit::OutlineRows(items, "", [](const std::string&, size_t) { return false; });
	CHECK(folded.size() <= 4);
	const auto unfolded = sqedit::OutlineRows(items, "", [](const std::string&, size_t) { return true; });
	CHECK(unfolded.size() == folded.size() + entries);
	const auto filtered = sqedit::OutlineRows(items, "vec_7", [](const std::string&, size_t) { return false; });
	CHECK(!filtered.empty() && filtered.size() < entries);

	LOG_INFO(g_log, "{} lines ({} eol, {} bytes, {} entries): max {} editor vertices, {} visible rows, outline {} items -> {} rows folded",
		lines, eol[1] ? "CRLF" : "LF", text.size(), entries, maxVtx, maxRows, items.size(), folded.size());
}

// A file of repeated `tbl = {` members (the reporter's outline: thousands of "tbl" rows) folds into one group.
static void RepeatedMemberCase()
{
	std::vector<std::string> l = {"Owner <- {"};
	for (int i = 0; i < 3000; ++i) { l.push_back("\ttbl = {"); l.push_back("\t\tname = \"t" + std::to_string(i) + "\","); l.push_back("\t}"); }
	l.push_back("}");
	const auto items = sqedit::BuildOutline(l.size(), [&](size_t i) { return l[i]; });
	size_t tbl = 0;
	for (const auto& o : items) if (o.group == "tbl") ++tbl;
	CHECK(tbl == 3000);
	const auto rows = sqedit::OutlineRows(items, "", [](const std::string&, size_t) { return false; });
	CHECK(rows.size() <= 3);
	CHECK(!items.empty() && items[0].name == "tbl  t0");
}

int main()
{
	quill::Backend::start();
	g_log = quill::Frontend::create_or_get_logger("script_big", quill::Frontend::create_or_get_sink<quill::ConsoleSink>("console"));

	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.DisplaySize = ImVec2(1280, 800);
	io.IniFilename = nullptr;
	io.Fonts->Build();

	for (size_t n : {9999u, 10001u, 14230u, 100000u}) EditorCase(n, "\r\n");
	EditorCase(14230, "\n");
	EditorCase(150000, "\r\n");   // ~5 MB
	RepeatedMemberCase();

	ImGui::DestroyContext();
	LOG_INFO(g_log, "{} failure(s), {} ImGui assertion(s)", g_fail, g_asserts);
	g_log->flush_log();
	return g_fail == 0 && g_asserts == 0 ? 0 : 1;
}
