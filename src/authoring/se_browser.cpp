#include "se_browser.h"
#include "../ui/ui_style.h"

#include "../filedialog.h"
#include "../i18n.h"
#include "../mbaacc_pack.h"
#include "../misc.h"       // sj2utf8 / utf82sj
#include "../png_writer.h" // BrowseForFolderUtf8

#include <imgui.h>

#include <windows.h>
#include <mmsystem.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <regex>
#include <set>
#include <sstream>

namespace fs = std::filesystem;

namespace sebrowse {

namespace {

uint32_t U32(const uint8_t* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | (static_cast<uint32_t>(p[3]) << 24); }
uint16_t U16(const uint8_t* p) { return static_cast<uint16_t>(p[0] | (p[1] << 8)); }

bool ReadLoose(const fs::path& p, std::vector<uint8_t>& out)
{
	std::ifstream f(p, std::ios::binary);
	if (!f) return false;
	out.assign(std::istreambuf_iterator<char>(f), {});
	return true;
}

std::string Lower(std::string s)
{
	std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return s;
}

} // namespace

WavInfo InspectWav(const std::vector<uint8_t>& b)
{
	WavInfo w;
	if (b.size() < 12 || std::memcmp(b.data(), "RIFF", 4) || std::memcmp(b.data() + 8, "WAVE", 4)) return w;
	size_t at = 12, dataAt = 0, dataLen = 0;
	int fmt = 0, align = 0;
	while (at + 8 <= b.size()) {
		const uint32_t len = U32(&b[at + 4]);
		if (!std::memcmp(&b[at], "fmt ", 4) && at + 8 + 16 <= b.size()) {
			fmt = U16(&b[at + 8]);
			w.channels = U16(&b[at + 10]);
			w.rate = static_cast<int>(U32(&b[at + 12]));
			align = U16(&b[at + 20]);
			w.bits = U16(&b[at + 22]);
		} else if (!std::memcmp(&b[at], "data", 4)) {
			dataAt = at + 8;
			dataLen = std::min<size_t>(len, b.size() - dataAt);
		}
		at += 8 + len + (len & 1);
	}
	if (!w.rate || !align || !dataAt) return w;
	w.ok = true;
	w.seconds = static_cast<double>(dataLen / align) / w.rate;
	if (fmt == 1 && w.bits == 16) {
		int peak = 0;
		for (size_t i = dataAt; i + 1 < dataAt + dataLen; i += 2) peak = std::max(peak, std::abs(static_cast<int>(static_cast<int16_t>(U16(&b[i])))));
		w.peakDb = peak ? 20.0 * std::log10(peak / 32768.0) : -120.0;
	}
	return w;
}

bool Window::readGameFile(const std::string& rel, std::vector<uint8_t>& out, std::string* source) const
{
	if (ReadLoose(fs::u8path(m_game) / fs::u8path(rel), out)) {
		if (source) *source = TXT("loose");
		return true;
	}
	for (const auto& a : m_packs) {
		const mbpack::Entry* e = a.find(rel);
		if (!e || !a.read(*e, out, a.detectMode())) continue;
		if (source) *source = fs::path(a.path()).filename().u8string();
		return true;
	}
	if (source) source->clear();
	return false;
}

bool Window::openGame(const std::string& gameDirUtf8, std::string* error)
{
	const fs::path g = fs::u8path(gameDirUtf8);
	std::set<std::string> names;
	std::error_code ec;
	auto take = [&](const std::string& file) {
		const std::string low = Lower(file);
		if (low.size() > 11 && low.compare(low.size() - 11, 11, "_selist.txt") == 0) names.insert(low.substr(0, low.size() - 11));
	};
	for (auto it = fs::directory_iterator(g / "data", ec); it != fs::directory_iterator(); it.increment(ec)) {
		if (ec) break;
		take(it->path().filename().u8string());
	}
	std::vector<mbpack::Archive> packs;
	auto files = mbpack::FindPacks(g.wstring());
	std::sort(files.rbegin(), files.rend());
	for (const auto& p : files) {
		mbpack::Archive a;
		std::string err;
		if (!a.open(p, err)) continue;
		for (const auto& e : a.entries())
			if (Lower(e.relativePath()).rfind("data/", 0) == 0) take(e.name);
		packs.push_back(std::move(a));
	}
	if (names.empty()) {
		if (error) *error = std::string(TXT("No *_SeList.txt (loose or packed) in")) + " " + gameDirUtf8;
		return false;
	}
	m_game = gameDirUtf8;
	m_packs = std::move(packs);
	if (m_outDir.empty()) m_outDir = gameDirUtf8;
	m_chars.assign(names.begin(), names.end());
	m_current = -1;
	m_slots.clear();
	open = true;
	return true;
}

bool Window::selectCharacter(size_t i)
{
	if (i >= m_chars.size()) return false;
	std::vector<uint8_t> bytes;
	if (!readGameFile("data/" + m_chars[i] + "_SeList.txt", bytes, &m_listSource)) {
		m_status = TXT("Cannot read the file.");
		m_statusError = true;
		return false;
	}
	m_list = bg::TextIni();
	m_list.LoadText(std::string(bytes.begin(), bytes.end()));
	m_current = static_cast<int>(i);
	rebuildSlots();
	return true;
}

void Window::rebuildSlots()
{
	m_slots.clear();
	static const std::regex slot(R"(^\s*(\d+)\s*=\s*([^\s/]+))");
	std::istringstream in(m_list.Text());
	std::string line;
	bool inList = false;
	while (std::getline(in, line)) {
		if (!line.empty() && line.back() == '\r') line.pop_back();
		const size_t ns = line.find_first_not_of(" \t");
		if (ns != std::string::npos && line[ns] == '[') { inList = Lower(line.substr(ns, 8)) == "[selist]"; continue; }
		std::smatch m;
		if (!inList || !std::regex_search(line, m, slot)) continue;
		Slot s;
		s.key = m[1].str();
		s.name = m[2].str();
		std::string c;
		if (m_list.GetComment("SeList", s.key, c)) s.comment = sj2utf8(c);
		std::vector<uint8_t> wav;
		if (readGameFile("se/battle_se/" + s.name + ".wav", wav, &s.source)) s.info = InspectWav(wav);
		m_slots.push_back(std::move(s));
	}
}

void Window::play(const Slot& s)
{
	std::vector<uint8_t> wav;
	std::string src;
	PlaySoundW(nullptr, nullptr, 0);
	if (!readGameFile("se/battle_se/" + s.name + ".wav", wav, &src)) { m_status = TXT("No such sound file."); m_statusError = true; return; }
	m_playing = std::move(wav);
	PlaySoundW(reinterpret_cast<LPCWSTR>(m_playing.data()), nullptr, SND_MEMORY | SND_ASYNC | SND_NODEFAULT);
}

void Window::draw()
{
	if (!open) {
		if (!m_playing.empty()) { PlaySoundW(nullptr, nullptr, 0); m_playing.clear(); }
		return;
	}
	ui::FitNextWindow(920, 600);
	if (!ImGui::Begin(LBL("Sound effects (SeList / se\\battle_se)"), &open)) {
		ImGui::End();
		return;
	}
	if (ImGui::Button(LBL("Open game folder..."))) {
		const std::string d = BrowseForFolderUtf8(m_game);
		std::string err;
		if (!d.empty() && !openGame(d, &err)) { m_status = err; m_statusError = true; }
	}
	ImGui::SameLine();
	if (ImGui::Button(LBL("Output folder..."))) {
		const std::string d = BrowseForFolderUtf8(m_outDir);
		if (!d.empty()) m_outDir = d;
	}
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("%s", TXT("Edited SeLists and replaced sounds are written here as loose files (data\\, se\\battle_se\\): the game folder, or a mod project's files folder."));
	ImGui::SameLine();
	ImGui::BeginDisabled(m_current < 0 || !m_list.IsDirty());
	if (ImGui::Button(LBL("Save SeList"))) {
		const fs::path out = fs::u8path(m_outDir) / "data" / fs::u8path(m_chars[m_current] + "_SeList.txt");
		std::error_code ec;
		fs::create_directories(out.parent_path(), ec);
		const bool ok = m_list.Save(out.string());
		m_status = ok ? std::string(TXT("Saved:")) + " " + out.u8string() : TXT("Save failed.");
		m_statusError = !ok;
	}
	ImGui::EndDisabled();
	ImGui::TextDisabled("%s %s | %s %s", TXT("game:"), m_game.empty() ? "-" : m_game.c_str(), TXT("output:"), m_outDir.empty() ? "-" : m_outDir.c_str());
	if (!m_status.empty()) ImGui::TextColored(m_statusError ? ImVec4(1, 0.45f, 0.4f, 1) : ImVec4(0.6f, 0.9f, 0.6f, 1), "%s", m_status.c_str());

	ImGui::BeginChild("##sechars", ImVec2(ui::Px(160), 0), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX);
	for (size_t i = 0; i < m_chars.size(); i++) {
		if (ImGui::Selectable(m_chars[i].c_str(), static_cast<int>(i) == m_current)) {
			if (m_list.IsDirty() && static_cast<int>(i) != m_current) { m_status = TXT("Save or revert the open file first."); m_statusError = true; }
			else selectCharacter(i);
		}
	}
	ImGui::EndChild();
	ImGui::SameLine();
	ImGui::BeginChild("##seslots", ImVec2(0, 0));
	if (m_current >= 0) {
		ImGui::TextDisabled("%s_SeList.txt (%s), %zu %s", m_chars[m_current].c_str(), m_listSource.c_str(), m_slots.size(), TXT("slots"));
		ImGui::SameLine();
		if (m_list.IsDirty() && ImGui::SmallButton(LBL("Revert"))) selectCharacter(static_cast<size_t>(m_current));
		ImGui::SameLine();
		ImGui::SetNextItemWidth(ui::Px(200));
		char f[64];
		snprintf(f, sizeof f, "%s", m_filter.c_str());
		if (ImGui::InputTextWithHint("##sefilter", TXT("filter"), f, sizeof f)) m_filter = f;
		std::string setKey, setName, setComment;
		bool doName = false, doComment = false;
		if (ImGui::BeginTable("##se", 7, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable)) {
			ImGui::TableSetupScrollFreeze(0, 1);
			ImGui::TableSetupColumn(TXT("Slot"), ImGuiTableColumnFlags_WidthFixed, 40);
			ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 70);
			ImGui::TableSetupColumn(TXT("File"), ImGuiTableColumnFlags_WidthFixed, 150);
			ImGui::TableSetupColumn(TXT("Length"), ImGuiTableColumnFlags_WidthFixed, 60);
			ImGui::TableSetupColumn(TXT("Peak"), ImGuiTableColumnFlags_WidthFixed, 70);
			ImGui::TableSetupColumn(TXT("From"), ImGuiTableColumnFlags_WidthFixed, 60);
			ImGui::TableSetupColumn(TXT("Comment"), ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableHeadersRow();
			for (size_t i = 0; i < m_slots.size(); i++) {
				const Slot& s = m_slots[i];
				if (!m_filter.empty() && s.key != m_filter && s.name.find(m_filter) == std::string::npos && s.comment.find(m_filter) == std::string::npos) continue;
				ImGui::PushID(static_cast<int>(i));
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(s.key.c_str());
				ImGui::TableNextColumn();
				ImGui::BeginDisabled(s.source.empty());
				if (ImGui::SmallButton(">")) play(s);
				ImGui::EndDisabled();
				ImGui::SameLine();
				if (ImGui::SmallButton(LBL("Replace"))) {
					const std::string src = FileDialog(fileType::WAV);
					std::vector<uint8_t> wav;
					if (!src.empty() && ReadLoose(fs::u8path(src), wav)) {
						if (!InspectWav(wav).ok) { m_status = TXT("Not a WAVE file."); m_statusError = true; }
						else {
							const fs::path out = fs::u8path(m_outDir) / "se" / "battle_se" / fs::u8path(s.name + ".wav");
							std::error_code ec;
							fs::create_directories(out.parent_path(), ec);
							const bool ok = WriteFileAtomic(out.string().c_str(), wav.data(), wav.size());
							m_status = ok ? std::string(TXT("Wrote")) + " " + out.u8string() : TXT("Save failed.");
							m_statusError = !ok;
							if (ok && fs::equivalent(fs::u8path(m_outDir), fs::u8path(m_game), ec)) rebuildSlots();
						}
					}
				}
				ImGui::TableNextColumn();
				char nb[64];
				snprintf(nb, sizeof nb, "%s", s.name.c_str());
				ImGui::SetNextItemWidth(-1);
				ImGui::InputText("##name", nb, sizeof nb, ImGuiInputTextFlags_CharsNoBlank);
				if (ImGui::IsItemDeactivatedAfterEdit() && s.name != nb && nb[0]) { setKey = s.key; setName = nb; doName = true; }
				ImGui::TableNextColumn();
				if (s.info.ok) ImGui::Text("%.2f s", s.info.seconds);
				else ImGui::TextColored(ImVec4(1, 0.45f, 0.4f, 1), "%s", s.source.empty() ? TXT("missing") : TXT("bad"));
				ImGui::TableNextColumn();
				if (s.info.ok && s.info.bits == 16) {
					const ImVec4 c = s.info.peakDb > -0.1 ? ImVec4(1, 0.5f, 0.3f, 1) : ImVec4(0.75f, 0.75f, 0.75f, 1);
					ImGui::TextColored(c, "%.1f dB", s.info.peakDb);
					if (ImGui::IsItemHovered()) ImGui::SetTooltip("%d Hz, %d ch, %d-bit", s.info.rate, s.info.channels, s.info.bits);
				}
				ImGui::TableNextColumn();
				ImGui::TextDisabled("%s", s.source.c_str());
				ImGui::TableNextColumn();
				char cb[256];
				snprintf(cb, sizeof cb, "%s", s.comment.c_str());
				ImGui::SetNextItemWidth(-1);
				ImGui::InputText("##comment", cb, sizeof cb);
				if (ImGui::IsItemDeactivatedAfterEdit() && s.comment != cb) { setKey = s.key; setComment = cb; doComment = true; }
				ImGui::PopID();
			}
			ImGui::EndTable();
		}
		if (doName) { m_list.Set("SeList", setKey, setName); rebuildSlots(); }
		if (doComment) { m_list.SetComment("SeList", setKey, " " + utf82sj(setComment)); rebuildSlots(); }
	} else ImGui::TextDisabled("%s", TXT("Pick a character."));
	ImGui::EndChild();
	ImGui::End();
}

} // namespace sebrowse
