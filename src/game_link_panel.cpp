// Game Link window — see game_link_panel.h.
#include "game_link_panel.h"
#include "game_link_api.h"
#include "framestate.h"
#include "background/bg_info.h"

#include <windows.h>

#include <imgui.h>

#include <cstdio>
#include <cstring>

namespace gamelink {

bool showPanel = false;

Client& SharedClient()
{
	static Client c;   // the worker thread starts on first use, i.e. when the panel is first opened
	return c;
}

namespace {

bool g_autoReload = true;
bool g_follow = false;
bool g_jump = false;
int  g_followSlot = 0;
int  g_lastFollowPattern = -1, g_lastFollowFrame = -1;
int  g_setChara[4] = { -1, -1, -1, -1 }, g_setMoon[4] = { 0, 0, 0, 0 }, g_setPal[4] = { 0, 0, 0, 0 };
bool g_forceReload = false;
std::string g_pushResult;
// stage section
bool g_autoReloadStage = true;
bool g_stageList = false, g_keepBgm = false;
int  g_stagePick = -1;
uint32_t g_stageListPid = 0;
bg::StageList g_gameStageList;   // the game's own Bg\BgList.ini, for names (the wire only carries validity)

} // namespace

// The game's folder, from its pid (the link knows the pid; the editor may have opened a stage from anywhere).
std::string GameDirOf(uint32_t pid)
{
	HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
	if (!h) return {};
	char buf[MAX_PATH];
	DWORD n = sizeof buf;
	std::string dir;
	if (QueryFullProcessImageNameA(h, 0, buf, &n)) {
		dir.assign(buf, n);
		const size_t b = dir.find_last_of("\\/");
		dir = b == std::string::npos ? std::string() : dir.substr(0, b);
	}
	CloseHandle(h);
	return dir;
}

namespace {

std::string StageLabel(int id)
{
	for (const auto& e : g_gameStageList.entries)
		if (e.index == id) return std::to_string(id) + " " + e.dataFile;
	return std::to_string(id);
}

void StageSection(Client& c, const Snapshot& s, const EditorContext& ctx)
{
	ImGui::SeparatorText("Stage");
	if (s.stageUnsupported) {
		ImGui::TextDisabled("The game's pchost.dll predates the stage ops (rebuild PovertyCaster mbaacc/stage-link).");
		return;
	}
	if (!s.haveStage) { ImGui::TextDisabled("waiting for the game's stage state..."); return; }
	if (s.pid != g_stageListPid) {
		g_stageListPid = s.pid;
		const std::string dir = GameDirOf(s.pid);
		if (dir.empty() || !g_gameStageList.Load(dir + "\\Bg\\BgList.ini")) g_gameStageList = bg::StageList{};
	}
	const wire::Stage& st = s.stage;
	ImGui::Text("On screen: %s  (selected %d, BGM %d, stage ops %u)", st.loaded > 0 ? StageLabel(st.loaded).c_str() : "none",
	            st.selected, st.bgmId, st.stageLoads);
	if (!st.allowed)
		ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.3f, 1.0f), "Stage ops refused right now (needs an offline battle).");
	if (g_stagePick < 1) g_stagePick = st.loaded;
	ImGui::SetNextItemWidth(180);
	if (ImGui::BeginCombo("##gstage", StageLabel(g_stagePick).c_str())) {
		for (int i = 1; i < 100; ++i) {
			if (!st.IsValid(i)) continue;
			std::string label = StageLabel(i);
			if (i == st.loaded) label += "  (on screen)";
			if (ImGui::Selectable(label.c_str(), i == g_stagePick)) g_stagePick = i;
		}
		ImGui::EndCombo();
	}
	const uint8_t fl = (uint8_t)((g_stageList ? wire::kFlagStageList : 0) | (g_keepBgm ? wire::kFlagKeepBgm : 0));
	ImGui::SameLine();
	if (ImGui::Button("Set stage")) c.SetStage(g_stagePick, fl);
	ImGui::SameLine();
	if (ImGui::Button("Reload stage")) c.ReloadStage(fl);
	if (ctx.openStageIndex > 0) {
		ImGui::SameLine();
		char b[48];
		std::snprintf(b, sizeof b, "Show open stage (%d)", ctx.openStageIndex);
		if (ImGui::Button(b)) c.SetStage(ctx.openStageIndex, fl);
	}
	ImGui::Checkbox("re-read BgList.ini", &g_stageList);
	ImGui::SameLine();
	ImGui::Checkbox("keep BGM", &g_keepBgm);
	ImGui::SameLine();
	ImGui::Checkbox("Auto-reload stage on save", &g_autoReloadStage);
	ImGui::SameLine();
	ImGui::TextDisabled("(%zu files)", s.watchedStage);
}

const char* SceneName(uint16_t s) { return s == 1 ? "battle" : s == 20 ? "character select" : "menu/other"; }

