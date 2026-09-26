// [authoring] The Authoring workspace: header (game folder, launch / attach, link, crash relaunch), Setup, Setups, the
// HUD tab host, the edit sink (instant apply + one undo history) and the public API. See authoring_window.h and
// docs/HANTEI_AUTHORING_MODE.md §5 / §8.9. Tuning: authoring_tuning_ui.cpp. Live + Log: authoring_live_ui.cpp.
#include "authoring_state.h"
#include "hud_layout.h"
#include "game_view.h"
#include "roster_mirror.h"
#include "../game_link_panel.h"
#include "../pal_file.h"
#include "../background/bg_project.h"

#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>

namespace authoring {

namespace fs = std::filesystem;
namespace wire = gamelink::wire;

bool showWindow = false;

namespace {

uint64_t NowMs() { return GetTickCount64(); }
bool FileExists(const std::string& p) { std::error_code ec; return !p.empty() && fs::exists(fs::u8path(p), ec); }
std::string Lower(std::string s) { for (char& c : s) if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a'); return s; }

const char* kTabNames[] = { "Setup", "Setups", "Tuning", "HUD", "Live", "Log" };
constexpr int kTabCount = 6;

std::string SettingsPath()
{
	char dir[MAX_PATH] = "";
	GetCurrentDirectoryA(MAX_PATH, dir);
	return std::string(dir) + "\\hanteichan_authoring.ini";
}

// ---- the edit sink: every edit is one undo step, saved at once and live-applied ----
class WindowSinkImpl final : public EditSink {
public:
	tagtune::SidecarWorkspace& Workspace() override { return St().ws; }
	bool CanEdit() const override { return St().ws.IsOpen() && Policy(Link().Get()).canWriteFiles; }
	void Begin(const std::string& label) override
	{
		AuthoringState& a = St();
		if (a.gestureOpen) return;
		a.gestureOpen = true;
		a.gestureLabel = label;
		a.gestureBefore = a.ws.Take();
		a.gestureDirty = false;
	}
	void Edited() override
	{
		AuthoringState& a = St();
		a.gestureDirty = true;
		const uint64_t now = NowMs();
		if (a.liveApply && a.gestureOpen && now - a.lastGestureSaveMs >= 160) {   // a drag: ~6 saves a second
			a.lastGestureSaveMs = now;
			SaveAndApply(a.gestureLabel);
		}
	}
	void End() override
	{
		AuthoringState& a = St();
		if (!a.gestureOpen) return;
		a.gestureOpen = false;
		const bool changed = a.history.Commit(a.gestureLabel, a.gestureBefore, a.ws);
		a.gestureBefore.clear();
		if (changed && a.liveApply) SaveAndApply(a.gestureLabel);
		else if (changed) a.status = "edited " + a.gestureLabel + " (not saved: live apply is off)";
	}
};

WindowSinkImpl g_sink;

// ---- settings ----
void LoadSettings()
{
	AuthoringState& a = St();
	a.loaded = true;
	a.mbacDir = "C:\\games\\MB\\AC\\install\\MBACPC\\02_extracted";
	std::ifstream f(SettingsPath(), std::ios::binary);
	if (!f) return;
	std::stringstream ss;
	ss << f.rdbuf();
	const std::string text = ss.str();
	a.lib.Parse(text);
	std::istringstream in(text);
	std::string line, sec;
	while (std::getline(in, line)) {
		if (!line.empty() && line.back() == '\r') line.pop_back();
		if (!line.empty() && line[0] == '[') { sec = line; continue; }
		if (sec != "[Authoring]") continue;
		const size_t eq = line.find('=');
		if (eq == std::string::npos) continue;
		const std::string k = line.substr(0, eq), v = line.substr(eq + 1);
		if (k == "gameDir") a.gameDir = v;
		else if (k == "mbacDir") a.mbacDir = v;
		else if (k == "openChars") a.openChars = v == "1";
		else if (k == "reReadFirst") a.reReadFirst = v == "1";
		else if (k == "liveApply") a.liveApply = v == "1";
		else if (k == "setup") { wire::MatchSetup w{}; if (FromHex(v, w)) a.setup = FromWire(w); }
		else if (k == "style") a.setup.style = v;
	}
}

// Characters of the setup, opened as editor tabs (§5.4): P1 point first (focus).
void OpenCharacters(HostContext& host)
{
	AuthoringState& a = St();
	a.charWarnings.clear();
	bool first = true;
	for (const OpenTarget& t : FilesToOpen(a.setup, a.roster, a.gameDir)) {
		if (!FileExists(t.txtPath)) { a.charWarnings.push_back("data\\" + t.file + "_" + std::to_string(t.moon) + ".txt missing (packed data?)"); continue; }
		if (host.openCharacter && !host.openCharacter(t.txtPath, first)) a.charWarnings.push_back("could not open " + t.txtPath);
		first = false;
	}
}

std::vector<Problem> ValidateNow(const gamelink::Snapshot& s)
{
	AuthoringState& a = St();
	ValidateContext vc;
	vc.team4p = s.haveCaps && (s.caps.caps & wire::kCapTeam4P);
	vc.trainingScene = !s.haveCaps || (s.caps.caps & wire::kCapTrainingScene);
	if (!a.gameDir.empty()) {
		vc.paletteCount = [](const std::string& f) { return PaletteCount(f); };
		vc.txtExists = [](const std::string& f, int m) { return FileExists(DataTxt(f, m)); };
		if (FileExists(a.gameDir + "\\Bg\\BgList.ini")) {
			static bg::StageProject proj;
			static std::string projDir;
			if (projDir != a.gameDir) { projDir = a.gameDir; proj.Open(a.gameDir, bg::Game::MBAACC); }
			if (proj.IsOpen()) vc.stageExists = [](int id) { const bg::StageEntry* e = proj.Find(id); return e && e->listed; };
		}
	}
	return Validate(a.setup, a.roster, vc);
}

void LoadInGame(HostContext& host, const gamelink::Snapshot& s)
{
	AuthoringState& a = St();
	if (a.reReadFirst && a.ws.AnyDirty()) {
		const tagtune::SidecarWorkspace::SaveResult r = a.ws.SaveDirty();
		if (!r.ok) { a.loadResult = "NOT loaded: " + r.message; return; }
	}
	Setup send = a.setup;
	send.tuningFirst = a.reReadFirst;
	a.loadRequested = send;
	a.loadSeq = Link().SetMatchSetup(ToWire(send));
	a.loadSentMs = NowMs();
	a.loadResult = a.loadSeq ? "sent: " + Describe(send, a.roster) : "not sent (no Authoring link)";
	a.lib.PushRecent(a.setup);
	TakeCheckpoint("Load in game: " + Describe(a.setup, a.roster));
	if (a.openChars) OpenCharacters(host);
	SaveSettings();
	(void)s;
}

void LaunchGame(HostContext& host)
{
	AuthoringState& a = St();
	(void)host;
	std::string why;
	Link().Disconnect();
	if (!a.launcher.Launch(a.gameDir, LaunchEnv(a.setup), &why)) { a.status = "launch: " + why; return; }
	a.crashDismissed = false;
	a.openAfterReady = a.openChars;
	a.lib.PushRecent(a.setup);
	TakeCheckpoint("Launch: " + Describe(a.setup, a.roster));
	a.status = "launching: " + Describe(a.setup, a.roster);
	SaveSettings();
}

// ---- the header ----
void Header(HostContext& host, const gamelink::Snapshot& s, const LinkPolicy& p)
{
	AuthoringState& a = St();
	ImGui::TextColored(kColExp, "EXPERIMENTAL");
	ImGui::SameLine();
	ImGui::TextDisabled("Authoring (MBAACC) - docs/HANTEI_AUTHORING_MODE.md");
	// game folder + checks
	char buf[512];
	std::snprintf(buf, sizeof buf, "%s", a.gameDir.c_str());
	ImGui::SetNextItemWidth(360);
	if (ImGui::InputTextWithHint("Game##dir", "C:\\games\\mbaacc_dev", buf, sizeof buf, ImGuiInputTextFlags_EnterReturnsTrue)) {
		a.gameDir = buf;
		SaveSettings();
	}
	if (ImGui::IsItemDeactivatedAfterEdit()) { a.gameDir = buf; SaveSettings(); }
	if (s.connected && s.pid) {
		ImGui::SameLine();
		if (ImGui::SmallButton("use the linked game's folder")) { a.gameDir = gamelink::GameDirOf(s.pid); SaveSettings(); }
	}
	const GameDirCheck chk = CheckGameDir(a.gameDir);
	ImGui::SameLine();
	auto mark = [](const char* what, bool ok) {
		ImGui::SameLine();
		ImGui::TextColored(ok ? kColOk : kColBad, "%s %s", what, ok ? "ok" : "missing");
	};
	ImGui::TextUnformatted("");
	mark("MBAA", chk.exe);
	mark("pchost", chk.pchost);
	mark("pc_inject", chk.inject);
	mark("loose data", chk.looseData);
	for (const std::string& n : chk.notes) ImGui::TextColored(kColWarn, "  %s", n.c_str());
	// launch / attach / detach
	const LaunchState ls = a.launcher.Get();
	const bool launching = ls.phase == LaunchPhase::Starting || ls.phase == LaunchPhase::WaitingForGame;
	ImGui::BeginDisabled(!chk.CanLaunch() || launching || !ValidateNow(s).empty());
	if (ImGui::Button("Launch")) LaunchGame(host);
	ImGui::EndDisabled();
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
		const std::vector<Problem> pr = ValidateNow(s);
		if (!chk.CanLaunch()) ImGui::SetTooltip("%s", chk.problems.front().c_str());
		else if (!pr.empty()) ImGui::SetTooltip("fix the setup first: %s", pr.front().msg.c_str());
		else ImGui::SetTooltip("pc_inject MBAA.exe pchost.dll with this setup (PCHOST_MBAACC_AUTHORING_SETUP)");
	}
	ImGui::SameLine();
	if (ImGui::BeginCombo("##attach", "Attach...", ImGuiComboFlags_NoArrowButton | ImGuiComboFlags_WidthFitPreview)) {
		const std::vector<RunningGame> games = ListRunningGames();
		if (games.empty()) ImGui::TextDisabled("no MBAA.exe running");
		for (const RunningGame& g : games) {
			const bool other = !a.gameDir.empty() && Lower(g.dir) != Lower(a.gameDir);
			char label[600];
			std::snprintf(label, sizeof label, "pid %u  %s%s", (unsigned)g.pid, g.image.c_str(), other ? "  (another folder)" : "");
			if (ImGui::Selectable(label)) {
				if (other && a.attachConfirmPid != g.pid) { a.attachConfirmPid = g.pid; a.status = "that game runs from another folder: pick it again to attach anyway"; }
				else {
					a.attachConfirmPid = 0;
					Link().SetTargetPid(g.pid);
					Link().Connect();
					a.launcher.Watch(g.pid, g.dir);
					if (a.gameDir.empty() || other) a.gameDir = g.dir;
					a.status = "attaching to pid " + std::to_string(g.pid);
				}
			}
		}
		ImGui::EndCombo();
	}
	ImGui::SameLine();
	ImGui::BeginDisabled(!s.connected && !s.wantConnected);
	if (ImGui::Button("Detach")) { Link().Disconnect(); a.status = "detached (the game keeps running)"; }
	ImGui::EndDisabled();
	if (a.launcher.Owns(ls.gamePid) && ls.phase == LaunchPhase::Running) {
		ImGui::SameLine();
		if (ImGui::Button("Close game")) a.launcher.KillOwn();
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Terminate the MBAA.exe this window launched (pid %u). Never another process.", ls.gamePid);
	}
	ImGui::SameLine();
	if (ImGui::Button(showGameView ? "Game view: on" : "Game view")) showGameView = !showGameView;
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("The game rendered inside Hantei-chan (a dockable panel), with input forwarding and a hitbox overlay");
	ImGui::SameLine();
	if (launching) ImGui::TextColored(kColWarn, "%s...", LaunchPhaseName(ls.phase));
	else if (!s.connected) ImGui::TextColored(kColDim, "not linked (%s)", s.status.empty() ? "idle" : s.status.c_str());
	else {
		ImGui::TextColored(kColOk, "linked pid %u", (unsigned)s.pid);
		ImGui::SameLine();
		if (s.capsUnknown) ImGui::TextColored(kColWarn, "pchost: link rev 1 (no Authoring)");
		else if (s.haveCaps)
			ImGui::TextColored(p.leverTableOk ? kColOk : kColBad, "pchost %s  authoring rev %u  lever table %s", s.caps.build,
			                   (unsigned)s.caps.revision, p.leverTableOk ? "ok" : "DIFFERS");
	}
	// phase / setup / session
	if (s.haveSetup) {
		const wire::SetupState& st = s.setup;
		ImGui::Text("Phase %s", wire::PhaseName(st.phase));
		ImGui::SameLine();
		const bool busy = st.authState == (uint8_t)wire::AuthState::ApplyingHot || st.authState == (uint8_t)wire::AuthState::Rebuilding;
		ImGui::TextColored(st.authState == (uint8_t)wire::AuthState::Failed ? kColBad : busy ? kColWarn : kColOk, "  Setup: %s%s%s",
		                   wire::AuthStateName(st.authState), st.message[0] ? " - " : "", st.message);
		if (st.lastPath && st.authState == (uint8_t)wire::AuthState::Ready) {
			ImGui::SameLine();
			ImGui::TextDisabled("(%s, %u ms)", st.lastPath == 1 ? "hot" : "cold", (unsigned)st.lastDurationMs);
		}
	}
	if (!p.banner.empty()) {
		const ImVec4 c = p.severity >= 3 ? kColBad : p.severity == 2 ? kColWarn : kColDim;
		ImGui::TextColored(c, "%s", p.banner.c_str());
		if (p.sessionLocked) {
			ImGui::SameLine();
			if (ImGui::SmallButton(a.unlocked ? "Lock again" : "Unlock (edits apply after the session)")) a.unlocked = !a.unlocked;
		}
	}
	// a crash / an exit: offer to relaunch the same setup (§8.9)
	if (ls.phase == LaunchPhase::Exited && !a.crashDismissed) {
		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.25f, 0.08f, 0.08f, 1.0f));
		ImGui::BeginChild("##crash", ImVec2(0, 150), ImGuiChildFlags_Borders);
		ImGui::TextColored(ls.crashed ? kColBad : kColWarn, "%s", ls.message.c_str());
		if (ImGui::Button("Relaunch with the same setup")) {
			wire::MatchSetup w{};
			if (FromHex(ls.setupHex, w)) { const std::string st = a.setup.style; a.setup = FromWire(w); a.setup.style = st; }
			a.launcher.Forget();
			LaunchGame(host);
		}
		ImGui::SameLine();
		if (ImGui::Button("Show the log")) { a.requestTab = 5; }
		ImGui::SameLine();
		if (ImGui::Button("Dismiss")) { a.crashDismissed = true; a.launcher.Forget(); }
		ImGui::TextDisabled("last lines of pchost_authoring.log:");
		for (const std::string& l : ls.lastLog) ImGui::TextUnformatted(l.c_str());
		ImGui::EndChild();
		ImGui::PopStyleColor();
	}
	if (!a.status.empty()) ImGui::TextWrapped("%s", a.status.c_str());
	for (const tagtune::Warning& w : a.blocked) ImGui::TextColored(kColBad, "  would add: %s", w.Text().c_str());
}

