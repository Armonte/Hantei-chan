#include "han2_browser.h"
#include "i18n.h"
#include "han2/pac_archive.h"
#include "han2/gof1_archive.h"
#include "han2_pac_window.h"
#include "filedialog.h"
#include "png_writer.h"
#include "misc.h"

#include <windows.h>
#include <imgui.h>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>

namespace han2ui {

bool showBrowser = false;

bool showLoadReport = false;
const char *Tr(const char *en, const char *jp) { return i18n::language == 1 ? jp : en; }

namespace {
std::string SettingsPath()
{
	char buf[512]; DWORD n = GetCurrentDirectoryA(512, buf);
	return std::string(buf, n) + "\\han2_settings.ini";
}
struct LoadEntry { std::string name, summary; std::vector<std::string> warnings; bool failed; };
std::vector<LoadEntry> g_reports;
std::string g_workFolder;
struct FolderNode { std::string name, path; std::vector<FolderNode> dirs; std::vector<std::string> files; };
FolderNode g_root; bool g_scanned = false;
void ScanFolder(const std::string &dir, FolderNode &node, int depth)
{
	if (dir.empty() || depth > 4) return;
	node.path = dir;
	std::error_code ec;
	for (auto &e : std::filesystem::directory_iterator(std::filesystem::u8path(dir), ec)) {
		if (e.is_directory(ec)) { FolderNode d; d.name = e.path().filename().u8string(); ScanFolder(e.path().u8string(), d, depth + 1); if (!d.dirs.empty() || !d.files.empty()) node.dirs.push_back(std::move(d)); continue; }
		if (!e.is_regular_file(ec)) continue;
		std::string x; for (char ch : e.path().extension().string()) x += (char)tolower((unsigned char)ch);
		if (x == ".dt2" || x == ".dat" || x == ".pac" || x == ".pat" || x == ".chp" || x == ".img" || x == ".p" || x == ".fob") node.files.push_back(e.path().u8string());
	}
	std::sort(node.files.begin(), node.files.end());
	std::sort(node.dirs.begin(), node.dirs.end(), [](const FolderNode &a, const FolderNode &b) { return a.name < b.name; });
}
struct Mounted { std::shared_ptr<pac::Archive> a; std::shared_ptr<gof1::Archive> g; std::string shortName; };
std::vector<Mounted> g_mounted;
int g_sel = 0;
char g_filter[64] = "";
std::string g_extractStatus;

std::string Lower(std::string s) { for (auto &c : s) c = (char)tolower((unsigned char)c); return s; }
bool EndsWith(const std::string &s, const char *suf)
{
	std::string l = Lower(s); size_t n = strlen(suf);
	return l.size() >= n && l.compare(l.size() - n, n, suf) == 0;
}
}

void LoadHan2Settings()
{
	const std::string p = SettingsPath();
	i18n::language = (int)GetPrivateProfileIntA("han2", "Language", 0, p.c_str());
	char buf[1024]{}; GetPrivateProfileStringA("han2", "WorkFolder", "", buf, sizeof(buf), p.c_str());
	g_workFolder = buf;
}
void SaveHan2Settings()
{
	const std::string p = SettingsPath();
	WritePrivateProfileStringA("han2", "Language", std::to_string(i18n::language).c_str(), p.c_str());
	WritePrivateProfileStringA("han2", "WorkFolder", g_workFolder.c_str(), p.c_str());
}
const std::string &WorkFolder() { return g_workFolder; }
void SetWorkFolder(const std::string &dir) { g_workFolder = dir; SaveHan2Settings(); }

void PushLoadReport(const std::string &name, const std::string &summary, const std::vector<std::string> &warnings, bool failed)
{
	g_reports.push_back({name, summary, warnings, failed});
	if (g_reports.size() > 50) g_reports.erase(g_reports.begin());
	if (failed || !warnings.empty()) showLoadReport = true;   // clean loads stay quiet; the window can be opened from the menu
}
// Load-report text is assembled in English by the loaders; translate it piecewise when it is shown.
static std::string TrReportPiece(const std::string &p)
{
	int n = 0; char buf[256];
	auto endsWith = [&](const char *suf) { size_t l = strlen(suf); return p.size() >= l && p.compare(p.size() - l, l, suf) == 0; };
	if (endsWith(" CG images") && sscanf(p.c_str(), "%d", &n) == 1) { snprintf(buf, sizeof buf, TXT("%d CG images"), n); return buf; }
	if (endsWith(" part sets") && sscanf(p.c_str(), "%d", &n) == 1) { snprintf(buf, sizeof buf, TXT("%d part sets"), n); return buf; }
	for (const char *pre : {"parts: ", "parts could not be converted: "})
		if (p.compare(0, strlen(pre), pre) == 0) return std::string(TXT(pre)) + p.substr(strlen(pre));
	return TXT(p.c_str());
}
static std::string TrReportSummary(const std::string &s)
{
	std::string out, sep = i18n::language == 1 ? "\xe3\x80\x81" : ", ";
	size_t i = 0;
	while (i <= s.size()) {
		size_t j = s.find(", ", i); if (j == std::string::npos) j = s.size();
		if (!out.empty()) out += sep;
		out += TrReportPiece(s.substr(i, j - i));
		i = j + 2;
	}
	return out;
}
void DrawLoadReport()
{
	if (!showLoadReport) return;
	ImGui::SetNextWindowSize(ImVec2(620, 320), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin(LBL("Loading report"), &showLoadReport)) { ImGui::End(); return; }
	if (ImGui::Button(LBL("Clear"))) g_reports.clear();
	for (int i = (int)g_reports.size() - 1; i >= 0; i--) {
		const LoadEntry &e = g_reports[i];
		ImGui::PushID(i);
		ImGui::TextColored(e.failed ? ImVec4(1, .4f, .3f, 1) : (e.warnings.empty() ? ImVec4(.5f, 1, .5f, 1) : ImVec4(1, .8f, .3f, 1)), "%s", e.name.c_str());
		ImGui::SameLine(); ImGui::TextDisabled("%s", TrReportSummary(e.summary).c_str());
		for (auto &w : e.warnings) ImGui::BulletText("%s", TrReportPiece(w).c_str());
		ImGui::PopID();
	}
	ImGui::End();
}

std::string AddArchive(const std::string &path)
{
	auto a = std::make_shared<pac::Archive>();
	std::string err;
	if (!pac::Open(path, *a, &err)) {
		auto g = std::make_shared<gof1::Archive>(); std::string e2;
		if (!gof1::Open(path, *g, &e2)) return path + ": " + err;
		for (auto &m : g_mounted) if (m.g && m.g->path == path) return {};
		Mounted m; m.g = g; m.a = std::make_shared<pac::Archive>(); m.a->path = path;
		for (auto &e : g->entries) { pac::Entry pe; pe.name = e.name; pe.size = e.size; pe.offset = e.offset; m.a->entries.push_back(pe); }
		m.shortName = std::filesystem::u8path(path).filename().string() + " (GOF1)";
		g_mounted.push_back(m); g_sel = (int)g_mounted.size() - 1; showBrowser = true;
		return {};
	}
	for (auto &m : g_mounted) if (m.a->path == path) return {};
	Mounted m; m.a = a;
	m.shortName = std::filesystem::u8path(path).filename().string();
	g_mounted.push_back(m);
	g_sel = (int)g_mounted.size() - 1;
	showBrowser = true;
	return {};
}

bool DrawBrowser(OpenRequest &req, std::string &message)
{
	bool open = false;
	if (!showBrowser) return false;
	ImGui::SetNextWindowSize(ImVec2(760, 520), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin(LBL("RBO / GOF2 archives"), &showBrowser)) { ImGui::End(); return false; }

	if (ImGui::Button(LBL("Add archive..."))) {
		std::string path = FileDialog(fileType::HAN2, false);
		if (!path.empty()) message = AddArchive(path);
	}
	ImGui::SameLine();
	if (ImGui::Button(LBL("Working folder..."))) { std::string d = BrowseForFolderUtf8(""); if (!d.empty()) { SetWorkFolder(d); g_scanned = false; } }
	ImGui::SameLine();
	if (ImGui::Button(LBL("Refresh"))) g_scanned = false;
	ImGui::SameLine();
	{ const char *lg = i18n::language == 1 ? "EN" : "JP"; if (ImGui::Button(lg)) { i18n::language = 1 - i18n::language; SaveHan2Settings(); } }
	if (!g_scanned) { g_scanned = true; g_root = FolderNode(); ScanFolder(WorkFolder(), g_root, 0); }
	ImGui::SameLine();
	ImGui::PushTextWrapPos(0.0f); ImGui::TextDisabled("%s", TXT("Later archives in the list override earlier ones when a file name occurs twice (Update01 and the Ex discs patch DATA0x).")); ImGui::PopTextWrapPos();

	// archive list
	ImGui::BeginChild("arcs", ImVec2(230, 0), true);
	for (int i = 0; i < (int)g_mounted.size(); i++) {
		ImGui::PushID(i);
		char lbl[160]; snprintf(lbl, sizeof(lbl), "%d. %s (%zu)", i + 1, g_mounted[i].shortName.c_str(), g_mounted[i].a->entries.size());
		if (ImGui::Selectable(lbl, g_sel == i)) g_sel = i;
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", g_mounted[i].a->path.c_str());
		ImGui::PopID();
	}
	if (!g_mounted.empty() && g_sel >= 0 && g_sel < (int)g_mounted.size()) {
		if (ImGui::Button(LBL("Up")) && g_sel > 0) { std::swap(g_mounted[g_sel], g_mounted[g_sel - 1]); g_sel--; }
		ImGui::SameLine();
		if (ImGui::Button(LBL("Down")) && g_sel + 1 < (int)g_mounted.size()) { std::swap(g_mounted[g_sel], g_mounted[g_sel + 1]); g_sel++; }
		ImGui::SameLine();
		if (ImGui::Button(LBL("Unmount"))) { g_mounted.erase(g_mounted.begin() + g_sel); g_sel = std::max(0, g_sel - 1); }
	}
	if (!WorkFolder().empty()) {
		ImGui::Separator(); ImGui::TextDisabled("%s", WorkFolder().c_str());
		if (WorkFolder().size() >= 2 && (WorkFolder()[0] == 'C' || WorkFolder()[0] == 'c') && WorkFolder()[1] == ':')
			ImGui::TextColored(ImVec4(1, .8f, .3f, 1), "%s", TXT("The working folder is on C: (not recommended: Program Files / permission problems)."));
		int uid = 0;
		std::function<void(const FolderNode &)> draw = [&](const FolderNode &n) {
			for (auto &d : n.dirs) if (ImGui::TreeNode((d.name + "##" + std::to_string(uid++)).c_str())) { draw(d); ImGui::TreePop(); }
			for (auto &f : n.files) {
				ImGui::PushID(uid++);
				std::string nm = std::filesystem::u8path(f).filename().string();
				if (ImGui::Selectable(nm.c_str())) { req.stem = "\x01open"; req.read = nullptr; req.origin = f; open = true; }
				ImGui::PopID();
			}
		};
		draw(g_root);
	}
	ImGui::EndChild();
	ImGui::SameLine();

	ImGui::BeginChild("entries");
	ImGui::SetNextItemWidth(200);
	ImGui::InputText(LBL("filter"), g_filter, sizeof(g_filter));
	if (!message.empty()) ImGui::TextColored(ImVec4(1, .5f, .3f, 1), "%s", message.c_str());
	if (!g_extractStatus.empty()) ImGui::TextDisabled("%s", g_extractStatus.c_str());
	if (g_sel >= 0 && g_sel < (int)g_mounted.size()) {
		pac::Archive &a = *g_mounted[g_sel].a;
		if (ImGui::BeginTable("ents", 3, ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable)) {
			ImGui::TableSetupColumn(LBL("name")); ImGui::TableSetupColumn(LBL("size")); ImGui::TableSetupColumn(LBL("action"));
			ImGui::TableSetupScrollFreeze(0, 1);
			ImGui::TableHeadersRow();
			std::string flt = Lower(g_filter);
			for (size_t i = 0; i < a.entries.size(); i++) {
				const pac::Entry &e = a.entries[i];
				if (!flt.empty() && Lower(e.name).find(flt) == std::string::npos) continue;
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::PushID((int)i);
				const bool isChar = EndsWith(e.name, ".dat") || EndsWith(e.name, ".dt2");
				ImGui::Selectable(e.name.c_str(), false, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick);
				if (isChar && ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
					if (g_mounted[g_sel].g) { req.stem = "\x01gof1"; req.gof1Archive = g_mounted[g_sel].g->path; req.gof1Entry = e.name; req.origin = g_mounted[g_sel].shortName; open = true; }
					else {
					std::string stem = e.name.substr(0, e.name.size() - 4);
					std::vector<std::shared_ptr<pac::Archive>> order;
					// the clicked archive first, then the rest with later mounts winning
					order.push_back(g_mounted[g_sel].a);
					for (int k = (int)g_mounted.size() - 1; k >= 0; k--) if (k != g_sel) order.push_back(g_mounted[k].a);
					req.stem = stem; req.read = han2::PacReader(order); req.origin = g_mounted[g_sel].shortName;
					open = true;
					}
				}
				if (!isChar && ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
					std::vector<uint8_t> bytes; std::string rerr;
					if (g_mounted[g_sel].g ? gof1::ReadEntry(*g_mounted[g_sel].g, i, bytes, &rerr) : pac::ReadEntry(a, i, bytes, &rerr)) OpenFileViewer(e.name, std::move(bytes), g_mounted[g_sel].shortName); else message = rerr;
				}
				ImGui::TableSetColumnIndex(1); ImGui::Text("%u", e.size);
				ImGui::TableSetColumnIndex(2);
				if (ImGui::SmallButton(LBL("Extract..."))) {
					char defName[260]{}; snprintf(defName, sizeof(defName), "%s", e.name.c_str());
					std::string out = FileDialog(-1, true, defName);
					if (!out.empty()) {
						std::vector<uint8_t> b; std::string err;
						if (g_mounted[g_sel].g ? gof1::ReadEntry(*g_mounted[g_sel].g, i, b, &err) : pac::ReadEntry(a, i, b, &err)) {
							std::ofstream f(std::filesystem::u8path(out), std::ios::binary);
							if (f && (b.empty() || f.write((const char *)b.data(), (std::streamsize)b.size()))) { char sb[1024]; snprintf(sb, sizeof(sb), TXT("extracted %s to %s"), e.name.c_str(), out.c_str()); g_extractStatus = sb; }
							else { char sb[1024]; snprintf(sb, sizeof(sb), TXT("could not write %s"), out.c_str()); g_extractStatus = sb; }
						} else g_extractStatus = err;
					}
				}
				ImGui::PopID();
			}
			ImGui::EndTable();
		}
	} else {
		ImGui::PushTextWrapPos(0.0f); ImGui::TextDisabled("%s", TXT("Add a PAC archive (RBO DATA01.PAC ... Ex3Disc.PAC, GOF2 data00.dat ...).")); ImGui::PopTextWrapPos();
	}
	ImGui::EndChild();
	ImGui::End();
	return open;
}

} // namespace han2ui