void SlotTable(const Snapshot& s)
{
	if (!ImGui::BeginTable("slots", 10, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit))
		return;
	const char* cols[] = { "slot", "file", "chara", "moon", "pal", "pattern", "frame", "ticks", "x,y", "tag" };
	for (const char* c : cols) ImGui::TableSetupColumn(c);
	ImGui::TableHeadersRow();
	for (int i = 0; i < 4; ++i) {
		const wire::Actor& a = s.state.actors[i];
		ImGui::TableNextRow();
		ImGui::TableNextColumn(); ImGui::Text("P%d", i + 1);
		if (!a.exists) { ImGui::TableNextColumn(); ImGui::TextDisabled("(empty)"); continue; }
		ImGui::TableNextColumn(); ImGui::TextUnformatted(a.file);
		ImGui::TableNextColumn(); ImGui::Text("%d", a.chara);
		ImGui::TableNextColumn(); ImGui::Text("%d", a.moon);
		ImGui::TableNextColumn(); ImGui::Text("%d", a.palette);
		ImGui::TableNextColumn(); ImGui::Text("%d", a.pattern);
		ImGui::TableNextColumn(); ImGui::Text("%d", a.frame);
		ImGui::TableNextColumn(); ImGui::Text("%d/%d", a.frameTicks, a.patternTicks);
		ImGui::TableNextColumn(); ImGui::Text("%d,%d", a.x, a.y);
		ImGui::TableNextColumn();
		if (a.tagFlag) ImGui::TextDisabled("reserve");
		else ImGui::Text("team %d", a.team);
		if (a.partnerSlot < 4) { ImGui::SameLine(); ImGui::TextDisabled("+P%d", a.partnerSlot + 1); }
	}
	ImGui::EndTable();
}

void FollowSection(const Snapshot& s, EditorContext& ctx)
{
	ImGui::Checkbox("Follow game slot", &g_follow);
	ImGui::SameLine();
	ImGui::SetNextItemWidth(70);
	const char* slots[] = { "P1", "P2", "P3", "P4" };
	ImGui::Combo("##followslot", &g_followSlot, slots, 4);
	ImGui::SameLine();
	ImGui::Checkbox("Jump editor to it", &g_jump);
	if (!g_follow || !s.haveState) return;
	const wire::Actor& a = s.state.actors[g_followSlot];
	if (!a.exists) { ImGui::TextDisabled("P%d is empty", g_followSlot + 1); return; }
	ImGui::Text("P%d %s: pattern %d  frame %d  (tick %d in frame, %d in pattern)", g_followSlot + 1, a.file, a.pattern,
	            a.frame, a.frameTicks, a.patternTicks);
	const bool matches = !ctx.activeKey.empty() &&
		(SlotMaskForFile(ctx.activeKey, s.state) & (1u << g_followSlot)) != 0;
	if (!matches)
		ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), "The active character (%s) is not what P%d loaded (%s).",
		                   ctx.activeKey.empty() ? "none" : ctx.activeKey.c_str(), g_followSlot + 1, a.file);
	if (!g_jump || !ctx.activeState || !matches) return;
	if (a.pattern == g_lastFollowPattern && a.frame == g_lastFollowFrame) return;
	g_lastFollowPattern = a.pattern;
	g_lastFollowFrame = a.frame;
	const int patterns = ctx.patternCount ? ctx.patternCount() : 0;
	if (a.pattern < 0 || a.pattern >= patterns) return;
	FrameState& st = *ctx.activeState;
	st.animating = false;
	st.pattern = a.pattern;
	const int frames = ctx.frameCount ? ctx.frameCount(a.pattern) : 0;
	st.frame = frames > 0 ? (a.frame < frames ? a.frame : frames - 1) : 0;
	st.currentTick = 0;
}

void SetCharSection(Client& c, const Snapshot& s)
{
	if (!ImGui::TreeNode("Set character per slot")) return;
	ImGui::TextDisabled("charaselect id / moon (0 C, 1 F, 2 H) / palette. -1 keeps the current value.");
	for (int i = 0; i < 4; ++i) {
		ImGui::PushID(i);
		if (g_setChara[i] < 0 && s.haveState && s.state.actors[i].exists) {
			g_setChara[i] = s.state.actors[i].chara;
			g_setMoon[i] = s.state.actors[i].moon;
			g_setPal[i] = s.state.actors[i].palette;
		}
		ImGui::Text("P%d", i + 1); ImGui::SameLine();
		ImGui::SetNextItemWidth(80); ImGui::InputInt("chara", &g_setChara[i]); ImGui::SameLine();
		ImGui::SetNextItemWidth(80); ImGui::InputInt("moon", &g_setMoon[i]); ImGui::SameLine();
		ImGui::SetNextItemWidth(80); ImGui::InputInt("pal", &g_setPal[i]); ImGui::SameLine();
		if (ImGui::SmallButton("Set + reload"))
			c.SetChar(i, g_setChara[i], g_setMoon[i], g_setPal[i], wire::kFlagReload | (g_forceReload ? wire::kFlagForce : 0));
		ImGui::PopID();
	}
	ImGui::TextDisabled("P3/P4 picks apply to a menu TAG session; with PCHOST_MBAACC_TAG_P3/_P4 the env pick wins.");
	ImGui::TreePop();
}

} // namespace