// ---- palette swatches ----
const PalFile* PalOf(const std::string& file1)
{
	static std::map<std::string, std::unique_ptr<PalFile>> cache;
	const std::string key = St().gameDir + "|" + file1;
	auto it = cache.find(key);
	if (it != cache.end()) return it->second.get();
	auto p = std::make_unique<PalFile>();
	if (!p->load((St().gameDir + "\\data\\" + file1 + ".pal").c_str())) p.reset();
	const PalFile* r = p.get();
	cache[key] = std::move(p);
	return r;
}

void Swatch(const std::string& file1, int palette)
{
	const PalFile* p = PalOf(file1);
	if (!p || palette < 0 || palette >= p->count()) return;
	ImDrawList* dl = ImGui::GetWindowDrawList();
	const ImVec2 o = ImGui::GetCursorScreenPos();
	const float sz = ImGui::GetTextLineHeight();
	static const int picks[6] = { 1, 16, 48, 96, 160, 224 };
	for (int k = 0; k < 6; ++k) {
		const uint32_t c = p->colors[(size_t)palette * 256 + picks[k]];   // BGRA
		const ImU32 col = IM_COL32((c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF, 255);
		dl->AddRectFilled(ImVec2(o.x + k * sz * 0.6f, o.y), ImVec2(o.x + (k + 1) * sz * 0.6f, o.y + sz), col);
	}
	ImGui::Dummy(ImVec2(6 * sz * 0.6f, sz));
}

// ---- one pick (character / moon / palette) ----
bool PickRow(int slot, const char* role, bool optional)
{
	AuthoringState& a = St();
	SlotPick& pk = a.setup.slot[slot];
	bool changed = false;
	ImGui::PushID(slot);
	ImGui::TextUnformatted(role);
	ImGui::SameLine(90);
	const RosterChar* cur = pk.Empty() ? nullptr : RosterFor(pk.chara);
	ImGui::SetNextItemWidth(150);
	if (ImGui::BeginCombo("##chara", cur ? cur->name.c_str() : pk.Empty() ? (optional ? "(none: solo)" : "(pick)") : "?", ImGuiComboFlags_HeightLarge)) {
		if (optional && ImGui::Selectable("(none: solo)", pk.Empty())) { pk = {}; changed = true; }
		for (const RosterChar& c : a.roster) {
			const std::string ban = BanReason(c, a.setup.mode);
			std::string label = c.name + (c.Duo() ? "  (duo)" : "") + (!ban.empty() ? "  - banned" : "");
			const bool isSel = !pk.Empty() && pk.chara == c.chara;
			if (ImGui::Selectable(label.c_str(), isSel, ban.empty() ? 0 : ImGuiSelectableFlags_Disabled)) {
				const int moon = pk.Empty() ? 0 : pk.moon;
				pk = { c.chara, moon, 0 };
				changed = true;
			}
			if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
				ImGui::SetTooltip("%s  (%s%s%s)%s%s", c.fullName.c_str(), c.file1.c_str(), c.file2.empty() ? "" : " + ", c.file2.c_str(),
				                  ban.empty() ? "" : "\n", ban.c_str());
			if (isSel) ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}
	if (!pk.Empty() && cur) {
		ImGui::SameLine();
		for (int m = 0; m < 3; ++m) {
			if (m) ImGui::SameLine(0, 2);
			const bool on = pk.moon == m;
			if (on) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.55f, 0.45f, 0.1f, 1.0f));
			if (ImGui::SmallButton(m == 0 ? "C" : m == 1 ? "F" : "H")) { pk.moon = m; changed = true; }
			if (on) ImGui::PopStyleColor();
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s Moon", MoonText(m).c_str());
		}
		ImGui::SameLine();
		const int n = PaletteCount(cur->file1);
		ImGui::SetNextItemWidth(70);
		char pl[16];
		std::snprintf(pl, sizeof pl, "pal %02d", pk.palette);
		if (ImGui::BeginCombo("##pal", pl, ImGuiComboFlags_HeightLarge)) {
			const int count = n > 0 ? n : 36;
			for (int k = 0; k < count; ++k) {
				char l[16];
				std::snprintf(l, sizeof l, "%02d", k);
				if (ImGui::Selectable(l, pk.palette == k)) { pk.palette = k; changed = true; }
				ImGui::SameLine(40);
				Swatch(cur->file1, k);
			}
			ImGui::EndCombo();
		}
		if (n > 0 && pk.palette >= n) { pk.palette = n - 1; changed = true; }
		ImGui::SameLine();
		Swatch(cur->file1, pk.palette);
	}
	ImGui::PopID();
	return changed;
}

