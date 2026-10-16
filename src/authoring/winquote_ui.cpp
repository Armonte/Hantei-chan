#include "winquote_ui.h"
#include "../ui/ui_style.h"

#include "roster_mirror.h"
#include "../i18n.h"
#include "../misc.h"       // sj2utf8 / utf82sj
#include "../png_writer.h" // BrowseForFolderUtf8

#include <imgui.h>

#include <algorithm>
#include <cstdio>
#include <filesystem>

namespace fs = std::filesystem;

namespace winquote {

namespace {

const char kBreakSj[] = "\x81\xa1"; // ■

std::string OpponentLabel(int id)
{
	if (id == 255) return TXT("Anyone");
	for (const auto& r : authoring::kRosterMirror)
		if (r.chara == id) return std::to_string(id) + "  " + r.fullName;
	return std::to_string(id);
}

// CP932 "■" breaks <-> UTF-8 newlines, for the multiline editor.
std::string ToEditor(const std::string& sj)
{
	std::string u = sj2utf8(sj), out;
	const std::string br = sj2utf8(kBreakSj);
	for (size_t i = 0; i < u.size();) {
		if (u.compare(i, br.size(), br) == 0) { out += '\n'; i += br.size(); }
		else out += u[i++];
	}
	return out;
}

std::string FromEditor(const std::string& utf8, bool* lossy)
{
	std::string sj;
	size_t b = 0;
	while (true) {
		const size_t e = utf8.find('\n', b);
		const std::string part = utf8.substr(b, e == std::string::npos ? std::string::npos : e - b);
		const std::string enc = utf82sj(part);
		if (lossy && sj2utf8(enc) != part) *lossy = true;
		sj += enc;
		if (e == std::string::npos) break;
		sj += kBreakSj;
		b = e + 1;
	}
	return sj;
}

} // namespace

bool Window::openGame(const std::string& gameDirUtf8, std::string* error)
{
	fs::path d = fs::u8path(gameDirUtf8);
	std::error_code ec;
	if (fs::is_directory(d / "story" / "battle", ec)) d = d / "story" / "battle";
	m_names.clear();
	for (auto it = fs::directory_iterator(d, ec); it != fs::directory_iterator(); it.increment(ec)) {
		if (ec) break;
		std::string n = it->path().filename().u8string(), low = n;
		std::transform(low.begin(), low.end(), low.begin(), [](unsigned char c) { return (char)std::tolower(c); });
		if (low.size() > 8 && low.compare(low.size() - 8, 8, "_win.txt") == 0) m_names.push_back(n);
	}
	std::sort(m_names.begin(), m_names.end());
	if (m_names.empty()) {
		if (error) *error = std::string(TXT("No *_win.txt under")) + " " + gameDirUtf8 + " " + TXT("(the loose story\\battle folder; quotes inside the .p archives must be extracted first)");
		return false;
	}
	m_dir = d.u8string();
	m_current = -1;
	open = true;
	return true;
}

bool Window::selectFile(size_t i)
{
	if (i >= m_names.size()) return false;
	if (!m_file.load((fs::u8path(m_dir) / fs::u8path(m_names[i])).u8string())) {
		m_status = TXT("Cannot read the file.");
		m_statusError = true;
		return false;
	}
	m_current = static_cast<int>(i);
	m_status.clear();
	return true;
}

void Window::draw()
{
	if (!open) return;
	ui::FitNextWindow(980, 640);
	if (!ImGui::Begin(LBL("Win quotes (story\\battle\\*_win.txt)"), &open)) {
		ImGui::End();
		return;
	}
	if (ImGui::Button(LBL("Open game folder..."))) {
		const std::string d = BrowseForFolderUtf8(m_dir);
		std::string err;
		if (!d.empty() && !openGame(d, &err)) { m_status = err; m_statusError = true; }
	}
	ImGui::SameLine();
	ImGui::BeginDisabled(m_current < 0 || !m_file.dirty());
	const auto save = [&] {
		const bool ok = m_file.save((fs::u8path(m_dir) / fs::u8path(m_names[m_current])).u8string());
		m_status = ok ? TXT("Saved.") : TXT("Save failed.");
		m_statusError = !ok;
	};
	if (ImGui::Button(LBL("Save"))) save();
	ImGui::EndDisabled();
	ImGui::SameLine();
	ImGui::TextDisabled("%s", m_dir.empty() ? TXT("No folder open.") : m_dir.c_str());
	if (!m_status.empty()) {
		ImGui::SameLine();
		ImGui::TextColored(m_statusError ? ImVec4(1, 0.45f, 0.4f, 1) : ImVec4(0.6f, 0.9f, 0.6f, 1), "%s", m_status.c_str());
	}
	ImGui::TextDisabled("%s", TXT("The game reads these files and regenerates data\\*.wmt from them; a win-quote mod ships the _win.txt."));

	ImGui::BeginChild("##wqfiles", ImVec2(200, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX);
	for (size_t i = 0; i < m_names.size(); i++) {
		if (ImGui::Selectable(m_names[i].c_str(), static_cast<int>(i) == m_current)) {
			if (m_file.dirty() && static_cast<int>(i) != m_current) { m_status = TXT("Save or revert the open file first."); m_statusError = true; }
			else selectFile(i);
		}
	}
	ImGui::EndChild();
	ImGui::SameLine();
	ImGui::BeginChild("##wqedit", ImVec2(0, 0));
	if (m_current >= 0) {
		if (m_file.dirty()) {
			if (ImGui::Button(LBL("Revert"))) selectFile(static_cast<size_t>(m_current));
			ImGui::SameLine();
		}
		if (ImGui::Button(LBL("Add quote"))) {
			Quote q;
			q.voice = "none.adx";
			m_file.add(m_file.quotes().size(), q);
		}
		ImGui::SameLine();
		ImGui::SetNextItemWidth(200);
		char f[64];
		snprintf(f, sizeof f, "%s", m_filter.c_str());
		if (ImGui::InputTextWithHint("##wqfilter", TXT("filter"), f, sizeof f)) m_filter = f;
		size_t delAt = SIZE_MAX, setAt = SIZE_MAX;
		Quote setQ;
		if (ImGui::BeginTable("##wq", 6, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable)) {
			ImGui::TableSetupScrollFreeze(0, 1);
			ImGui::TableSetupColumn(TXT("Opponent"), ImGuiTableColumnFlags_WidthFixed, 170);
			ImGui::TableSetupColumn(TXT("Mode"), ImGuiTableColumnFlags_WidthFixed, 110);
			ImGui::TableSetupColumn(TXT("Face"), ImGuiTableColumnFlags_WidthFixed, 50);
			ImGui::TableSetupColumn(TXT("Voice"), ImGuiTableColumnFlags_WidthFixed, 140);
			ImGui::TableSetupColumn(TXT("Quote"), ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 24);
			ImGui::TableHeadersRow();
			for (size_t i = 0; i < m_file.quotes().size(); i++) {
				const Quote& q = m_file.quotes()[i];
				const std::string shown = ToEditor(q.text);
				if (!m_filter.empty() && shown.find(m_filter) == std::string::npos && OpponentLabel(q.opponent).find(m_filter) == std::string::npos)
					continue;
				ImGui::PushID(static_cast<int>(i));
				ImGui::TableNextRow();
				Quote nq = q;
				bool changed = false;
				ImGui::TableNextColumn();
				ImGui::SetNextItemWidth(-1);
				if (ImGui::BeginCombo("##opp", OpponentLabel(q.opponent).c_str())) {
					if (ImGui::Selectable(OpponentLabel(255).c_str(), q.opponent == 255)) { nq.opponent = 255; changed = true; }
					for (const auto& r : authoring::kRosterMirror)
						if (ImGui::Selectable(OpponentLabel(r.chara).c_str(), q.opponent == r.chara)) { nq.opponent = r.chara; changed = true; }
					ImGui::EndCombo();
				}
				ImGui::TableNextColumn();
				int mode = q.mode;
				ImGui::SetNextItemWidth(-1);
				const char* modes[] = {TXT("Normal"), TXT("Arcade boss")};
				if (ImGui::Combo("##mode", &mode, modes, 2)) { nq.mode = mode; changed = true; }
				ImGui::TableNextColumn();
				int face = q.face;
				ImGui::SetNextItemWidth(-1);
				ImGui::InputInt("##face", &face, 0);
				if (ImGui::IsItemDeactivatedAfterEdit() && face != q.face && face >= 0) { nq.face = face; changed = true; }
				ImGui::TableNextColumn();
				char vb[64];
				snprintf(vb, sizeof vb, "%s", q.voice.c_str());
				ImGui::SetNextItemWidth(-1);
				ImGui::InputText("##voice", vb, sizeof vb, ImGuiInputTextFlags_CharsNoBlank);
				if (ImGui::IsItemDeactivatedAfterEdit() && q.voice != vb && vb[0]) { nq.voice = vb; changed = true; }
				ImGui::TableNextColumn();
				char tb[1024];
				snprintf(tb, sizeof tb, "%s", shown.c_str());
				const int lines = 1 + static_cast<int>(std::count(shown.begin(), shown.end(), '\n'));
				ImGui::InputTextMultiline("##text", tb, sizeof tb, ImVec2(-1, ImGui::GetTextLineHeight() * (lines + 0.6f)));
				if (ImGui::IsItemDeactivatedAfterEdit() && shown != tb) {
					bool lossy = false;
					nq.text = FromEditor(tb, &lossy);
					if (lossy) { m_status = TXT("Some characters have no Shift-JIS (CP932) form and were replaced."); m_statusError = true; }
					changed = true;
				}
				const int w = File::widestLine(q.text);
				if (w > File::kMaxColumns) {
					ImGui::TextColored(ImVec4(1, 0.6f, 0.3f, 1), TXT("A line is %d columns wide; the box fits %d (27 full-width characters)."), w, File::kMaxColumns);
				}
				ImGui::TableNextColumn();
				if (ImGui::SmallButton("x")) delAt = i;
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", TXT("Delete this quote"));
				if (changed) { setAt = i; setQ = nq; }
				ImGui::PopID();
			}
			ImGui::EndTable();
		}
		if (setAt != SIZE_MAX) m_file.set(setAt, setQ);
		else if (delAt != SIZE_MAX) m_file.remove(delAt);
	} else ImGui::TextDisabled("%s", TXT("Pick a character's _win.txt."));
	ImGui::EndChild();
	if (m_current >= 0 && m_file.dirty() && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && ImGui::IsKeyDown(ImGuiMod_Ctrl) &&
		ImGui::IsKeyPressed(ImGuiKey_S, false))
		save();
	ImGui::End();
}

} // namespace winquote