void StartFollowing(int slot)
{
	showPanel = true;
	g_follow = true;
	g_jump = true;
	g_followSlot = slot;
	SharedClient().Connect();
}

void DrawPanel(EditorContext& ctx)
{
	if (!showPanel) return;
	Client& c = SharedClient();
	c.SetAutoReload(g_autoReload);
	c.SetWatchedFiles(ctx.files);
	c.SetAutoReloadStage(g_autoReloadStage);
	c.SetWatchedStageFiles(ctx.stageFiles);
	c.SetPollHz(g_follow ? 30 : 5);
	const Snapshot s = c.Get();

	ImGui::SetNextWindowSize(ImVec2(640, 520), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Game Link (MBAACC)", &showPanel)) { ImGui::End(); return; }

	if (!s.wantConnected) { if (ImGui::Button("Connect")) c.Connect(); }
	else if (ImGui::Button(s.connected ? "Disconnect" : "Stop connecting")) c.Disconnect();
	ImGui::SameLine();
	if (s.connected)
		ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "%s (pid %u)", s.pipe.c_str(), s.pid);
	else
		ImGui::TextDisabled("%s", s.status.empty() ? "not connected" : s.status.c_str());

	if (s.connected && s.haveState) {
		ImGui::Text("Game: %s, frame %u%s, reloads %u", SceneName(s.state.scene), s.state.worldTimer,
		            s.state.tagLive ? ", TAG" : "", s.state.reloadCount);
		if (!s.state.reloadAllowed)
			ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.3f, 1.0f),
			                   "Reload refused right now (needs an offline battle - no netplay, replay or recording).");
	}
	ImGui::Separator();

	ImGui::Checkbox("Auto-reload on save", &g_autoReload);
	ImGui::SameLine();
	ImGui::TextDisabled("(%zu files watched)", s.watched);
	ImGui::SameLine();
	ImGui::Checkbox("skip file check", &g_forceReload);
	if (!s.lastChange.empty()) ImGui::TextWrapped("%s", s.lastChange.c_str());

	if (!s.connected) ImGui::BeginDisabled();
	if (ImGui::Button("Push to game")) {
		// save what is modified, then reload the slots that use the active character (all if none match)
		const bool saved = !ctx.saveAll || ctx.saveAll();
		uint8_t mask = s.haveState && !ctx.activeKey.empty() ? SlotMaskForFile(ctx.activeKey, s.state) : 0;
		const uint16_t seq = c.Reload(mask, g_forceReload ? wire::kFlagForce : 0);
		char b[96];
		std::snprintf(b, sizeof b, "%s; reload #%u sent (mask 0x%X)", saved ? "saved" : "SAVE FAILED", seq, mask);
		g_pushResult = b;
	}
	ImGui::SameLine();
	if (ImGui::Button("Reload all")) c.Reload(0, g_forceReload ? wire::kFlagForce : 0);
	ImGui::SameLine();
	if (ImGui::Button("Ping")) c.Ping();
	if (!s.connected) ImGui::EndDisabled();
	if (!g_pushResult.empty()) { ImGui::SameLine(); ImGui::TextDisabled("%s", g_pushResult.c_str()); }
	if (!s.lastReply.empty()) ImGui::TextWrapped("Last reply: %s", s.lastReply.c_str());
	ImGui::Separator();

	if (s.connected && s.haveState) {
		SlotTable(s);
		FollowSection(s, ctx);
		SetCharSection(c, s);
	}
	if (s.connected) StageSection(c, s, ctx);

	if (ImGui::TreeNode("Log")) {
		const auto log = c.RecentLog();
		ImGui::BeginChild("linklog", ImVec2(0, 140), true);
		for (const auto& l : log) ImGui::TextUnformatted(l.c_str());
		if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) ImGui::SetScrollHereY(1.0f);
		ImGui::EndChild();
		ImGui::TreePop();
	}
	ImGui::End();
}

} // namespace gamelink

// ---- game_link_api.h ----
uint16_t GameLink_ShowStageInGame(int stageId)
{
	if (stageId < 1 || stageId > 99) return 0;
	return gamelink::SharedClient().SetStageWhenConnected(stageId);
}

uint16_t GameLink_ReloadStageInGame(bool rereadList)
{
	gamelink::Client& c = gamelink::SharedClient();
	if (!c.Get().connected) return 0;
	return c.ReloadStage(rereadList ? gamelink::wire::kFlagStageList : 0);
}

GameLinkStageInfo GameLink_GetStageInfo()
{
	const gamelink::Snapshot s = gamelink::SharedClient().Get();
	GameLinkStageInfo i;
	i.connected = s.connected;
	i.haveStage = s.haveStage;
	i.allowed = s.haveStage && s.stage.allowed;
	i.loaded = s.haveStage ? s.stage.loaded : -1;
	i.bgm = s.haveStage ? s.stage.bgmId : -1;
	i.dataFile = s.haveStage ? s.stage.dataFile : "";
	i.lastReply = s.lastReply;
	return i;
}