const char* kDirNames[5] = { "5", "2", "6", "4", "8" };

void AssistRow(int side)
{
	AuthoringState& a = St();
	ImGui::PushID(100 + side);
	ImGui::TextDisabled("Assists");
	ImGui::SameLine(90);
	for (int d = 0; d < 5; ++d) {
		if (d) ImGui::SameLine();
		ImGui::PushID(d);
		ImGui::TextUnformatted(kDirNames[d]);
		ImGui::SameLine(0, 2);
		ImGui::SetNextItemWidth(72);
		const uint8_t c = a.setup.assist[side][d];
		if (ImGui::BeginCombo("##as", c == 0 ? "tuning" : kAssistMotionChoices[c - 1])) {
			if (ImGui::Selectable("tuning", c == 0)) a.setup.assist[side][d] = 0;
			for (int k = 0; k < kAssistMotionCount; ++k)
				if (ImGui::Selectable(kAssistMotionChoices[k], c == k + 1)) a.setup.assist[side][d] = (uint8_t)(k + 1);
			ImGui::EndCombo();
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("%s+FN1 for this side's partner. A MATCH setting like a CSS pick: never written to the sidecars.\n"
			                  "\"tuning\" = the character's own action (its sidecar, else the default).", kDirNames[d]);
		ImGui::PopID();
	}
	ImGui::PopID();
}

void StageCombo()
{
	AuthoringState& a = St();
	static bg::StageProject proj;
	static std::string projDir;
	if (projDir != a.gameDir) { projDir = a.gameDir; if (!a.gameDir.empty()) proj.Open(a.gameDir, bg::Game::MBAACC); }
	std::string preview = a.setup.stage == 0 ? "keep the current stage" : a.setup.stage < 0 ? "random" : "stage " + std::to_string(a.setup.stage);
	if (a.setup.stage > 0 && proj.IsOpen())
		if (const bg::StageEntry* e = proj.Find(a.setup.stage)) preview = e->Label();
	ImGui::SetNextItemWidth(320);
	if (ImGui::BeginCombo("Stage", preview.c_str(), ImGuiComboFlags_HeightLargest)) {
		if (ImGui::Selectable("keep the current stage", a.setup.stage == 0)) a.setup.stage = 0;
		if (ImGui::Selectable("random (the game's roll)", a.setup.stage < 0)) a.setup.stage = -1;
		if (proj.IsOpen())
			for (const bg::StageEntry& e : proj.Entries()) {
				if (!e.listed) continue;
				std::string l = e.Label();
				if (e.excludedTraining) l += "  (not in Training)";
				if (e.datPath.empty()) l += "  (no .dat)";
				if (ImGui::Selectable(l.c_str(), a.setup.stage == e.id, e.datPath.empty() ? ImGuiSelectableFlags_Disabled : 0)) a.setup.stage = e.id;
			}
		else ImGui::TextDisabled("no Bg\\BgList.ini in the game folder");
		ImGui::EndCombo();
	}
}

void SetupTab(HostContext& host, const gamelink::Snapshot& s, const LinkPolicy& p)
{
	AuthoringState& a = St();
	// mode / scene / style / rules
	int mode = (int)a.setup.mode;
	ImGui::TextUnformatted("Mode");
	ImGui::SameLine(90);
	bool modeChanged = ImGui::RadioButton("1v1", &mode, 0);
	ImGui::SameLine();
	modeChanged |= ImGui::RadioButton("TAG", &mode, 1);
	ImGui::SameLine();
	modeChanged |= ImGui::RadioButton("TEAM", &mode, 2);
	if (modeChanged) {
		a.setup.mode = (Mode)mode;
		if (a.setup.mode == Mode::Versus) { a.setup.slot[2] = {}; a.setup.slot[3] = {}; }
		if (a.setup.mode != Mode::Versus && a.setup.scene == (uint8_t)wire::Scene::Training) a.setup.scene = (uint8_t)wire::Scene::Auto;
		a.setup.assists = a.setup.mode == Mode::Tag;
	}
	ImGui::SameLine(0, 30);
	ImGui::SetNextItemWidth(150);
	const char* scenes[] = { "Auto", "Training", "Authoring VS" };
	if (ImGui::BeginCombo("Scene", scenes[a.setup.scene < 3 ? a.setup.scene : 0])) {
		for (int k = 0; k < 3; ++k) {
			const bool dis = k == 1 && a.setup.mode != Mode::Versus;
			if (ImGui::Selectable(scenes[k], a.setup.scene == k, dis ? ImGuiSelectableFlags_Disabled : 0)) a.setup.scene = (uint8_t)k;
			if (dis && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
				ImGui::SetTooltip("Training is 1v1 only for now: TAG / TEAM in native Training breaks the game's == 0x1010 checks\n"
				                  "(gap G3). They use Authoring VS: infinite timer, an endless round, P2 = the second player.");
		}
		ImGui::EndCombo();
	}
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("Auto = Training for 1v1, Authoring VS for TAG / TEAM");
	// style (a TUNING edit: global.ini active_style)
	ImGui::TextUnformatted("Style");
	ImGui::SameLine(90);
	{
		const tagtune::GlobalResolution g = a.ws.Global();
		const std::vector<std::string> names = tagtune::SidecarStyleNames(a.ws.Set());
		ImGui::SetNextItemWidth(150);
		const bool can = Sink().CanEdit();
		ImGui::BeginDisabled(!can);
		if (ImGui::BeginCombo("##style", g.style.empty() ? "(defaults)" : g.style.c_str())) {
			for (const std::string& n : names) {
				const tagtune::BuiltinStyle* b = tagtune::FindBuiltinStyle(n);
				if (ImGui::Selectable(n.c_str(), tagtune::ieq(n, g.style))) {
					Sink().Begin("style " + n);
					tagtune::SelectStyle(a.ws.Doc(tagtune::GlobalDoc(a.editShipped ? tagtune::Layer::Shipped : tagtune::Layer::Local)).ini, n, false);
					a.setup.style = n;
					Sink().Edited();
					Sink().End();
				}
				if (b && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", b->desc);
			}
			ImGui::EndCombo();
		}
		ImGui::EndDisabled();
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
			ImGui::SetTooltip("Edits global.ini active_style (a tuning edit: undoable, applied at once). The style layer sits under\nthe global overrides and every character / moon value.");
	}
	ImGui::SameLine(0, 30);
	ImGui::SetNextItemWidth(130);
	const char* ko[] = { "oneDown", "allDown", "the tuning's" };
	int koi = a.setup.koRule == 0 ? 0 : a.setup.koRule == 1 ? 1 : 2;
	if (ImGui::Combo("KO rule", &koi, ko, 3)) a.setup.koRule = koi == 2 ? 0xFF : koi;
	ImGui::SameLine();
	ImGui::SetNextItemWidth(110);
	const char* timers[] = { "infinite", "slow (1)", "normal (2)", "fast (4)", "scene default" };
	const int tv[] = { 0, 1, 2, 4, 0xFF };
	int ti = 4;
	for (int k = 0; k < 5; ++k) if (tv[k] == a.setup.timer) ti = k;
	if (ImGui::Combo("Timer", &ti, timers, 5)) a.setup.timer = tv[ti];
	if (a.setup.mode == Mode::Tag) {
		ImGui::SameLine();
		ImGui::Checkbox("Assists this match", &a.setup.assists);
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("kSetupAssists: the CSS \"TAG Battle\" vs \"no assists\" preset (a match setting).");
	}
	ImGui::Dummy(ImVec2(0, 0));
	ImGui::SameLine(90);
	StageCombo();
	ImGui::SameLine();
	if (ImGui::Button("Browse...")) { a.wantStagePick = true; if (host.browseStage) host.browseStage(); }
	if (a.wantStagePick) { ImGui::SameLine(); ImGui::TextDisabled("(pick in the Stage Browser: \"Use for the Authoring setup\")"); }
	ImGui::SameLine();
	ImGui::Checkbox("keep BGM", &a.setup.keepBgm);
	ImGui::Separator();
	// the teams
	const bool partners = a.setup.mode != Mode::Versus;
	if (ImGui::BeginTable("##teams", 2, ImGuiTableFlags_SizingStretchSame)) {
		for (int side = 0; side < 2; ++side) {
			ImGui::TableNextColumn();
			ImGui::SeparatorText(side == 0 ? "P1 TEAM" : "P2 TEAM");
			PickRow(EngineSlot(side, 0), "Point", false);
			if (partners) PickRow(EngineSlot(side, 1), "Partner", a.setup.mode == Mode::Tag);
			if (a.setup.mode == Mode::Tag) AssistRow(side);
		}
		ImGui::EndTable();
	}
	ImGui::Separator();
	// validation + Load in game
	const std::vector<Problem> probs = ValidateNow(s);
	bool needsRestart = false;
	for (const Problem& pr : probs) {
		ImGui::TextColored(pr.status == (int16_t)wire::Status::NeedsRestart ? kColWarn : kColBad, "%s", pr.msg.c_str());
		needsRestart |= pr.status == (int16_t)wire::Status::NeedsRestart;
	}
	const bool onlyRestart = needsRestart && probs.size() == 1;
	ImGui::BeginDisabled(!p.canLoadInGame || !probs.empty());
	if (ImGui::Button("Load in game", ImVec2(160, 0))) LoadInGame(host, s);
	ImGui::EndDisabled();
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
		if (!s.connected) ImGui::SetTooltip("Not linked: Launch (header) starts the game with this setup, or Attach to a running one.");
		else if (!p.canLoadInGame) ImGui::SetTooltip("%s", p.setupBusy ? "a setup is still being applied" : p.banner.c_str());
		else ImGui::SetTooltip("SetMatchSetup: the hot path (~1 s) when only picks / stage change in an authoring battle, else the\ncold path through the character select (~3-10 s). The characters open as editor tabs right away.");
	}
	if (onlyRestart) {
		ImGui::SameLine();
		if (ImGui::Button("Relaunch with 4 players")) { a.launcher.KillOwn(); LaunchGame(host); }
	}
	ImGui::SameLine();
	ImGui::Checkbox("open characters in the editor", &a.openChars);
	ImGui::SameLine();
	ImGui::Checkbox("re-read tuning first", &a.reReadFirst);
	ImGui::SameLine();
	if (ImGui::Button("Open characters now")) OpenCharacters(host);
	for (const std::string& w : a.charWarnings) ImGui::TextColored(kColWarn, "%s", w.c_str());
	// the result, and requested != in game
	if (a.loadSeq) {
		wire::Reply r{};
		if (Link().PeekReply(a.loadSeq, r)) {
			a.loadResult = std::string(r.status == 0 ? "READY - " : "FAILED - ") + wire::StatusName(r.status) + ": " + r.message;
			a.loadSeq = 0;
		} else if (s.haveSetup) {
			a.loadResult = std::string("running: ") + wire::AuthStateName(s.setup.authState) + " - " + s.setup.message;
		}
	}
	if (!a.loadResult.empty()) ImGui::TextWrapped("last: %s", a.loadResult.c_str());
	if (s.haveSetup && s.setup.phase == (uint8_t)wire::Phase::Battle && s.setup.authState == (uint8_t)wire::AuthState::Ready) {
		const Setup inForce = FromWire(s.setup.inForce);
		std::string diff;
		for (int i = 0; i < 4; ++i)
			if (inForce.slot[i].chara != a.loadRequested.slot[i].chara || inForce.slot[i].moon != a.loadRequested.slot[i].moon)
				diff += std::string(diff.empty() ? "" : ", ") + SlotRoleName(i);
		if (!diff.empty() && a.loadRequested.slot[0].chara >= 0) {
			ImGui::TextColored(kColWarn, "requested != in game: %s", diff.c_str());
			ImGui::SameLine();
			if (ImGui::SmallButton("Adopt game's")) { const std::string st = a.setup.style; a.setup = inForce; a.setup.style = st; a.loadRequested = inForce; }
			ImGui::SameLine();
			if (ImGui::SmallButton("Re-send")) LoadInGame(host, s);
		}
		ImGui::TextDisabled("in game: %s", Describe(inForce, a.roster).c_str());
	}
	ImGui::Separator();
	// save as a named setup
	ImGui::SetNextItemWidth(200);
	ImGui::InputTextWithHint("##savename", "setup name", a.saveName, sizeof a.saveName);
	ImGui::SameLine();
	ImGui::BeginDisabled(!a.saveName[0]);
	if (ImGui::Button("Save setup")) {
		a.setup.style = a.ws.Global().style;
		a.lib.Save(a.setup, a.saveName);
		SaveSettings();
		a.status = std::string("saved setup '") + a.saveName + "'";
	}
	ImGui::EndDisabled();
	ImGui::SameLine();
	ImGui::TextDisabled("%s", Describe(a.setup, a.roster).c_str());
}

void SetupsTab(HostContext& host, const gamelink::Snapshot& s, const LinkPolicy& p)
{
	AuthoringState& a = St();
	(void)host; (void)s; (void)p;
	auto row = [&](const Setup& x, bool named, int idx) -> int {
		ImGui::PushID(idx + (named ? 0 : 1000));
		int action = 0;
		if (ImGui::SmallButton("Use")) action = 1;
		ImGui::SameLine();
		if (named) { if (ImGui::SmallButton("Delete")) action = 2; ImGui::SameLine(); }
		if (named) ImGui::Text("%-20s", x.name.c_str()), ImGui::SameLine();
		ImGui::TextUnformatted(Describe(x, a.roster).c_str());
		if (!x.style.empty()) { ImGui::SameLine(); ImGui::TextDisabled("style %s", x.style.c_str()); }
		ImGui::PopID();
		return action;
	};
	ImGui::SeparatorText("Saved setups");
	if (a.lib.named.empty()) ImGui::TextDisabled("none yet: Setup > Save setup");
	for (size_t i = 0; i < a.lib.named.size(); ++i) {
		const int act = row(a.lib.named[i], true, (int)i);
		if (act == 1) { a.setup = a.lib.named[i]; a.requestTab = 0; a.status = "setup '" + a.setup.name + "' loaded into the Setup tab"; }
		if (act == 2) { const std::string n = a.lib.named[i].name; a.lib.Remove(n); SaveSettings(); break; }
	}
	ImGui::SeparatorText("Recent");
	if (a.lib.recent.empty()) ImGui::TextDisabled("every Load in game / Launch lands here");
	for (size_t i = 0; i < a.lib.recent.size(); ++i)
		if (row(a.lib.recent[i], false, (int)i) == 1) { const std::string n = a.setup.name; a.setup = a.lib.recent[i]; a.requestTab = 0; }
	ImGui::SeparatorText("Setup as hex");
	ImGui::TextDisabled("PCHOST_MBAACC_AUTHORING_SETUP / game_link_cli setup-hex:");
	std::string hex = ToHex(ToWire(a.setup));
	ImGui::SetNextItemWidth(-1);
	ImGui::InputText("##hex", hex.data(), hex.size() + 1, ImGuiInputTextFlags_ReadOnly);
}

void Tick(HostContext& host, const gamelink::Snapshot& s)
{
	AuthoringState& a = St();
	// the roster: the game's when it sent one, else the mirror
	a.rosterFromGame = s.rosterComplete && !s.roster.empty();
	a.roster = a.rosterFromGame ? RosterFromLink(s.roster) : MirrorRoster(!FileExists(a.gameDir + "\\0002.p"));
	// a launch: link as soon as the game is up; open the characters when the launch setup is Ready
	const LaunchState ls = a.launcher.Get();
	if (ls.phase == LaunchPhase::Running && ls.gamePid && (!s.wantConnected || s.targetPid != ls.gamePid)) {
		Link().SetTargetPid(ls.gamePid);
		Link().Connect();
	}
	if (a.openAfterReady && s.haveSetup && s.setup.authState == (uint8_t)wire::AuthState::Ready && s.setup.setupCount > 0) {
		a.openAfterReady = false;
		a.loadRequested = FromWire(s.setup.requested);
		OpenCharacters(host);
		a.status = "the launch setup is ready: " + Describe(FromWire(s.setup.inForce), a.roster);
	}
	// sidecar external changes (1 s)
	const uint64_t now = NowMs();
	if (now - a.lastPollMs > 1000) {
		a.lastPollMs = now;
		EnsureWorkspace();
		for (const tagtune::DocId& d : a.ws.PollExternal()) {
			const tagtune::SidecarDoc* doc = a.ws.Find(d);
			if (doc && !doc->externalChange) a.status = d.Label() + " changed on disk: reloaded";
		}
	}
}

} // namespace

// ================== shared helpers ==================

AuthoringState& St() { static AuthoringState s; return s; }
gamelink::Client& Link() { return gamelink::SharedClient(); }
EditSink& Sink() { return g_sink; }
LinkPolicy Policy(const gamelink::Snapshot& s) { return DecidePolicy(s, St().unlocked); }

std::string TagRoot()
{
	const AuthoringState& a = St();
	if (!a.tagRootOverride.empty()) return a.tagRootOverride;
	return a.gameDir.empty() ? std::string() : a.gameDir + "\\povertycaster\\tag";
}

void EnsureWorkspace()
{
	AuthoringState& a = St();
	const std::string root = TagRoot();
	if (root == a.wsRoot && (a.ws.IsOpen() || root.empty())) return;
	if (a.ws.AnyDirty()) {
		const tagtune::SidecarWorkspace::SaveResult r = a.ws.SaveDirty();
		if (!r.ok) { a.status = "the game folder changed but the open edits could not be saved: " + r.message; return; }
	}
	a.wsRoot = root;
	a.ws.Close();
	a.history.Clear();
	a.checkpoints.clear();
	for (AbSlot& s : a.ab) s = AbSlot{};
	a.abActive = -1;
	tagtune::ResetBakMemory();
	if (!root.empty()) {
		a.ws.Open(root);
		TakeCheckpoint("window open");
	}
}

void SaveAndApply(const std::string& why)
{
	AuthoringState& a = St();
	const gamelink::Snapshot s = Link().Get();
	const LinkPolicy p = Policy(s);
	if (!p.canWriteFiles) { a.status = "NOT saved: " + p.banner; return; }
	const tagtune::SidecarWorkspace::SaveResult r = a.ws.SaveDirty();
	a.blocked = r.blocked;
	if (!r.ok) { a.status = r.message; return; }
	if (r.nothing) return;
	std::string line = why + ": " + r.message;
	if (p.canApplyTuning) {
		a.applySeq = Link().ApplyTuning(wire::kFlagQueryAfter);
		line += p.hostBetweenRounds ? "; applies to both players at the next round" : "; the game re-reads it now";
	} else if (!s.connected) {
		line += "; not linked (the game re-reads the files within a second when it runs)";
	} else if (p.sessionLocked) {
		line += "; the session plays the host's tuning: it applies after the session";
	}
	a.status = line;
}

void TakeCheckpoint(const std::string& label)
{
	AuthoringState& a = St();
	if (!a.ws.IsOpen()) return;
	const tagtune::SidecarWorkspace::Snapshot snap = a.ws.Take();
	if (!a.checkpoints.empty() && a.checkpoints.back().snap == snap) return;
	a.checkpoints.push_back({ label, NowText(), snap });
	if (a.checkpoints.size() > 30) a.checkpoints.erase(a.checkpoints.begin());
}

std::string NowText()
{
	SYSTEMTIME st;
	GetLocalTime(&st);
	char b[16];
	std::snprintf(b, sizeof b, "%02u:%02u:%02u", st.wHour, st.wMinute, st.wSecond);
	return b;
}

std::string MoonText(int m) { return m == 0 ? "Crescent" : m == 1 ? "Full" : m == 2 ? "Half" : "?"; }

const RosterChar* RosterFor(int chara) { return FindChara(St().roster, chara); }

std::string CharLabel(const std::string& file)
{
	const RosterChar* c = FindFile(St().roster, file);
	return c ? c->name + " (" + file + ")" : file;
}

int PaletteCount(const std::string& file1)
{
	static std::map<std::string, int> cache;
	const std::string key = St().gameDir + "|" + file1;
	auto it = cache.find(key);
	if (it != cache.end()) return it->second;
	const int n = PaletteCountOf(St().gameDir + "\\data\\" + file1 + ".pal");
	cache[key] = n;
	return n;
}

std::string DataTxt(const std::string& file, int moon) { return St().gameDir + "\\data\\" + file + "_" + std::to_string(moon) + ".txt"; }
std::string CommandsTxt(const std::string& file, int moon) { return St().gameDir + "\\data\\" + file + "_" + std::to_string(moon) + "_c.txt"; }

// ================== public API ==================

void Open(const std::string& tab)
{
	showWindow = true;
	for (int k = 0; k < kTabCount; ++k) if (tagtune::ieq(tab, kTabNames[k])) St().requestTab = k;
}

void ApplyStartup(const StartupOptions& o)
{
	AuthoringState& a = St();
	if (!a.loaded) LoadSettings();
	if (!o.gameDir.empty()) a.gameDir = o.gameDir;
	if (!o.tagRoot.empty()) a.tagRootOverride = o.tagRoot;
	if (!o.setupHex.empty()) { wire::MatchSetup w{}; if (FromHex(o.setupHex, w)) a.setup = FromWire(w); }
	if (!o.charFile.empty()) {
		int moon = 0;
		for (int i = 0; i < 4; ++i) {
			const RosterChar* c = FindChara(MirrorRoster(true), a.setup.slot[i].chara);
			if (c && c->file1 == o.charFile) { moon = a.setup.slot[i].moon; break; }
		}
		a.tuningView = o.charFile + ":" + std::to_string(moon);
		a.forceViewFrames = 3;
	}
	if (o.subTab == "global") { a.tuningView = "global"; a.forceViewFrames = 3; }
	if (o.layer == "all") a.layer = -1;
	else if (o.layer == "c") a.layer = 0;
	else if (o.layer == "f") a.layer = 1;
	else if (o.layer == "h") a.layer = 2;
	a.unlocked = o.sessionUnlock;
	Open(o.tab.empty() ? "Setup" : o.tab);
	EnsureWorkspace();
	if (o.pid) Link().SetTargetPid(o.pid);
	if (o.link) Link().Connect();
	if (o.abDemo == "1" && a.ws.IsOpen()) {
		a.ab[0] = { true, "A (as loaded)", NowText(), a.ws.Take() };
		tagtune::SidecarDoc& d = a.ws.Doc(tagtune::CharDoc(tagtune::Layer::Local, "shiki"));
		a.ab[1] = a.ab[0];
		tagtune::SetDocLever(d.ini, tagtune::DocKind::CharShared, tagtune::FindLever("cancelWindowTicks"), 2);
		a.ab[1].label = "B (cancelWindowTicks 2)";
		a.ab[1].snap = a.ws.Take();
		a.ws.Restore(a.ab[0].snap);   // back to A in memory; nothing is saved (captures never write)
		a.abActive = 0;
	}
}

void StartupLoadInGame()
{
	AuthoringState& a = St();
	a.requestTab = 0;
	a.loadRequested = a.setup;
	a.loadSeq = Link().SetMatchSetupWhenConnected(ToWire(a.setup));
	a.loadResult = "sent (startup): " + Describe(a.setup, a.roster.empty() ? MirrorRoster(true) : a.roster);
}

bool WantsStagePick() { return St().wantStagePick; }
void StagePicked(int id)
{
	AuthoringState& a = St();
	a.setup.stage = id;
	a.wantStagePick = false;
	a.status = "stage " + std::to_string(id) + " picked from the Stage Browser";
}

bool HasFocus() { return showWindow && St().focused; }

bool UndoRedo(bool redo)
{
	AuthoringState& a = St();
	if (a.gestureOpen) return false;
	const std::string label = redo ? a.history.RedoLabel() : a.history.UndoLabel();
	const std::vector<tagtune::DocId> t = redo ? a.history.Redo(a.ws) : a.history.Undo(a.ws);
	if (t.empty()) return false;
	SaveAndApply(std::string(redo ? "redo " : "undo ") + label);
	return true;
}

std::string UndoLabel(bool redo) { return redo ? St().history.RedoLabel() : St().history.UndoLabel(); }

std::string TabBadge(const std::string& txtPath, bool* isPoint)
{
	if (isPoint) *isPoint = false;
	if (!showWindow || txtPath.empty()) return {};
	const AuthoringState& a = St();
	const std::string leaf = Lower(fs::u8path(txtPath).filename().u8string());
	std::string badge;
	int slotFound = -1;
	for (const OpenTarget& t : FilesToOpen(a.setup, a.roster.empty() ? MirrorRoster(true) : a.roster, a.gameDir))
		if (leaf == Lower(t.file + "_" + std::to_string(t.moon) + ".txt")) { badge = SlotRoleName(t.slot); slotFound = t.slot; break; }
	if (slotFound < 0) return {};
	const gamelink::Snapshot s = Link().Get();
	if (isPoint && s.haveState && s.state.tagLive) {
		for (int team = 0; team < 2; ++team) {
			const int pt = s.state.teamActive[team];
			if (pt >= 0 && pt < 4 && s.state.actors[pt].exists &&
			    leaf.compare(0, strnlen(s.state.actors[pt].file, 28), Lower(std::string(s.state.actors[pt].file, strnlen(s.state.actors[pt].file, 28)))) == 0 &&
			    (pt == slotFound || pt % 2 == slotFound % 2))
				*isPoint = pt == slotFound;
		}
	} else if (isPoint) {
		*isPoint = slotFound < 2;
	}
	return badge;
}

void SaveSettings()
{
	AuthoringState& a = St();
	if (!a.loaded) return;
	std::string t = "[Authoring]\ngameDir=" + a.gameDir + "\nmbacDir=" + a.mbacDir + "\nopenChars=" + (a.openChars ? "1" : "0") +
	                "\nreReadFirst=" + (a.reReadFirst ? "1" : "0") + "\nliveApply=" + (a.liveApply ? "1" : "0") +
	                "\nsetup=" + ToHex(ToWire(a.setup)) + "\nstyle=" + a.setup.style + "\n";
	t += a.lib.Serialize();
	tagtune::WriteFileAtomic(SettingsPath(), t, false, nullptr);
}

void Draw(HostContext& host)
{
	AuthoringState& a = St();
	if (!a.loaded) LoadSettings();
	gamelink::Client& c = Link();
	if (!showWindow) { a.focused = false; c.SetAuthoringPoll(false, false); return; }
	EnsureWorkspace();
	const gamelink::Snapshot s = c.Get();
	c.SetAuthoringPoll(true, a.tab == 2 || a.tab == 4);
	c.SetTagQuery(a.tab == 4 || a.follow);
	c.SetPollHz(a.follow ? 30 : 10);
	Tick(host, s);
	FollowTick(host, s);
	const LinkPolicy p = Policy(s);

	const ImGuiViewport* vp = ImGui::GetMainViewport();
	if (a.maximize) {
		ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + 20, vp->WorkPos.y + 30), ImGuiCond_Always);
		ImGui::SetNextWindowSize(ImVec2(vp->WorkSize.x - 40, vp->WorkSize.y - 50), ImGuiCond_Always);
	} else {
		// inside the main window, so it stays part of it (a window larger than the main viewport becomes its own OS window)
		ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + 30, vp->WorkPos.y + 40), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(std::min(1240.0f, vp->WorkSize.x - 60), std::min(900.0f, vp->WorkSize.y - 60)), ImGuiCond_FirstUseEver);
	}
	if (!ImGui::Begin("Authoring (MBAACC)###authoring", &showWindow, ImGuiWindowFlags_MenuBar)) {
		a.focused = false;
		ImGui::End();
		return;
	}
	a.focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
	if (ImGui::BeginMenuBar()) {
		if (ImGui::BeginMenu("Workspace")) {
			ImGui::MenuItem("Fill the main window", nullptr, &a.maximize);
			ImGui::MenuItem("Live apply (save + re-read on every edit)", nullptr, &a.liveApply);
			ImGui::MenuItem("Open characters on Load in game", nullptr, &a.openChars);
			ImGui::MenuItem("Edit the SHIPPED defaults (not the local overlay)", nullptr, &a.editShipped);
			ImGui::Separator();
			if (ImGui::MenuItem(("Undo " + a.history.UndoLabel()).c_str(), "Ctrl+Z", false, a.history.CanUndo())) UndoRedo(false);
			if (ImGui::MenuItem(("Redo " + a.history.RedoLabel()).c_str(), "Ctrl+Y", false, a.history.CanRedo())) UndoRedo(true);
			ImGui::EndMenu();
		}
		ImGui::EndMenuBar();
	}
	Header(host, s, p);
	ImGui::Separator();
	if (ImGui::BeginTabBar("##authoringtabs")) {
		for (int k = 0; k < kTabCount; ++k) {
			const ImGuiTabItemFlags fl = a.requestTab == k ? ImGuiTabItemFlags_SetSelected : 0;
			if (!ImGui::BeginTabItem(kTabNames[k], nullptr, fl)) continue;
			a.tab = k;
			ImGui::BeginChild("##tabbody", ImVec2(0, 0), ImGuiChildFlags_None);
			switch (k) {
			case 0: SetupTab(host, s, p); break;
			case 1: SetupsTab(host, s, p); break;
			case 2: DrawTuningTab(host, s, p); break;
			case 3: {
				int chars[4];
				for (int i = 0; i < 4; ++i) chars[i] = a.setup.slot[i].chara;
				DrawHudLayoutEditor(Sink(), a.gameDir, chars);
				break;
			}
			case 4: DrawLiveTab(host, s, p); break;
			case 5: DrawLogTab(s); break;
			}
			ImGui::EndChild();
			ImGui::EndTabItem();
		}
		a.requestTab = -1;
		ImGui::EndTabBar();
	}
	ImGui::End();
}

} // namespace authoring
