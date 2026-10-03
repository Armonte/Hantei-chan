#include "han2_browser.h"
#include "han2/pac_archive.h"
#include "han2_pac_window.h"
#include "filedialog.h"
#include "png_writer.h"
#include "misc.h"

#include <imgui.h>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>

namespace han2ui {

bool showBrowser = false;

namespace {
struct Mounted { std::shared_ptr<pac::Archive> a; std::string shortName; };
std::vector<Mounted> g_mounted;
int g_sel = 0;
char g_filter[64] = "";
std::string g_extractStatus;
std::string g_folder;
std::vector<std::string> g_folderFiles;

std::string Lower(std::string s) { for (auto &c : s) c = (char)tolower((unsigned char)c); return s; }
bool EndsWith(const std::string &s, const char *suf)
{
	std::string l = Lower(s); size_t n = strlen(suf);
	return l.size() >= n && l.compare(l.size() - n, n, suf) == 0;
}
}

std::string AddArchive(const std::string &path)
{
	auto a = std::make_shared<pac::Archive>();
	std::string err;
	if (!pac::Open(path, *a, &err)) return path + ": " + err;
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
	if (!ImGui::Begin("RBO / GOF2 archives", &showBrowser)) { ImGui::End(); return false; }

	if (ImGui::Button("Add archive...")) {
		std::string path = FileDialog(fileType::HAN2, false);
		if (!path.empty()) message = AddArchive(path);
	}
	ImGui::SameLine();
	if (ImGui::Button("Open folder...")) { std::string d = BrowseForFolderUtf8(""); if (!d.empty()) { g_folder = d; g_folderFiles.clear(); std::error_code ec; for (auto &e : std::filesystem::directory_iterator(std::filesystem::u8path(d), ec)) { if (!e.is_regular_file()) continue; std::string x = Lower(e.path().extension().string()); if (x == ".dt2" || x == ".dat" || x == ".pac" || x == ".pat" || x == ".chp" || x == ".img") g_folderFiles.push_back(e.path().u8string()); } std::sort(g_folderFiles.begin(), g_folderFiles.end()); } }
	ImGui::SameLine();
	ImGui::TextDisabled("Later archives in the list override earlier ones when a file name occurs twice (Update01 and the Ex discs patch DATA0x).");

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
		if (ImGui::Button("Up") && g_sel > 0) { std::swap(g_mounted[g_sel], g_mounted[g_sel - 1]); g_sel--; }
		ImGui::SameLine();
		if (ImGui::Button("Down") && g_sel + 1 < (int)g_mounted.size()) { std::swap(g_mounted[g_sel], g_mounted[g_sel + 1]); g_sel++; }
		ImGui::SameLine();
		if (ImGui::Button("Unmount")) { g_mounted.erase(g_mounted.begin() + g_sel); g_sel = std::max(0, g_sel - 1); }
	}
	if (!g_folderFiles.empty()) {
		ImGui::Separator(); ImGui::TextDisabled("folder: %s", g_folder.c_str());
		for (size_t i = 0; i < g_folderFiles.size(); i++) {
			ImGui::PushID((int)(1000 + i));
			std::string nm = std::filesystem::u8path(g_folderFiles[i]).filename().string();
			if (ImGui::Selectable(nm.c_str()) ) { req.stem.clear(); req.read = nullptr; req.origin = g_folderFiles[i]; req.stem = "\x01open"; open = true; }
			ImGui::PopID();
		}
	}
	ImGui::EndChild();
	ImGui::SameLine();

	ImGui::BeginChild("entries");
	ImGui::SetNextItemWidth(200);
	ImGui::InputText("filter", g_filter, sizeof(g_filter));
	if (!message.empty()) ImGui::TextColored(ImVec4(1, .5f, .3f, 1), "%s", message.c_str());
	if (!g_extractStatus.empty()) ImGui::TextDisabled("%s", g_extractStatus.c_str());
	if (g_sel >= 0 && g_sel < (int)g_mounted.size()) {
		pac::Archive &a = *g_mounted[g_sel].a;
		if (ImGui::BeginTable("ents", 3, ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable)) {
			ImGui::TableSetupColumn("name"); ImGui::TableSetupColumn("size"); ImGui::TableSetupColumn("action");
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
					std::string stem = e.name.substr(0, e.name.size() - 4);
					std::vector<std::shared_ptr<pac::Archive>> order;
					// the clicked archive first, then the rest with later mounts winning
					order.push_back(g_mounted[g_sel].a);
					for (int k = (int)g_mounted.size() - 1; k >= 0; k--) if (k != g_sel) order.push_back(g_mounted[k].a);
					req.stem = stem; req.read = han2::PacReader(order); req.origin = g_mounted[g_sel].shortName;
					open = true;
				}
				if (!isChar && ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
					std::vector<uint8_t> bytes; std::string rerr;
					if (pac::ReadEntry(a, i, bytes, &rerr)) OpenFileViewer(e.name, std::move(bytes), g_mounted[g_sel].shortName); else message = rerr;
				}
				ImGui::TableSetColumnIndex(1); ImGui::Text("%u", e.size);
				ImGui::TableSetColumnIndex(2);
				if (ImGui::SmallButton("Extract...")) {
					char defName[260]{}; snprintf(defName, sizeof(defName), "%s", e.name.c_str());
					std::string out = FileDialog(-1, true, defName);
					if (!out.empty()) {
						std::vector<uint8_t> b; std::string err;
						if (pac::ReadEntry(a, i, b, &err)) {
							std::ofstream f(std::filesystem::u8path(out), std::ios::binary);
							if (f && (b.empty() || f.write((const char *)b.data(), (std::streamsize)b.size()))) g_extractStatus = "extracted " + e.name + " to " + out;
							else g_extractStatus = "could not write " + out;
						} else g_extractStatus = err;
					}
				}
				ImGui::PopID();
			}
			ImGui::EndTable();
		}
	} else {
		ImGui::TextDisabled("Add a PAC archive (RBO DATA01.PAC ... Ex3Disc.PAC, GOF2 data00.dat ...).");
	}
	ImGui::EndChild();
	ImGui::End();
	return open;
}

} // namespace han2ui
