// [authoring] The Live tab (what the game runs and resolved: teams, tag / assist state, Follow point, the resolved-values
// grid with the cells that differ from Hantei-chan's own resolution) and the Log tab (a filtered tail of
// <gamedir>\pchost_authoring.log plus the link and launcher logs). docs/HANTEI_AUTHORING_MODE.md §5.6, §8.9.
#include "authoring_state.h"
#include "../tag_tuning/tag_levers.h"

#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>

namespace authoring {

namespace fs = std::filesystem;
namespace wire = gamelink::wire;
using namespace tagtune;

namespace {

uint64_t NowMs() { return GetTickCount64(); }
std::string Lower(std::string s) { for (char& c : s) if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a'); return s; }
std::string ActorFile(const wire::Actor& a) { return Lower(std::string(a.file, strnlen(a.file, sizeof a.file))); }

const char* TagStateName(int32_t r)
{
	switch (r) {
	case 0: return "idle";
	case 100: return "exit";
	case 101: return "cooldown";
	case 150: return "forced tag-in pending";
	case 200: return "22D accepted";
	case 254: return "swap guard (S1)";
	case 255: case 256: return "swapping";
	case 300: case 301: case 302: case 303: return "assist";
	}
	return "?";
}

std::string Badges(const gamelink::Snapshot& s, int col, int lever)
{
	std::string b;
	auto add = [&b](const char* x) { b += b.empty() ? x : std::string(", ") + x; };
	const wire::TuningGlobal& g = s.tuning;
	if (wire::MaskBit(g.styleMask, lever)) add("style");
	if (wire::MaskBit(g.tuningMask, lever)) add("global");
	if (wire::MaskBit(g.envMask, lever)) add("env");
	if (col >= 0) {
		const wire::TuningSlot& t = s.tuningSlot[col];
		if (wire::MaskBit(t.charMask, lever)) add("char");
		if (wire::MaskBit(t.moonMask, lever)) add("moon");
		if (wire::MaskBit(t.cssMask, lever)) add("CSS");
		if (t.flags & wire::kTunSlotFromHost) add("host");
	}
	return b.empty() ? "default" : b;
}

} // namespace

// Follow point: the chosen team's point drives its tab (switching tabs on a swap).
void FollowTick(HostContext& host, const gamelink::Snapshot& s)
{
	AuthoringState& a = St();
	if (!a.follow || !s.haveState || !host.showPattern) return;
	const int team = a.followTeam;
	int slot = s.state.tagLive ? s.state.teamActive[team] : team;
	if (slot < 0 || slot > 3) slot = team;
	const wire::Actor& act = s.state.actors[slot];
	if (!act.exists) return;
	const int moon = act.moon >= 0 && act.moon <= 2 ? act.moon : 0;
	const std::string txt = DataTxt(ActorFile(act), moon);
	const bool swapped = slot != a.lastFollowSlot;
	if (!swapped && act.pattern == a.lastFollowPattern && act.frame == a.lastFollowFrame) return;
	a.lastFollowSlot = slot;
	a.lastFollowPattern = act.pattern;
	a.lastFollowFrame = act.frame;
	host.showPattern(txt, act.pattern, act.frame, swapped && a.followSwitchTab);
}

void DrawLiveTab(HostContext& host, const gamelink::Snapshot& s, const LinkPolicy& p)
{
	AuthoringState& a = St();
	(void)host;
	if (!s.connected) { ImGui::TextDisabled("Not linked. Launch or Attach in the header."); return; }
	// actions
	ImGui::BeginDisabled(!p.canApplyTuning);
	if (ImGui::Button("Re-read tuning")) a.applySeq = Link().ApplyTuning(wire::kFlagQueryAfter);
	ImGui::EndDisabled();
	ImGui::SameLine();
	if (ImGui::Button("Query tuning now")) Link().QueryTuning(0);
	ImGui::SameLine();
	ImGui::BeginDisabled(!s.Authoring() || p.sessionLocked);
	if (ImGui::Button("End authoring")) Link().EndAuthoring();
	ImGui::EndDisabled();
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("Clear the Authoring VS pins (timer, endless round): the match goes on as a normal offline match.");
	ImGui::SameLine(0, 30);
	ImGui::Checkbox("Follow point", &a.follow);
	ImGui::SameLine();
	ImGui::RadioButton("P1 team", &a.followTeam, 0);
	ImGui::SameLine();
	ImGui::RadioButton("P2 team", &a.followTeam, 1);
	ImGui::SameLine();
	ImGui::Checkbox("switch tabs on a tag", &a.followSwitchTab);
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("The editor tab follows the team's POINT: its pattern / frame every frame, and the tab itself when the point changes.");
	// state
	if (s.haveState) {
		const wire::State& st = s.state;
		ImGui::Text("scene %u  mode 0x%X  timer %u  reloads %u  %s", st.scene, st.gameModeKind, st.worldTimer, st.reloadCount,
		            st.reloadAllowed ? "reload gate open" : "reload gate closed");
		if (ImGui::BeginTable("##teams", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit)) {
			ImGui::TableSetupColumn("team");
			ImGui::TableSetupColumn("point");
			ImGui::TableSetupColumn("pattern / frame");
			ImGui::TableSetupColumn("reserve");
			ImGui::TableSetupColumn("tag / assist");
			ImGui::TableHeadersRow();
			for (int team = 0; team < 2; ++team) {
				const int pt = st.tagLive ? st.teamActive[team] : team;
				const int rs = pt >= 0 && pt < 4 ? (pt < 2 ? pt + 2 : pt - 2) : -1;
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::Text("P%d", team + 1);
				ImGui::TableNextColumn();
				if (pt >= 0 && pt < 4 && st.actors[pt].exists) ImGui::TextColored(kColOk, "slot %d %s %s", pt, st.actors[pt].file, MoonShort(st.actors[pt].moon));
				else ImGui::TextDisabled("-");
				ImGui::TableNextColumn();
				if (pt >= 0 && pt < 4 && st.actors[pt].exists) ImGui::Text("p%d f%d (%d/%d)", st.actors[pt].pattern, st.actors[pt].frame, st.actors[pt].frameTicks, st.actors[pt].patternTicks);
				ImGui::TableNextColumn();
				if (rs >= 0 && st.actors[rs].exists) ImGui::TextDisabled("slot %d %s", rs, st.actors[rs].file);
				ImGui::TableNextColumn();
				if (s.haveTag) {
					const wire::TagTeam& t = s.tag.team[team];
					ImGui::Text("%s (%d)", TagStateName(t.tagRequest), t.tagRequest);
					if (t.cooldownLeft > 0) { ImGui::SameLine(); ImGui::TextColored(kColWarn, "cooldown %d", t.cooldownLeft); }
					if (t.assistPattern) { ImGui::SameLine(); ImGui::TextColored(kColOver, "assist p%d tick %d", t.assistPattern, t.assistTick); }
					if (t.assistCooldown > 0) { ImGui::SameLine(); ImGui::TextDisabled("assist cd %d", t.assistCooldown); }
				} else ImGui::TextDisabled(st.teamTagRequest[team] ? "tag in progress" : "-");
			}
			ImGui::EndTable();
		}
	}
	// the resolved grid
	if (!s.haveTuning) { ImGui::TextDisabled("no tuning answer yet (rev 1 DLL, or waiting)"); return; }
	const wire::TuningGlobal& g = s.tuning;
	const char* src[] = { "defaults", "legacy tag_tuning.ini", "sidecars", "host / tape (adopted)" };
	ImGui::Text("tuning: %s  style '%s'  sha %s  loads %u  warnings %u  char files %u", g.source < 4 ? src[g.source] : "?", g.activeStyle, g.sha,
	            (unsigned)g.tuningLoads, g.warnings, g.charFiles);
	if (g.flags & wire::kTunFlagHotReloadPaused) { ImGui::SameLine(); ImGui::TextColored(kColWarn, "hot reload PAUSED (F3)"); }
	if (g.flags & wire::kTunFlagReadError) { ImGui::SameLine(); ImGui::TextColored(kColBad, "read error: previous values kept"); }
	if (g.flags & wire::kTunFlagLegacyIgnored) { ImGui::SameLine(); ImGui::TextColored(kColWarn, "tag_tuning.ini ignored (sidecars win): migrate it"); }
	if (g.flags & wire::kTunFlagParkXRestart) { ImGui::SameLine(); ImGui::TextColored(kColWarn, "parkX changed: restart the game"); }
	if (g.leverTableHash != LeverTableHash()) ImGui::TextColored(kColBad, "lever table differs: raw numbers, not interpreted");
	const GlobalResolution mine = St().ws.Global();
	SlotResolution mineSlot[4];
	for (int k = 0; k < 4; ++k)
		if (s.haveTuningSlot[k] && s.tuningSlot[k].exists) mineSlot[k] = St().ws.Slot(s.tuningSlot[k].file, s.tuningSlot[k].moon);
	static bool onlyDiff = false;
	ImGui::Checkbox("only rows that differ from these files", &onlyDiff);
	ImGui::SameLine();
	ImGui::TextDisabled("red = the game resolved another value than Hantei-chan does from the same files (hover: the provenance)");
	if (ImGui::BeginTable("##grid", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit,
	                      ImVec2(0, ImGui::GetContentRegionAvail().y))) {
		ImGui::TableSetupScrollFreeze(1, 1);
		ImGui::TableSetupColumn("lever");
		ImGui::TableSetupColumn("global");
		for (int k = 0; k < 4; ++k) {
			char h[48];
			const wire::TuningSlot& t = s.tuningSlot[k];
			if (s.haveTuningSlot[k] && t.exists) std::snprintf(h, sizeof h, "%s %s %s", SlotRoleName(k), t.file, MoonShort(t.moon));
			else std::snprintf(h, sizeof h, "%s (empty)", SlotRoleName(k));
			ImGui::TableSetupColumn(h);
		}
		ImGui::TableHeadersRow();
		for (int i = 0; i < (int)kLeverCount && i < g.leverCount; ++i) {
			const Lever& l = kLevers[i];
			bool diff[5] = {};
			diff[0] = l.scope != LeverScope::CharOnly && l.scope != LeverScope::Harness && g.values[i] != mine.values[i] && !wire::MaskBit(g.envMask, i);
			for (int k = 0; k < 4; ++k)
				diff[k + 1] = s.haveTuningSlot[k] && s.tuningSlot[k].exists && s.tuningSlot[k].values[i] != mineSlot[k].values[i] &&
				              !wire::MaskBit(s.tuningSlot[k].cssMask, i) && !wire::MaskBit(g.envMask, i) && l.scope != LeverScope::Harness;
			const bool any = diff[0] || diff[1] || diff[2] || diff[3] || diff[4];
			if (onlyDiff && !any) continue;
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(l.key);
			for (int c = 0; c < 5; ++c) {
				ImGui::TableNextColumn();
				const int k = c - 1;
				if (c > 0 && !(s.haveTuningSlot[k])) { ImGui::TextDisabled("-"); continue; }
				const int32_t v = c == 0 ? g.values[i] : s.tuningSlot[k].values[i];
				if (c == 0 && l.scope == LeverScope::CharOnly) { ImGui::TextDisabled("-"); continue; }
				if (diff[c]) {
					ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, IM_COL32(120, 30, 30, 255));
					ImGui::TextColored(kColBad, "%s", FormatLeverValue(l, v).c_str());
				} else {
					const bool set = c == 0 ? (wire::MaskBit(g.tuningMask, i) || wire::MaskBit(g.styleMask, i) || wire::MaskBit(g.envMask, i))
					                        : (wire::MaskBit(s.tuningSlot[k].charMask, i) || wire::MaskBit(s.tuningSlot[k].moonMask, i) || wire::MaskBit(s.tuningSlot[k].cssMask, i));
					ImGui::TextColored(set ? kColOver : kColDim, "%s", FormatLeverValue(l, v).c_str());
				}
				if (ImGui::IsItemHovered()) {
					const int32_t m = c == 0 ? mine.values[i] : mineSlot[k].values[i];
					ImGui::SetTooltip("%s: game %s (%s)\nHantei-chan resolves %s from the same files%s", l.key, FormatLeverValue(l, v).c_str(),
					                  Badges(s, k, i).c_str(), FormatLeverValue(l, m).c_str(),
					                  diff[c] ? "\n-> DIFFERS: re-read, hot reload paused, a read error or a stale DLL" : "");
				}
			}
		}
		ImGui::EndTable();
	}
}

// ---- Log ----
namespace {
void PollGameLog()
{
	AuthoringState& a = St();
	const uint64_t now = NowMs();
	if (now - a.lastLogMs < 500 || a.gameDir.empty()) return;
	a.lastLogMs = now;
	uint64_t size = 0;
	const std::string path = a.gameDir + "\\pchost_authoring.log";
	std::vector<std::string> tail = TailFile(path, 3000, &size);
	if (size == a.gameLogSize) return;
	a.gameLogSize = size;
	a.gameLog = std::move(tail);
}

bool IsProblemLine(const std::string& l)
{
	const std::string x = Lower(l);
	return x.find("error") != std::string::npos || x.find("warn") != std::string::npos || x.find("fail") != std::string::npos ||
	       x.find("refus") != std::string::npos || x.find("crash") != std::string::npos || x.find("exception") != std::string::npos;
}

bool TagRelated(const std::string& l)
{
	const std::string x = Lower(l);
	for (const char* k : { "authoring", "tag", "tuning", "assist", "link", "sidecar", "setup", "reload", "stage", "adopt" })
		if (x.find(k) != std::string::npos) return true;
	return false;
}
} // namespace

void DrawLogTab(const gamelink::Snapshot& s)
{
	AuthoringState& a = St();
	PollGameLog();
	ImGui::SetNextItemWidth(260);
	ImGui::InputTextWithHint("##filter", "filter (substring, case-insensitive)", a.logFilter, sizeof a.logFilter);
	ImGui::SameLine();
	ImGui::Checkbox("tag / authoring lines only", &a.logTagOnly);
	ImGui::SameLine();
	ImGui::Checkbox("problems only", &a.logProblemsOnly);
	ImGui::SameLine();
	ImGui::Checkbox("follow the end", &a.logFollow);
	ImGui::SameLine();
	if (ImGui::Button("Copy shown")) {
		std::string all;
		const std::string f = Lower(a.logFilter);
		for (const std::string& l : a.gameLog)
			if ((!a.logTagOnly || TagRelated(l)) && (!a.logProblemsOnly || IsProblemLine(l)) && (f.empty() || Lower(l).find(f) != std::string::npos)) all += l + "\n";
		ImGui::SetClipboardText(all.c_str());
	}
	ImGui::TextDisabled("%s\\pchost_authoring.log (%llu bytes)", a.gameDir.c_str(), (unsigned long long)a.gameLogSize);
	const float h = ImGui::GetContentRegionAvail().y * 0.68f;
	ImGui::BeginChild("##gamelog", ImVec2(0, h), ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar);
	const std::string f = Lower(a.logFilter);
	int shown = 0;
	for (const std::string& l : a.gameLog) {
		if (a.logTagOnly && !TagRelated(l)) continue;
		if (a.logProblemsOnly && !IsProblemLine(l)) continue;
		if (!f.empty() && Lower(l).find(f) == std::string::npos) continue;
		ImGui::TextColored(IsProblemLine(l) ? kColWarn : ImVec4(0.85f, 0.85f, 0.85f, 1.0f), "%s", l.c_str());
		++shown;
	}
	if (!shown) ImGui::TextDisabled(a.gameLog.empty() ? "(no log yet: launch the game from this window)" : "(nothing matches the filter)");
	if (a.logFollow) ImGui::SetScrollHereY(1.0f);
	ImGui::EndChild();
	ImGui::TextDisabled("link + launcher");
	ImGui::BeginChild("##linklog", ImVec2(0, 0), ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar);
	for (const std::string& l : a.launcher.Log()) if (f.empty() || Lower(l).find(f) != std::string::npos) ImGui::TextUnformatted(l.c_str());
	for (const std::string& l : Link().RecentLog()) if (f.empty() || Lower(l).find(f) != std::string::npos) ImGui::TextDisabled("%s", l.c_str());
	if (a.logFollow) ImGui::SetScrollHereY(1.0f);
	ImGui::EndChild();
	(void)s;
}

} // namespace authoring
