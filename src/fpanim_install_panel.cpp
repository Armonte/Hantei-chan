// fpan-im install window: see fpanim_install_panel.h.
#include "fpanim_install_panel.h"
#include "fpanim_install.h"
#include "squirrel/fpanim_link.h"
#include "ini.h"
#include "i18n.h"
#include "png_writer.h"   // BrowseForFolderUtf8
#include "app_log.h"
#include "ui/ui_style.h"

#include <imgui.h>
#include <imgui_internal.h>   // MarkIniSettingsDirty

#include <windows.h>

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <future>
#include <string>
#include <vector>

namespace fpanimpanel {

bool showPanel = false;

namespace {

using namespace fpaninst;
using Clock = std::chrono::steady_clock;

struct GameSlot {
	const char* id;      // "uni2" / "mbtl" (fpanim_link pipe profile)
	const char* name;    // shown
	std::string* setting;
	const char* defaultDir;
	GameState state;
	uint32_t runningPid = 0;
	Clock::time_point lastScan{};
	std::future<sqedit::FpanimReply> ping;
	bool pingBusy = false, connected = false;
	std::string pingText;
	Clock::time_point lastPing{};
	std::string result;
	bool resultOk = true;
	int confirm = 0;     // 1 install/update, 2 uninstall (waiting for the confirm button)

