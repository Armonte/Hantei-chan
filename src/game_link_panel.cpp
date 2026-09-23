// Game Link window — see game_link_panel.h.
#include "game_link_panel.h"
#include "framestate.h"

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
