#include "script_panel.h"

#include "script_document.h"
#include "script_vm.h"

#include "../app_log.h"
#include "../png_writer.h" // BrowseForFolderUtf8

#include <ImGuiColorTextEdit/TextEditor.h>
#include <imgui.h>

#include <algorithm>
#include <chrono>
#include <ctime>
#include <deque>
#include <filesystem>
#include <fstream>
#include <regex>
#include <vector>

#include <windows.h>

namespace fs = std::filesystem;

namespace sqedit {

namespace {

fs::path U8(const std::string& s) { return fs::u8path(s); }
std::string ToU8(const fs::path& p) { return p.generic_u8string(); }

bool ReadBytes(const fs::path& p, std::string& out)
{
	std::ifstream f(p, std::ios::binary);
	if (!f) return false;
	out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
	return !f.bad();
}

// Temp file next to the target, then rename over it (MoveFileExW REPLACE_EXISTING in libstdc++).
bool WriteAtomic(const fs::path& p, const std::string& bytes, std::string* error)
{
	fs::path tmp = p;
	tmp += ".tmp" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
	{
		std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
		if (!f || !f.write(bytes.data(), static_cast<std::streamsize>(bytes.size()))) {
			if (error) *error = "cannot write " + ToU8(tmp);
			return false;
		}
	}
	std::error_code ec;
	fs::rename(tmp, p, ec);
	if (ec) {
		fs::remove(tmp, ec);
		if (error) *error = "cannot replace " + ToU8(p);
		return false;
	}
	return true;
}

// <dir>/.hantei-backups/<name>.<YYYYmmdd-HHMMSS>[-n].bak, never overwriting (same scheme as the cmdfile editor).
bool WriteBackup(const fs::path& target, const std::string& bytes, std::string* backup)
{
	std::error_code ec;
	const fs::path dir = target.parent_path() / ".hantei-backups";
	fs::create_directories(dir, ec);
	const std::time_t now = std::time(nullptr);
	std::tm local{};
	localtime_s(&local, &now);
	char stamp[32];
	std::strftime(stamp, sizeof stamp, "%Y%m%d-%H%M%S", &local);
	for (int n = 0; n < 1000; ++n) {
		fs::path b = dir / (target.filename().u8string() + "." + stamp + (n ? "-" + std::to_string(n) : "") + ".bak");
		if (fs::exists(b)) continue;
		std::ofstream f(b, std::ios::binary);
		if (!f || !f.write(bytes.data(), static_cast<std::streamsize>(bytes.size()))) return false;
		if (backup) *backup = ToU8(b);
		return true;
	}
	return false;
}

// Same classification as sqtool (exe loader path formats, SCOPE.md §1.4).
bool IsSquirrelScript(const std::string& relGeneric)
{
	if (relGeneric.rfind("script/", 0) == 0) return true;
	const std::string f = relGeneric.substr(relGeneric.find_last_of('/') + 1);
	static const std::regex re(R"(^(chr|npc)\d+_(mv|sc|cmd|com|com_ranking)(_\d+)?\.txt$|^(chr|npc)\d+_se_list\.txt$)"
	                           R"(|^[A-Za-z]+(_s)?_\d+_(mv|sc|cmd|com)\.txt$)",
		std::regex::icase);
	return std::regex_match(f, re);
}

// Squirrel 2.2.4 grammar for the widget: C-like, keywords from sqlexer.cpp, '#' line comments.
const TextEditor::Language* SquirrelLanguage()
{
	static TextEditor::Language lang = [] {
		TextEditor::Language l = *TextEditor::Language::C();
		l.name = "Squirrel 2.2.4";
		l.preprocess = 0;
		l.singleLineComment = "//";
		l.singleLineCommentAlt = "#";
		l.commentStart = "/*";
		l.commentEnd = "*/";
		l.hasSingleQuotedStrings = true;
		l.hasDoubleQuotedStrings = true;
		l.otherStringStart = "@\"";
		l.otherStringEnd = "\"";
		l.stringEscape = '\\';
		l.keywords.clear();
		l.declarations.clear();
		l.identifiers.clear();
		for (const char* k : {"while", "do", "if", "else", "break", "continue", "return", "null", "function", "local", "for",
				 "foreach", "in", "typeof", "delegate", "delete", "try", "catch", "throw", "clone", "yield", "resume",
				 "switch", "case", "default", "this", "parent", "class", "extends", "constructor", "instanceof", "vargc",
				 "vargv", "true", "false", "static", "enum", "const"})
			l.keywords.insert(k);
		for (const char* d : {"BMvTbl", "BMvEff", "BMvCore", "BtlMvStd", "Battle_Std", "BCMDTbl", "BCom", "BSound", "BMvConst",
				 "BMvCoreConst", "BCMDConst", "vConst", "debuglog", "__dofile__", "SQ_LoadString"})
			l.declarations.insert(d);
		for (const char* i : {"print", "assert", "array", "type", "getroottable", "getconsttable", "compilestring",
				 "newthread", "suspend", "collectgarbage", "seterrorhandler", "setdebughook", "format", "split", "strip",
				 "abs", "fabs", "sqrt", "rand", "srand", "floor", "ceil", "sin", "cos", "clock", "time", "date", "blob"})
			l.identifiers.insert(i);
		return l;
	}();
	return &lang;
}

} // namespace

std::string GuessGame(const std::string& rootUtf8)
{
	std::string low = rootUtf8;
	std::transform(low.begin(), low.end(), low.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	const fs::path r = U8(rootUtf8);
	std::error_code ec;
	if (fs::exists(r / "MBTL.exe", ec) || low.find("mbtl") != std::string::npos || low.find("melty blood type lumina") != std::string::npos) return "mbtl";
	if (fs::exists(r / "uni2.exe", ec) || low.find("uni2") != std::string::npos) return "uni2";
	return "";
}

std::string FindApiProfile(const std::string& game)
{
	if (game.empty()) return "";
	const std::string name = game + "_api.json";
	std::vector<fs::path> bases{fs::current_path()};
	wchar_t exe[1024] = {};
	if (GetModuleFileNameW(nullptr, exe, 1024)) bases.push_back(fs::path(exe).parent_path());
	for (const auto& b : bases) {
		for (fs::path d = b; !d.empty(); d = d.parent_path()) {
			for (const char* sub : {"res/squirrel", "docs/squirrel/api"}) {
				std::error_code ec;
				const fs::path p = d / sub / name;
				if (fs::exists(p, ec)) return ToU8(p);
			}
			if (d == d.parent_path()) break;
		}
	}
	return "";
}

struct ScriptPanel::Impl {
	TextEditor editor;
	ScriptDocument doc;
	ScriptVM vm;
	std::string root, game, apiPath;
	std::vector<std::string> files; // relative, generic
	std::string filter;
	std::string currentRel;
	bool dirty = false;
	bool vmReady = false;
	std::string status;
	bool statusError = false;
	bool offerSaveAnyway = false;
	bool forceNextSave = false;
	std::deque<std::string> log;

	void say(const std::string& s)
	{
		HC_LOG_INFO("[script] {}", s);
		log.push_back(s);
		while (log.size() > 200) log.pop_front();
	}

	void setStatus(const std::string& s, bool err)
	{
		status = s;
		statusError = err;
		say(s);
	}

	void showDiagnostic(const Diagnostic& d, const std::string& name)
	{
		editor.ClearMarkers();
		if (d.ok) return;
		const bool here = !d.runtime || d.source == name;
		if (here && d.line > 0) {
			const ImU32 col = d.runtime ? IM_COL32(200, 140, 0, 255) : IM_COL32(220, 50, 50, 255);
			const ImU32 bg = d.runtime ? IM_COL32(120, 80, 0, 90) : IM_COL32(140, 20, 20, 110);
			editor.AddMarker(static_cast<size_t>(d.line - 1), col, bg, d.message, d.message);
			editor.SetCursor(TextEditor::DocPos(static_cast<size_t>(d.line - 1), 0));
			editor.ScrollToLine(static_cast<size_t>(d.line - 1));
		}
	}
};

ScriptPanel::ScriptPanel() : m(std::make_unique<Impl>())
{
	m->editor.SetLanguage(SquirrelLanguage());
	m->editor.SetChangeCallback([this] { m->dirty = true; });
}

ScriptPanel::~ScriptPanel() = default;

bool ScriptPanel::hasUnsavedChanges() const { return m->dirty; }

bool ScriptPanel::openGameRoot(const std::string& rootUtf8, const std::string& profile, std::string* error)
{
	const fs::path r = U8(rootUtf8);
	std::error_code ec;
	if (!fs::exists(r / "script", ec) && !fs::exists(r / "data", ec)) {
		if (error) *error = "no script/ or data/ folder in " + rootUtf8;
		return false;
	}
	m->root = rootUtf8;
	m->game = profile.empty() ? GuessGame(rootUtf8) : profile;
	m->apiPath = FindApiProfile(m->game);
	m->files.clear();
	for (const char* sub : {"script", "data"}) {
		if (!fs::exists(r / sub, ec)) continue;
		for (auto it = fs::recursive_directory_iterator(r / sub, ec); it != fs::recursive_directory_iterator(); it.increment(ec)) {
			if (ec) break;
			if (!it->is_regular_file(ec) || it->path().extension() != ".txt") continue;
			const std::string rel = fs::relative(it->path(), r, ec).generic_u8string();
			if (IsSquirrelScript(rel)) m->files.push_back(rel);
		}
	}
	std::sort(m->files.begin(), m->files.end());
	std::string vmErr;
	m->vmReady = m->vm.open(ToU8(r), m->apiPath, [this](const std::string& s) { m->say(s); }, &vmErr);
	if (!m->vmReady) m->vm.open(ToU8(r), "", [this](const std::string& s) { m->say(s); }, &vmErr);
	m->setStatus(std::to_string(m->files.size()) + " Squirrel scripts; profile " + (m->game.empty() ? "none" : m->game) +
			(m->apiPath.empty() ? " (no API table found: compile-only)" : " (" + std::to_string(m->vm.nativeCount()) + " natives)"),
		false);
	return true;
}

bool ScriptPanel::openFile(const std::string& pathUtf8, std::string* error)
{
	std::string bytes;
	if (!ReadBytes(U8(pathUtf8), bytes)) {
		if (error) *error = "cannot read " + pathUtf8;
		return false;
	}
	m->doc.loadBytes(bytes);
	m->editor.SetText(m->doc.utf8Text());
	m->editor.ClearMarkers();
	m->dirty = false;
	m->offerSaveAnyway = false;
	std::error_code ec;
	m->currentRel = m->root.empty() ? pathUtf8 : fs::relative(U8(pathUtf8), U8(m->root), ec).generic_u8string();
	m->setStatus("opened " + m->currentRel + " (" + std::to_string(bytes.size()) + " bytes, " + std::to_string(m->doc.lineCount()) + " lines)", false);
	return true;
}

void ScriptPanel::draw()
{
	m_focused = false;
	if (!m_visible) return;
	ImGui::SetNextWindowSize(ImVec2(1000, 700), ImGuiCond_FirstUseEver);
	const std::string title = std::string("Script") + (m->dirty ? " *" : "") + (m->currentRel.empty() ? "" : " - " + m->currentRel) + "###SquirrelScript";
	if (!ImGui::Begin(title.c_str(), &m_visible)) {
		ImGui::End();
		return;
	}
	m_focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

	if (ImGui::Button("Open game folder...")) {
		const std::string dir = BrowseForFolderUtf8(m->root);
		std::string err;
		if (!dir.empty() && !openGameRoot(dir, "", &err)) m->setStatus(err, true);
	}
	ImGui::SameLine();
	ImGui::BeginDisabled(m->root.empty());
	ImGui::SetNextItemWidth(110);
	const char* games[] = {"(auto)", "uni2", "mbtl"};
	int gi = m->game == "uni2" ? 1 : m->game == "mbtl" ? 2 : 0;
	if (ImGui::Combo("Profile", &gi, games, 3)) {
		std::string err;
		openGameRoot(m->root, gi ? games[gi] : "", &err);
	}
	ImGui::EndDisabled();
	ImGui::SameLine();
	ImGui::BeginDisabled(m->currentRel.empty());
	if (ImGui::Button("Save (Ctrl+S)")) m_saveRequested = true;
	ImGui::SameLine();
	if (ImGui::Button("Check")) {
		std::string bytes;
		std::vector<EncodeError> errs;
		if (m->doc.encode(SplitLines(m->editor.GetText()), bytes, &errs)) {
			const Diagnostic d = m->vm.compile(bytes, m->currentRel);
			m->showDiagnostic(d, m->currentRel);
			m->setStatus(d.ok ? "compiles" : "line " + std::to_string(d.line) + ": " + d.message, !d.ok);
		}
	}
	ImGui::EndDisabled();
	if (m->offerSaveAnyway) {
		ImGui::SameLine();
		if (ImGui::Button("Save anyway")) { m->offerSaveAnyway = false; m->forceNextSave = true; m_saveRequested = true; }
	}
	if (!m->status.empty()) {
		ImGui::SameLine();
		ImGui::TextColored(m->statusError ? ImVec4(1, 0.45f, 0.4f, 1) : ImVec4(0.6f, 0.9f, 0.6f, 1), "%s", m->status.c_str());
	}

	const float listW = 260.0f, logH = 110.0f;
	ImGui::BeginChild("##files", ImVec2(listW, -logH), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX);
	ImGui::SetNextItemWidth(-1);
	char buf[128];
	snprintf(buf, sizeof buf, "%s", m->filter.c_str());
	if (ImGui::InputTextWithHint("##filter", "filter", buf, sizeof buf)) m->filter = buf;
	for (const auto& rel : m->files) {
		if (!m->filter.empty() && rel.find(m->filter) == std::string::npos) continue;
		if (ImGui::Selectable(rel.c_str(), rel == m->currentRel)) {
			if (m->dirty && rel != m->currentRel) m->setStatus("unsaved changes in " + m->currentRel + ": save first", true);
			else {
				std::string err;
				if (!openFile(ToU8(U8(m->root) / U8(rel)), &err)) m->setStatus(err, true);
			}
		}
	}
	ImGui::EndChild();
	ImGui::SameLine();
	ImGui::BeginChild("##edit", ImVec2(0, -logH));
	if (!m->currentRel.empty()) m->editor.Render("##sqtext", ImVec2(-1, -1));
	else ImGui::TextDisabled("Open a game folder (the one holding script/ and data/), then pick a script.");
	ImGui::EndChild();
	ImGui::BeginChild("##log", ImVec2(0, 0), ImGuiChildFlags_Borders);
	for (const auto& l : m->log) ImGui::TextUnformatted(l.c_str());
	if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) ImGui::SetScrollHereY(1.0f);
	ImGui::EndChild();

	if (m_focused && ImGui::IsKeyDown(ImGuiMod_Ctrl) && ImGui::IsKeyPressed(ImGuiKey_S, false)) m_saveRequested = true;
	ImGui::End();

	if (!m_saveRequested || m->currentRel.empty()) { m_saveRequested = false; return; }
	const bool force = m->forceNextSave;
	m->forceNextSave = false;
	m_saveRequested = false;

	// 1. CP932 encode (strict; untouched lines keep their original bytes).
	std::string bytes;
	std::vector<EncodeError> errs;
	if (!m->doc.encode(SplitLines(m->editor.GetText()), bytes, &errs)) {
		m->editor.ClearMarkers();
		for (const auto& e : errs) m->editor.AddMarker(static_cast<size_t>(e.line), IM_COL32(220, 50, 50, 255), IM_COL32(140, 20, 20, 110), e.message, e.message);
		m->setStatus("not saved: line " + std::to_string(errs.front().line + 1) + " has a character CP932 cannot represent", true);
		return;
	}
	// 2. Compile with the game's VM; with an API profile also run the top level after the bootstrap.
	Diagnostic d;
	if (!m->apiPath.empty()) {
		const Diagnostic boot = m->vm.bootstrap();
		if (!boot.ok) m->say("bootstrap: " + boot.source + ":" + std::to_string(boot.line) + " " + boot.message);
		d = m->vm.compileAndRun(bytes, "./" + m->currentRel);
		m->showDiagnostic(d, "./" + m->currentRel);
	} else {
		d = m->vm.compile(bytes, "./" + m->currentRel);
		m->showDiagnostic(d, "./" + m->currentRel);
	}
	if (!d.ok && !d.runtime && !force) {
		m->offerSaveAnyway = true;
		m->setStatus("not saved: compile error line " + std::to_string(d.line) + ": " + d.message, true);
		return;
	}
	m->offerSaveAnyway = false;
	if (bytes == m->doc.originalBytes()) {
		m->dirty = false;
		m->setStatus(d.ok ? "no changes (file is byte-identical); compiles" : "no changes; " + d.message, !d.ok);
		return;
	}
	// 3. Backup + atomic replace.
	const fs::path target = U8(m->root) / U8(m->currentRel);
	std::string backup, err;
	if (!WriteBackup(target, m->doc.originalBytes(), &backup)) { m->setStatus("not saved: cannot write backup", true); return; }
	if (!WriteAtomic(target, bytes, &err)) { m->setStatus("not saved: " + err, true); return; }
	m->doc.commit(bytes);
	m->dirty = false;
	if (d.ok) m->setStatus("saved " + m->currentRel + " (" + std::to_string(bytes.size()) + " bytes); compiles" + (m->apiPath.empty() ? "" : " and runs against the " + m->game + " stubs"), false);
	else m->setStatus("saved with " + std::string(d.runtime ? "a runtime warning" : "a COMPILE ERROR") + " (" + d.source + ":" + std::to_string(d.line) + ") " + d.message, true);
}

} // namespace sqedit