	std::string dir() const { return setting->empty() ? std::string(defaultDir) : *setting; }
};

GameSlot g_games[2] = {
	{ "uni2", "UNDER NIGHT IN-BIRTH II Sys:Celes (1.42)", &gSettings.sqGameDirUni2,
	  "C:\\Program Files (x86)\\Steam\\steamapps\\common\\UNDER NIGHT IN-BIRTH II Sys Celes" },
	{ "mbtl", "MELTY BLOOD: TYPE LUMINA", &gSettings.sqGameDirMbtl,
	  "C:\\Program Files (x86)\\Steam\\steamapps\\common\\MELTY BLOOD TYPE LUMINA" },
};

Bundle g_bundle;
FileInfo g_bundleShim, g_bundleCore;
Clock::time_point g_lastBundleScan{};
bool g_bundleScanned = false;

bool ExistsU8(const std::string& p)
{
	std::error_code ec;
	return std::filesystem::is_regular_file(std::filesystem::u8path(p), ec);
}

void ScanBundle()
{
	const std::string dir = gSettings.fpanimBundleDir.empty() ? DefaultBundleDir() : gSettings.fpanimBundleDir;
	g_bundle = ResolveBundle(dir, ExistsU8);
	g_bundleShim = g_bundle.found ? ReadFileInfo(g_bundle.winmm) : FileInfo{};
	g_bundleCore = g_bundle.found ? ReadFileInfo(g_bundle.core) : FileInfo{};
	if (g_bundle.found) {
		FILE* f = _wfopen(std::filesystem::u8path(g_bundle.dir + "/VERSION").c_str(), L"rb");
		if (f) {
			char b[128] = {};
			const size_t n = fread(b, 1, sizeof b - 1, f);
			fclose(f);
			std::string v(b, n);
			while (!v.empty() && (v.back() == '\n' || v.back() == '\r' || v.back() == ' ')) v.pop_back();
			g_bundle.versionFile = v;
		}
	}
	g_lastBundleScan = Clock::now();
	g_bundleScanned = true;
}

void ScanGame(GameSlot& g)
{
	g.state = ReadGameState(g.dir());
	g.runningPid = FindRunningGame(g.dir());
	g.lastScan = Clock::now();
}

void PollPing(GameSlot& g)
{
	if (g.pingBusy && g.ping.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
		const sqedit::FpanimReply r = g.ping.get();
		g.pingBusy = false;
		g.connected = r.connected && r.ok;
		g.pingText = r.text;
	}
	if (!g.pingBusy && Clock::now() - g.lastPing > std::chrono::seconds(3)) {
		g.lastPing = Clock::now();
		g.pingBusy = true;
		const std::string pipe = sqedit::FpanimPipeForGame(g.id);
		g.ping = std::async(std::launch::async, [pipe] { return sqedit::FpanimRequest(pipe, "ping", 1500); });
	}
}

std::vector<std::string> PdbsOf(const std::string& dir)
{
	std::vector<std::string> out;
	std::error_code ec;
	for (auto it = std::filesystem::directory_iterator(std::filesystem::u8path(dir) / "fpan-im", ec);
	     !ec && it != std::filesystem::directory_iterator(); it.increment(ec)) {
		if (it->is_regular_file(ec) && it->path().extension() == ".pdb") out.push_back(it->path().filename().u8string());
	}
	return out;
}

void RunAction(GameSlot& g, bool install)
{
	ScanGame(g);
	if (g.runningPid) {
		g.result = TXT("Refused: the game is running. Close it first.");
		g.resultOk = false;
		return;
	}
	if (install) ScanBundle();
	const Plan p = install ? PlanInstall(g.state, g_bundle, NowStamp()) : PlanUninstall(g.state, PdbsOf(g.dir()));
	if (!p.refused.empty()) { g.result = std::string(TXT("Refused: ")) + p.refused; g.resultOk = false; return; }
	const std::string err = Execute(g.dir(), p);
	std::string msg = err.empty() ? std::string(install ? TXT("Installed.") : TXT("Uninstalled.")) : std::string(TXT("Failed: ")) + err;
	for (const std::string& n : p.notes) msg += "\n- " + n;
	g.result = msg;
	g.resultOk = err.empty();
	HC_LOG_INFO("[fpan-im] {} {} in {}: {}", install ? "install" : "uninstall", g.id, g.dir(), err.empty() ? "ok" : err);
	ScanGame(g);
}

void DrawGame(GameSlot& g)
{
	ImGui::PushID(g.id);
	ImGui::SeparatorText(g.name);
	if (Clock::now() - g.lastScan > std::chrono::seconds(3)) ScanGame(g);
	PollPing(g);

	char buf[512];
	std::snprintf(buf, sizeof buf, "%s", g.dir().c_str());
	ImGui::SetNextItemWidth(ui::Px(420));
	if (ImGui::InputText(LBL("Game folder"), buf, sizeof buf)) {
		*g.setting = buf == std::string(g.defaultDir) ? std::string() : std::string(buf);
		ImGui::MarkIniSettingsDirty();
		g.lastScan = {};
	}
	ImGui::SameLine();
	if (ImGui::Button(LBL("Browse..."))) {
		const std::string d = BrowseForFolderUtf8(g.dir());
		if (!d.empty()) { *g.setting = d; ImGui::MarkIniSettingsDirty(); g.lastScan = {}; }
	}

	const Status st = Classify(g.state);
	const ImVec4 good(0.4f, 1.0f, 0.5f, 1.0f), warn(1.0f, 0.7f, 0.3f, 1.0f);
	if (st == Status::NotInstalled) ImGui::TextDisabled("%s", TXT("fpan-im: not installed"));
	else if (st == Status::Installed)
		ImGui::TextColored(good, TXT("fpan-im: installed, core %s, shim %s"), VersionLabel(g.state.core).c_str(), VersionLabel(g.state.shim).c_str());
	else
		ImGui::TextColored(warn, TXT("fpan-im: partly installed (core %s, shim %s)"),
		                   g.state.core.exists ? VersionLabel(g.state.core).c_str() : TXT("missing"),
		                   IsOurShim(g.state.shim) ? VersionLabel(g.state.shim).c_str() : g.state.shim.exists ? TXT("not fpan-im") : TXT("missing"));
	if (g.state.shim.exists && !IsOurShim(g.state.shim))
		ImGui::TextColored(warn, TXT("winmm.dll here is not fpan-im (%s): Install moves it to winmm.chain.dll, which fpan-im chainloads."),
		                   g.state.shim.productName.empty() ? TXT("no version info") : g.state.shim.productName.c_str());
	if (g.state.chainExists) ImGui::TextDisabled("%s", TXT("winmm.chain.dll present (chainloaded by fpan-im's winmm.dll)."));

	if (g.connected) ImGui::TextColored(good, "%s", TXT("Game: connected (fpan-im pipe answers)"));
	else if (g.runningPid) ImGui::TextColored(warn, TXT("Game: running (pid %u), fpan-im not connected"), (unsigned)g.runningPid);
	else ImGui::TextDisabled("%s", TXT("Game: not connected"));
	if (g.connected && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", g.pingText.c_str());

	const bool busy = g.runningPid != 0;
	ImGui::BeginDisabled(busy || !g_bundle.found);
	if (ImGui::Button(st == Status::NotInstalled ? LBL("Install") : LBL("Update"))) g.confirm = 1;
	ImGui::EndDisabled();
	ImGui::SameLine();
	ImGui::BeginDisabled(busy || st == Status::NotInstalled);
	if (ImGui::Button(LBL("Uninstall"))) g.confirm = 2;
	ImGui::EndDisabled();
	if (busy) { ImGui::SameLine(); ImGui::TextDisabled("%s", TXT("(close the game to install or uninstall)")); }

	if (g.confirm) {
		ImGui::TextColored(warn, "%s", g.confirm == 1 ? TXT("Copy winmm.dll and fpan-im\\fpan-im.dll into this folder?")
		                                              : TXT("Remove fpan-im's winmm.dll and fpan-im\\fpan-im.dll? (mods, ini and logs are kept)"));
		if (ImGui::Button(LBL("Confirm"))) { RunAction(g, g.confirm == 1); g.confirm = 0; }
		ImGui::SameLine();
		if (ImGui::Button(LBL("Cancel"))) g.confirm = 0;
	}
	if (!g.result.empty()) {
		ImGui::PushTextWrapPos(ui::Px(640));
		if (g.resultOk) ImGui::TextUnformatted(g.result.c_str());
		else ImGui::TextColored(warn, "%s", g.result.c_str());
		ImGui::PopTextWrapPos();
	}
	ImGui::PopID();
}

} // namespace

void DrawPanel()
{
	if (!showPanel) return;
	ImGui::SetNextWindowSize(ImVec2(ui::Px(680), 0), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin(LBL("fpan-im (UNI2 / MBTL)"), &showPanel)) { ImGui::End(); return; }
	if (!g_bundleScanned || Clock::now() - g_lastBundleScan > std::chrono::seconds(5)) ScanBundle();

	ImGui::SeparatorText(TXT("Bundled fpan-im"));
	if (g_bundle.found) {
		ImGui::Text(TXT("Bundle: core %s, shim %s%s%s"), VersionLabel(g_bundleCore).c_str(), VersionLabel(g_bundleShim).c_str(),
		            g_bundle.versionFile.empty() ? "" : "  ", g_bundle.versionFile.c_str());
		ImGui::TextDisabled("%s", g_bundle.dir.c_str());
	} else {
		ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), "%s", TXT("fpan-im not bundled"));
		ui::HelpMarker(TXT("This build has no res\\fpan-im folder. Choose a folder holding winmm.dll and fpan-im.dll (fpan-im's build\\bin, or a folder with winmm.dll + fpan-im\\fpan-im.dll)."));
	}
	if (ImGui::Button(LBL("Choose fpan-im folder..."))) {
		const std::string d = BrowseForFolderUtf8(gSettings.fpanimBundleDir.empty() ? DefaultBundleDir() : gSettings.fpanimBundleDir);
		if (!d.empty()) { gSettings.fpanimBundleDir = d; ImGui::MarkIniSettingsDirty(); ScanBundle(); }
	}
	if (!gSettings.fpanimBundleDir.empty()) {
		ImGui::SameLine();
		if (ImGui::Button(LBL("Use bundled copy"))) { gSettings.fpanimBundleDir.clear(); ImGui::MarkIniSettingsDirty(); ScanBundle(); }
	}

	for (GameSlot& g : g_games) DrawGame(g);
	ImGui::TextDisabled("%s", TXT("UNI2-IM / MBTL-IM (dinput8.dll) are never touched. fpan-im\\mods, fpan-im.ini and logs stay on uninstall."));
	ImGui::End();
}

} // namespace fpanimpanel
