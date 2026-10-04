#include "han2_browser.h"
#include "i18n.h"
#include "han2/pac_archive.h"
#include "han2/gof1_archive.h"
#include "fbarc/fb_archive.h"
#include "han2_pac_window.h"
#include "archive_browser.h"
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
std::string g_gameDir;
}

namespace {
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
	char gb[1024]{}; GetPrivateProfileStringA("han2", "LiveReloadGameDir", "", gb, sizeof(gb), p.c_str());
	g_gameDir = gb;
}
void SaveHan2Settings()
{
	const std::string p = SettingsPath();
	WritePrivateProfileStringA("han2", "Language", std::to_string(i18n::language).c_str(), p.c_str());
	WritePrivateProfileStringA("han2", "WorkFolder", g_workFolder.c_str(), p.c_str());
	WritePrivateProfileStringA("han2", "LiveReloadGameDir", g_gameDir.c_str(), p.c_str());
}
const std::string &WorkFolder() { return g_workFolder; }
const std::string &LiveReloadGameDir() { return g_gameDir; }
void SetLiveReloadGameDir(const std::string &dir) { g_gameDir = dir; SaveHan2Settings(); }
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

std::string AddArchive(const std::string &path) { return abrowser::OpenPath(path); }

} // namespace han2ui
