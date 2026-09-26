// EXPERIMENTAL Tag / Team window — see tag_panel.h and docs/HANTEI_TAG_PANEL.md.
#include "tag_panel.h"
#include "tag_tuning/tag_assist.h"
#include "tag_tuning/tag_ini.h"
#include "tag_tuning/tag_widgets.h"
#include "cmdfile/cmd_io.h"
#include "filedialog.h"
#include "framestate.h"
#include "game_link_panel.h"
#include "misc.h"

#include <windows.h>

#include <imgui.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>

namespace tagpanel {

bool showPanel = false;

namespace {

namespace fs = std::filesystem;
using namespace tagtune;

const ImVec4 kWarn(1.0f, 0.62f, 0.25f, 1.0f), kOk(0.45f, 0.95f, 0.45f, 1.0f), kOver(0.55f, 0.8f, 1.0f, 1.0f),
             kBad(1.0f, 0.4f, 0.4f, 1.0f), kExp(0.95f, 0.35f, 0.05f, 1.0f);

// ---- document state ----
TagIni g_ini;
std::string g_path;                  // the ini being edited ("" = none yet)
bool g_pathExplicit = false;         // chosen by the user / startup (else follows the game dir)
bool g_loaded = false;
bool g_existsOnDisk = false;
fs::file_time_type g_diskTime{};
uintmax_t g_diskSize = 0;
std::string g_diskText;              // what is on disk (the save baseline + validation baseline)
bool g_diskChanged = false;          // changed outside the panel while we had edits
uint64_t g_lastStatMs = 0;
std::string g_status;                // one line: last load / save result
std::vector<Warning> g_blocked;      // new warnings that refused the last save

// ---- options ----
char g_gameDirBuf[260] = "";
bool g_autoApply = false;
bool g_reloadChars = false;
bool g_dropOverrides = true;
char g_newStyle[48] = "";
uint64_t g_editMs = 0;               // last edit, for auto-apply
bool g_pendingApply = false;

// ---- character / assists ----
std::string g_char;                  // [char.<file>] being edited (lower case file name)
int g_cmdMoon = 0;
std::string g_cmdPath;               // the _c.txt the assist editor reads
std::string g_cmdLoadedPath = "\x01";   // never a real path: the first call always loads
std::vector<CommandInfo> g_cmds;
std::string g_cmdStatus;
char g_motionBuf[5][16] = {};
bool g_motionInit[5] = {};

// ---- live ----
bool g_follow = false;
int g_followTeam = 0;
int g_lastFollow = -1, g_lastFollowFrame = -1;
std::string g_startTab;

// ---- the session gate ----
uint16_t g_probeSeq = 0;
uint64_t g_probeSentMs = 0;
int g_probeVerdict = 0;              // 0 unknown, 1 open, -1 closed
std::string g_probeWhy;
uint64_t g_probeAtMs = 0;
bool g_applyWaitingProbe = false;

uint64_t NowMs() { return GetTickCount64(); }

std::string Lower(std::string s) { for (char& c : s) c = (char)tolower((unsigned char)c); return s; }

// "sion_0" -> "sion"; "Hermes_2" -> "hermes"; "akaakiha" -> "akaakiha"
std::string CharFileOfStem(const std::string& stem)
{
	std::string s = Lower(stem);
	if (s.size() > 2 && s[s.size() - 2] == '_' && s.back() >= '0' && s.back() <= '9') s.resize(s.size() - 2);
	return s;
}

std::string g_activeTxt;              // the editor's active character .txt (set every frame)

// The game folder: the box, else the linked MBAA.exe's folder, else the folder above the open character's data// when it holds an MBAA.exe (so the panel works without the link, on the game's own data).
std::string GameDir(const gamelink::Snapshot& s)
{
	if (g_gameDirBuf[0]) return g_gameDirBuf;
	if (s.connected && s.pid) return gamelink::GameDirOf(s.pid);
	if (!g_activeTxt.empty()) {
		std::error_code ec;
		const fs::path data = fs::u8path(g_activeTxt).parent_path();
		if (ieq(data.filename().u8string(), "data") && fs::exists(data.parent_path() / "MBAA.exe", ec))
			return data.parent_path().u8string();
	}
	return {};
}

std::string DefaultIniPath(const gamelink::Snapshot& s)
{
	const std::string dir = GameDir(s);
	return dir.empty() ? std::string() : dir + "\\tag_tuning.ini";
}

bool ReadFile(const std::string& p, std::string& out) { return cmdfile::ReadFileBytes(p, out, nullptr); }

void StatDisk(bool& exists, fs::file_time_type& t, uintmax_t& size)
{
	std::error_code ec;
	exists = !g_path.empty() && fs::exists(fs::u8path(g_path), ec);
	if (!exists) return;
	t = fs::last_write_time(fs::u8path(g_path), ec);
	size = fs::file_size(fs::u8path(g_path), ec);
}

void LoadFromDisk(const std::string& why)
{
	std::string text;
	StatDisk(g_existsOnDisk, g_diskTime, g_diskSize);
	if (g_existsOnDisk && !ReadFile(g_path, text)) { g_status = "could not read " + g_path; return; }
	g_diskText = text;
	g_ini.LoadText(text);
	g_loaded = true;
	g_diskChanged = false;
	g_blocked.clear();
	for (auto& b : g_motionInit) b = false;
	g_status = (g_existsOnDisk ? why + ": " + g_path : "no " + g_path + " yet (all defaults); it is created on save");
}

void Edited() { g_editMs = NowMs(); if (g_autoApply) g_pendingApply = true; }

// ---- the session gate: may the ini be written right now? ----
// 1 = yes, -1 = no, 0 = asking (probe in flight). why = the reason shown.
int WriteVerdict(gamelink::Client& c, const gamelink::Snapshot& s, std::string& why, bool allowProbe)
{
	if (!s.connected) {
		why = "link not connected: the session state is unknown (the game only re-reads the file offline)";
		return 1;
	}
	if (s.haveTag) {
		if (s.tag.sessionFlags) { why = "the game reports a session (tuning frozen): not writing"; return -1; }
		why = "offline (QueryTag)";
		return 1;
	}
	if (s.haveState) {
		if (s.state.gameModeKind == 0xFFFFFFFFu) { why = "native netplay mode (g_GameModeKind = -1): not writing"; return -1; }
		if (s.state.scene == 1) {
			if (s.state.reloadAllowed) { why = "offline battle (reload gate open)"; return 1; }
			why = "reload gate closed in battle (session / replay / recording): not writing";
			return -1;
		}
	}
	// outside a battle LinkState cannot tell: ask the gate (session is checked before the scene)
	const uint64_t now = NowMs();
	if (g_probeVerdict != 0 && now - g_probeAtMs < 3000) { why = g_probeWhy; return g_probeVerdict; }
	if (!allowProbe) {
		// probing only on Apply: a refused probe writes a line to the game's log, so it is not polled
		why = "not in a battle: the session state is checked with the game's gate when you apply";
		return 1;
	}
	if (!g_probeSeq || now - g_probeSentMs > 1500) {
		if (g_probeSeq && now - g_probeSentMs > 1500) {
			g_probeSeq = 0;
			g_probeVerdict = -1; g_probeAtMs = now; g_probeWhy = "the gate probe got no answer: not writing";
			why = g_probeWhy;
			return -1;
		}
		g_probeSeq = c.ProbeGate();
		g_probeSentMs = now;
	}
	gamelink::wire::Reply r;
	if (c.PeekReply(g_probeSeq, r)) {
		using S = gamelink::wire::Status;
		const S st = (S)r.status;
		g_probeSeq = 0;
		g_probeAtMs = now;
		if (st == S::RefusedSession || st == S::RefusedRecording) {
			g_probeVerdict = -1; g_probeWhy = std::string("game gate: ") + gamelink::wire::StatusName(r.status) + ": not writing";
		} else {
			g_probeVerdict = 1; g_probeWhy = std::string("offline (gate probe: ") + gamelink::wire::StatusName(r.status) + ")";
		}
		why = g_probeWhy;
		return g_probeVerdict;
	}
	why = "asking the game's gate...";
	return 0;
}

bool WriteNow(gamelink::Client& c, const gamelink::Snapshot& s)
{
	if (g_path.empty()) { g_status = "no ini path: connect the Game Link or set the game folder"; return false; }
	// validation: the file may keep the warnings it had on disk; an edit may not add one
	TagIni disk;
	disk.LoadText(g_diskText);
	g_blocked = NewWarnings(AllWarnings(disk), AllWarnings(g_ini));
	if (!g_blocked.empty()) { g_status = "NOT saved: the edit adds " + std::to_string(g_blocked.size()) + " warning(s)"; return false; }
	if (g_diskChanged) { g_status = "NOT saved: the file changed on disk (reload or keep yours first)"; return false; }
	const std::string& t = g_ini.Text();
	if (g_existsOnDisk && t == g_diskText) { g_status = "nothing to save (identical)"; return true; }
	if (g_existsOnDisk) WriteFileAtomic((g_path + ".bak").c_str(), g_diskText.data(), g_diskText.size());
	if (!WriteFileAtomic(g_path.c_str(), t.data(), t.size())) { g_status = "SAVE FAILED: " + g_path; return false; }
	g_diskText = t;
	g_ini.MarkSaved();
	StatDisk(g_existsOnDisk, g_diskTime, g_diskSize);
	g_status = "saved " + g_path + " (the game re-reads it within a second, offline)";
	if (g_reloadChars && s.connected) {
		if (s.haveState && s.state.reloadAllowed) {
			const uint16_t seq = c.Reload(0);
			g_status += "; character reload #" + std::to_string(seq) + " sent";
		} else {
			g_status += "; character reload skipped (not in an offline battle)";
		}
	}
	return true;
}

// Apply = gate check (maybe async) + write.
void RequestApply() { g_applyWaitingProbe = true; }
void PumpApply(gamelink::Client& c, const gamelink::Snapshot& s)
{
	if (!g_applyWaitingProbe) return;
	std::string why;
	const int v = WriteVerdict(c, s, why, true);
	if (v == 0) return;
	g_applyWaitingProbe = false;
	if (v < 0) { g_status = "NOT saved: " + why; return; }
	WriteNow(c, s);
}

// ---- lever widgets: tag_tuning/tag_widgets.h (shared with the Authoring window) ----
using tagui::RangeText;
using tagui::GuideNote;
using tagui::LeverTooltip;
using tagui::ValueWidget;
using tagui::IsSlotAction;
using tagui::IsSlotEntry;

// ================== header ==================

void Header(gamelink::Client& c, const gamelink::Snapshot& s)
{
	ImGui::TextColored(kExp, "EXPERIMENTAL");
	ImGui::SameLine();
	ImGui::TextDisabled("edits PovertyCaster's tag_tuning.ini (TAG_TUNING_GUIDE.md). Tag design is not locked.");

	// file
	const std::string def = DefaultIniPath(s);
	if (!g_pathExplicit && def != g_path && !def.empty() && !g_ini.IsDirty()) { g_path = def; LoadFromDisk("loaded"); }
	ImGui::SetNextItemWidth(260);
	ImGui::InputTextWithHint("##gamedir", "game folder (default: from the Game Link)", g_gameDirBuf, sizeof g_gameDirBuf);
	ImGui::SameLine();
	if (ImGui::Button("Open ini...")) {
		const std::string p = FileDialog(-1, false);
		if (!p.empty()) { g_path = p; g_pathExplicit = true; LoadFromDisk("opened"); }
	}
	if (g_pathExplicit) {
		ImGui::SameLine();
		if (ImGui::SmallButton("use game folder")) { g_pathExplicit = false; g_path.clear(); }
	}
	ImGui::Text("File: %s%s", g_path.empty() ? "(none: connect the Game Link or set the game folder)" : g_path.c_str(),
	            g_ini.IsDirty() ? "  *modified" : "");
	if (!g_path.empty() && !g_existsOnDisk) { ImGui::SameLine(); ImGui::TextDisabled("(not on disk yet)"); }

	// session / write gate
	std::string why;
	const int v = s.connected ? WriteVerdict(c, s, why, false) : 1;
	if (!s.connected) ImGui::TextColored(kWarn, "Game Link not connected: session state unknown.");
	else ImGui::TextColored(v > 0 ? kOk : v < 0 ? kBad : kWarn, "%s", why.c_str());
	if (!s.connected) { ImGui::SameLine(); if (ImGui::SmallButton("Connect")) c.Connect(); }

	// actions
	if (g_path.empty()) ImGui::BeginDisabled();
	if (ImGui::Button("Apply to game")) RequestApply();
	ImGui::SameLine();
	if (ImGui::Button("Reload from disk")) LoadFromDisk("reloaded");
	ImGui::SameLine();
	if (!g_ini.IsDirty()) ImGui::BeginDisabled();
	if (ImGui::Button("Revert")) { g_ini.SetText(g_ini.SavedText()); g_blocked.clear(); }
	if (!g_ini.IsDirty()) ImGui::EndDisabled();
	if (g_path.empty()) ImGui::EndDisabled();
	ImGui::SameLine();
	ImGui::Checkbox("auto-apply", &g_autoApply);
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("Save 0.4 s after each edit, so the game picks it up while you play.");
	ImGui::SameLine();
	ImGui::Checkbox("+ reload characters", &g_reloadChars);
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("After saving, send a Game Link character reload so HA6 / pattern edits apply too\n(the game also re-reads the ini after every character reload).");
	if (g_applyWaitingProbe) { ImGui::SameLine(); ImGui::TextDisabled("(waiting for the game's gate)"); }

	if (g_diskChanged) {
		ImGui::TextColored(kWarn, "tag_tuning.ini changed on disk (the F3 panel's Save rewrites it).");
		ImGui::SameLine();
		if (ImGui::SmallButton("Load theirs")) LoadFromDisk("reloaded");
		ImGui::SameLine();
		if (ImGui::SmallButton("Keep mine")) {
			std::string t;
			ReadFile(g_path, t);
			g_diskText = t;
			StatDisk(g_existsOnDisk, g_diskTime, g_diskSize);
			g_diskChanged = false;
		}
	}
	if (!g_status.empty()) ImGui::TextWrapped("%s", g_status.c_str());
	for (const Warning& w : g_blocked) ImGui::TextColored(kBad, "  would add: %s", w.Text().c_str());
	const std::vector<Warning> all = AllWarnings(g_ini);
	if (!all.empty()) {
		ImGui::PushStyleColor(ImGuiCol_Text, kWarn);
		const bool open = ImGui::TreeNode("warnings", "%zu ini warning(s) (the game skips these keys)", all.size());
		ImGui::PopStyleColor();
		if (open) {
			for (const Warning& w : all) ImGui::BulletText("%s", w.Text().c_str());
			ImGui::TreePop();
		}
	}
}

// ================== Global ==================

void GlobalTab()
{
	const Resolved r = Resolve(g_ini);
	Values base;
	std::vector<Warning> ignored;
	StyleBase(g_ini, r.style, base, ignored);

	// style
	const std::string active = g_ini.ActiveStyle();
	ImGui::SetNextItemWidth(200);
	if (ImGui::BeginCombo("style (active_style)", active.empty() ? "(none: defaults)" : active.c_str())) {
		if (ImGui::Selectable("(none: defaults)", active.empty())) { SelectStyle(g_ini, "", g_dropOverrides); Edited(); }
		for (const std::string& n : StyleNames(g_ini)) {
			const BuiltinStyle* b = FindBuiltinStyle(n);
			const bool inIni = g_ini.HasSection(SecKind::Style, n);
			std::string label = n + (inIni ? (b ? "  (ini, replaces the built-in)" : "  (ini)") : "  (built-in)");
			if (ImGui::Selectable(label.c_str(), ieq(n, active))) { SelectStyle(g_ini, n, g_dropOverrides); Edited(); }
			if (b && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", b->desc);
		}
		ImGui::EndCombo();
	}
	ImGui::SameLine();
	ImGui::Checkbox("picking a style drops [tuning] overrides", &g_dropOverrides);
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("As the in-game F3 panel does: the style is what you asked for.");
	if (!r.styleFound) ImGui::TextColored(kWarn, "active_style '%s' is unknown: the game uses the defaults", active.c_str());
	ImGui::SetNextItemWidth(160);
	ImGui::InputTextWithHint("##newstyle", "new style name", g_newStyle, sizeof g_newStyle);
	ImGui::SameLine();
	const bool nameOk = g_newStyle[0] && !std::strpbrk(g_newStyle, "[]=;# \t");
	if (!nameOk) ImGui::BeginDisabled();
	if (ImGui::Button("Save as style")) { SaveAsStyle(g_ini, g_newStyle, r.global); Edited(); }
	if (!nameOk) ImGui::EndDisabled();
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		ImGui::SetTooltip("[style.<name>] = every lever that differs from the defaults; it becomes active and the\n[tuning] lever keys go (F3 'Save as style'). Comments and unknown keys stay.");
	ImGui::TextDisabled("Blue = set in [tuning] (overrides the style). Hover a lever for the guide's notes; x clears an override.");

	const char* group = nullptr;
	for (size_t i = 0; i < kLeverCount; ++i) {
		const Lever& l = kLevers[i];
		if (l.scope == LeverScope::CharOnly) continue;
		if (!group || std::strcmp(group, l.group)) {
			group = l.group;
			ImGui::SeparatorText(group);
		}
		ImGui::PushID((int)i);
		std::string raw;
		const bool over = g_ini.Get(SecKind::Tuning, "", l.key, raw);
		int32_t v = r.global[i];
		if (l.scope == LeverScope::SimBoot) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
		if (ValueWidget(l, v, 200)) { SetTuningLever(g_ini, (int)i, v); Edited(); }
		if (l.scope == LeverScope::SimBoot) ImGui::PopStyleColor();
		LeverTooltip(l, base[i], r.style.empty() ? "defaults" : ("style " + r.style).c_str());
		ImGui::SameLine();
		if (over) ImGui::TextColored(kOver, "%s", l.key); else ImGui::TextUnformatted(l.key);
		LeverTooltip(l, base[i], r.style.empty() ? "defaults" : ("style " + r.style).c_str());
		if (over) {
			ImGui::SameLine();
			if (ImGui::SmallButton("x")) { ClearTuningLever(g_ini, (int)i); Edited(); }
		}
		if (base[i] != l.def && !over) { ImGui::SameLine(); ImGui::TextDisabled("(style)"); }
		ImGui::PopID();
	}
}

// ================== characters ==================

std::vector<std::string> CharacterList(const EditorContext& ctx, const gamelink::Snapshot& s)
{
	std::vector<std::string> out;
	auto add = [&](const std::string& n) {
		if (n.empty()) return;
		const std::string l = Lower(n);
		if (std::find(out.begin(), out.end(), l) == out.end()) out.push_back(l);
	};
	if (!ctx.activeKey.empty()) add(CharFileOfStem(ctx.activeKey));
	if (s.haveState)
		for (const auto& a : s.state.actors) if (a.exists) add(a.file);
	for (const std::string& n : g_ini.SectionNames(SecKind::Char)) add(n);
	static std::string listedDir;
	static std::vector<std::string> dirChars;
	const std::string dir = GameDir(s);
	if (dir != listedDir) {
		listedDir = dir;
		dirChars.clear();
		std::error_code ec;
		if (!dir.empty())
			for (const auto& e : fs::directory_iterator(fs::u8path(dir + "\\data"), ec)) {
				const std::string f = e.path().filename().u8string();
				if (f.size() > 6 && ieq(f.substr(f.size() - 6), "_c.txt")) {
					const std::string stem = f.substr(0, f.size() - 6);
					if (stem.size() > 2 && stem[stem.size() - 2] == '_') dirChars.push_back(CharFileOfStem(stem));
				}
			}
		std::sort(dirChars.begin(), dirChars.end());
		dirChars.erase(std::unique(dirChars.begin(), dirChars.end()), dirChars.end());
	}
	for (const std::string& n : dirChars) add(n);
	for (const TeamChangeRow& t : kTeamChangeTable) add(t.file);
	return out;
}

bool ActiveMatches(const EditorContext& ctx) { return !ctx.activeKey.empty() && CharFileOfStem(ctx.activeKey) == g_char; }

void Jump(EditorContext& ctx, int pattern)
{
	if (!ctx.activeState || pattern < 0) return;
	const int n = ctx.patternCount ? ctx.patternCount() : 0;
	if (pattern >= n) return;
	FrameState& st = *ctx.activeState;
	st.animating = false;
	st.pattern = pattern;
	st.frame = 0;
	st.currentTick = 0;
}

std::string PatternLabel(const EditorContext& ctx, int p)
{
	std::string n = ctx.patternName ? ctx.patternName(p) : std::string();
	return std::to_string(p) + (n.empty() ? "" : " " + n);
}

// A pattern picker over the active character's HA6 (numbers only when it is not the character being edited).
bool PatternPicker(EditorContext& ctx, const char* id, int32_t& v, int lo, float width)
{
	bool changed = false;
	ImGui::PushID(id);
	if (ActiveMatches(ctx) && ctx.patternCount) {
		const int n = ctx.patternCount();
		ImGui::SetNextItemWidth(width);
		const std::string cur = v < lo ? std::string("(unset)") : v < n ? PatternLabel(ctx, v) : std::to_string(v) + " (not in the HA6!)";
		if (ImGui::BeginCombo("##pat", cur.c_str(), ImGuiComboFlags_HeightLarge)) {
			static char filter[32] = "";
			ImGui::SetNextItemWidth(-1);
			ImGui::InputTextWithHint("##f", "filter", filter, sizeof filter);
			for (int p = 0; p < n && p <= 999; ++p) {
				const std::string label = PatternLabel(ctx, p);
				if (filter[0] && Lower(label).find(Lower(filter)) == std::string::npos) continue;
				if (ctx.patternName && ctx.patternName(p).empty() && !(ctx.frameCount && ctx.frameCount(p) > 0)) continue;
				if (ImGui::Selectable(label.c_str(), p == v)) { v = p; changed = true; }
			}
			ImGui::EndCombo();
		}
	} else {
		ImGui::SetNextItemWidth(width);
		changed = ImGui::InputInt("##pat", &v, 1, 10);
		if (v < lo) v = lo;
		if (v > 999) v = 999;
	}
	ImGui::SameLine();
	if (!ActiveMatches(ctx) || v < 0) ImGui::BeginDisabled();
	if (ImGui::SmallButton("jump")) Jump(ctx, v);
	if (!ActiveMatches(ctx) || v < 0) ImGui::EndDisabled();
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		ImGui::SetTooltip(ActiveMatches(ctx) ? "Open this pattern in the editor" : "Open %s's .txt in the editor to pick by name and jump", g_char.c_str());
	ImGui::PopID();
	return changed;
}

void CharPicker(EditorContext& ctx, const gamelink::Snapshot& s)
{
	const std::vector<std::string> chars = CharacterList(ctx, s);
	if (g_char.empty() && !chars.empty()) g_char = chars.front();
	ImGui::SetNextItemWidth(200);
	std::string cur = g_char + (g_ini.HasSection(SecKind::Char, g_char) ? " *" : "");
	if (ImGui::BeginCombo("character ([char.<file>])", cur.c_str(), ImGuiComboFlags_HeightLarge)) {
		for (const std::string& n : chars) {
			std::string label = n + (g_ini.HasSection(SecKind::Char, n) ? " *" : "");
			if (!ctx.activeKey.empty() && CharFileOfStem(ctx.activeKey) == n) label += "  (open in the editor)";
			if (ImGui::Selectable(label.c_str(), n == g_char)) g_char = n;
		}
		ImGui::EndCombo();
	}
	ImGui::SameLine();
	if (!ctx.activeKey.empty() && ImGui::SmallButton("editor's")) g_char = CharFileOfStem(ctx.activeKey);
	ImGui::SameLine();
	if (!g_ini.HasSection(SecKind::Char, g_char)) ImGui::BeginDisabled();
	if (ImGui::SmallButton("Clear overrides")) { g_ini.RemoveSection(SecKind::Char, g_char); Edited(); }
	if (!g_ini.HasSection(SecKind::Char, g_char)) ImGui::EndDisabled();
	ImGui::TextDisabled("* = has a [char] section. Ticked = this character's own value; unticked = follows the global value.");
}

void CharacterTab(EditorContext& ctx, const gamelink::Snapshot& s)
{
	CharPicker(ctx, s);
	const Resolved r = Resolve(g_ini);
	const Values mine = ForChar(g_ini, r.global, g_char);
	const TeamChangeRow* tc = FindTeamChangeRow(g_char);
	const char* group = nullptr;
	for (size_t i = 0; i < kLeverCount; ++i) {
		const Lever& l = kLevers[i];
		if (!l.perChar || IsSlotAction(l) || IsSlotEntry(l)) continue;
		if (!group || std::strcmp(group, l.group)) { group = l.group; ImGui::SeparatorText(group); }
		ImGui::PushID((int)i);
		std::string raw;
		bool own = g_ini.Get(SecKind::Char, g_char, l.key, raw);
		if (ImGui::Checkbox("##own", &own)) {
			if (own) SetCharLever(g_ini, g_char, (int)i, mine[i]);
			else ClearCharLever(g_ini, g_char, (int)i);
			Edited();
		}
		ImGui::SameLine();
		int32_t v = mine[i];
		if (!own) ImGui::BeginDisabled();
		bool changed;
		if (!std::strcmp(l.key, "tagIn") || !std::strcmp(l.key, "tagOut")) changed = PatternPicker(ctx, l.key, v, 0, 200);
		else changed = ValueWidget(l, v, 200);
		if (changed && own) { SetCharLever(g_ini, g_char, (int)i, v); Edited(); }
		if (!own) ImGui::EndDisabled();
		LeverTooltip(l, r.global[i], "global value");
		ImGui::SameLine();
		if (own) ImGui::TextColored(kOver, "%s", l.key); else ImGui::TextUnformatted(l.key);
		LeverTooltip(l, r.global[i], "global value");
		if (tc && (!std::strcmp(l.key, "tagIn") || !std::strcmp(l.key, "tagOut"))) {
			const bool in = !std::strcmp(l.key, "tagIn");
			ImGui::SameLine();
			ImGui::TextDisabled("(0 = table: %d%s)", in ? tc->tagIn : tc->tagOut, (in ? tc->inMod : tc->outMod) ? ", tag_mod HA6 only" : "");
			if (ActiveMatches(ctx)) {
				ImGui::SameLine();
				ImGui::PushID("tbl");
				if (ImGui::SmallButton("jump table")) Jump(ctx, in ? tc->tagIn : tc->tagOut);
				ImGui::PopID();
			}
		}
		ImGui::PopID();
	}
}

// ================== assists ==================

void LoadCommands(const EditorContext& ctx, const gamelink::Snapshot& s)
{
	std::string want;
	if (ActiveMatches(ctx) && !ctx.activeCommandsPath.empty()) want = ctx.activeCommandsPath;
	else if (ActiveMatches(ctx) && !ctx.activeTxtPath.empty() && !cmdfile::CommandFileCandidates(ctx.activeTxtPath).empty())
		want = cmdfile::CommandFileCandidates(ctx.activeTxtPath).front();   // the open character's own _c.txt
	else {
		const std::string dir = GameDir(s);
		if (!dir.empty()) want = dir + "\\data\\" + g_char + "_" + std::to_string(g_cmdMoon) + "_c.txt";
	}
	g_cmdPath = want;
	if (want == g_cmdLoadedPath) return;
	g_cmdLoadedPath = want;
	g_cmds.clear();
	std::string bytes;
	if (want.empty()) g_cmdStatus = "no _c.txt: open the character or set the game folder";
	else if (!ReadFile(want, bytes)) g_cmdStatus = "cannot read " + want;
	else {
		g_cmds = ParseCommands(bytes, &cmdfile::Cp932ToUtf8);
		g_cmdStatus = std::to_string(g_cmds.size()) + " commands from " + want;
	}
}

std::string CommandLabel(const CommandInfo& c)
{
	std::string s = std::to_string(c.id) + "  " + c.input + "  -> " + std::to_string(c.pattern);
	if (c.meter) s += "  $" + std::to_string(c.meter);
	if (!c.standing) s += c.air ? "  (air)" : "  (not standing)";
	if (!c.name.empty()) s += "  " + c.name;
	return s;
}

void AssistTab(EditorContext& ctx, const gamelink::Snapshot& s)
{
	CharPicker(ctx, s);
	const bool fromEditor = ActiveMatches(ctx) && (!ctx.activeCommandsPath.empty() ||
		(!ctx.activeTxtPath.empty() && !cmdfile::CommandFileCandidates(ctx.activeTxtPath).empty()));
	if (!fromEditor) {
		ImGui::SetNextItemWidth(120);
		const char* moons[] = { "0 Crescent", "1 Full", "2 Half" };
		ImGui::Combo("moon (_c.txt)", &g_cmdMoon, moons, 3);
	}
	LoadCommands(ctx, s);
	ImGui::TextDisabled("%s", g_cmdStatus.c_str());
	const Resolved r = Resolve(g_ini);
	const Values mine = ForChar(g_ini, r.global, g_char);
	if (!mine[FindLever("assistEnabled")])
		ImGui::TextColored(kWarn, "assistEnabled is off: FN1 does nothing (Global tab, or the Assist / Freestyle style).");
	{
		AssistAction def;
		const ActionView dv = DescribeAction(def, g_cmds);
		ImGui::Text("Character %s", dv.text.c_str());
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Used when neither the slot nor slot 5 has an action: the lowest _c.txt id whose input is a\nmotion + A, costing no meter, usable standing, with a pattern (TAG_TUNING_GUIDE.md 3.1).\nThe game also checks moon / ExComCheck at call time.");
		if (dv.pattern >= 0 && ActiveMatches(ctx)) { ImGui::SameLine(); if (ImGui::SmallButton("jump##def")) Jump(ctx, dv.pattern); }
	}
	ImGui::TextDisabled("Inside a slot: pattern > command > motion. A slot with no action uses slot 5's, then the default.");

	const char* dirNames[5] = { "5 (FN1)", "2 (1/2/3)", "6 (toward)", "4 (away)", "8 (7/8/9)" };
	for (int slot = 0; slot < 5; ++slot) {
		ImGui::PushID(slot);
		ImGui::SeparatorText(dirNames[slot]);
		const SlotLevers li = SlotLeverIndices(slot);
		// entry
		{
			std::string raw;
			const bool own = g_ini.Get(SecKind::Char, g_char, kLevers[li.entry].key, raw);
			int32_t e = mine[li.entry];
			if (ValueWidget(kLevers[li.entry], e, 110)) {
				if (e == 0 && !own) {}
				else if (e == 0) ClearCharLever(g_ini, g_char, li.entry);
				else SetCharLever(g_ini, g_char, li.entry, e);
				Edited();
			}
			LeverTooltip(kLevers[li.entry], r.global[li.entry], "global value");
			ImGui::SameLine();
			ImGui::TextDisabled("entry");
		}
		// action mode
		std::string raw;
		const bool hasP = g_ini.Get(SecKind::Char, g_char, kLevers[li.pattern].key, raw);
		const bool hasC = g_ini.Get(SecKind::Char, g_char, kLevers[li.command].key, raw);
		const bool hasM = g_ini.Get(SecKind::Char, g_char, kLevers[li.motion].key, raw);
		int mode = hasP ? 1 : hasC ? 2 : hasM ? 3 : 0;
		const int before = mode;
		ImGui::SameLine();
		ImGui::RadioButton("default", &mode, 0); ImGui::SameLine();
		ImGui::RadioButton("pattern", &mode, 1); ImGui::SameLine();
		ImGui::RadioButton("command", &mode, 2); ImGui::SameLine();
		ImGui::RadioButton("motion", &mode, 3);
		auto clearActions = [&](int keep) {
			if (keep != 1) ClearCharLever(g_ini, g_char, li.pattern);
			if (keep != 2) ClearCharLever(g_ini, g_char, li.command);
			if (keep != 3) ClearCharLever(g_ini, g_char, li.motion);
		};
		if (mode != before) {
			clearActions(mode);
			if (mode == 1) SetCharLever(g_ini, g_char, li.pattern, 1 > mine[li.pattern] ? 1 : mine[li.pattern]);
			if (mode == 2) {
				const int d = PickDefaultCommand(g_cmds);
				SetCharLever(g_ini, g_char, li.command, mine[li.command] >= 0 ? mine[li.command] : d >= 0 ? g_cmds[d].id : 0);
			}
			if (mode == 3) {
				int32_t p = 0;
				PackMotion("236A", p);
				SetCharLever(g_ini, g_char, li.motion, mine[li.motion] ? mine[li.motion] : p);
				g_motionInit[slot] = false;
			}
			Edited();
		}
		const Values now = ForChar(g_ini, r.global, g_char);
		if (mode == 1) {
			int32_t p = now[li.pattern];
			if (PatternPicker(ctx, "p", p, 1, 260)) { SetCharLever(g_ini, g_char, li.pattern, p); Edited(); }
		} else if (mode == 2) {
			const int32_t id = now[li.command];
			const CommandInfo* cur = FindCommand(g_cmds, id);
			ImGui::SetNextItemWidth(360);
			const std::string label = cur ? CommandLabel(*cur) : std::to_string(id) + " (not in the _c.txt)";
			if (ImGui::BeginCombo("##cmd", label.c_str(), ImGuiComboFlags_HeightLarge)) {
				for (const CommandInfo& c : g_cmds)
					if (ImGui::Selectable(CommandLabel(c).c_str(), c.id == id)) { SetCharLever(g_ini, g_char, li.command, c.id); Edited(); }
				ImGui::EndCombo();
			}
			if (g_cmds.empty()) {
				ImGui::SameLine();
				int32_t x = id;
				ImGui::SetNextItemWidth(100);
				if (ImGui::InputInt("id", &x)) { x = std::clamp(x, 0, 999); SetCharLever(g_ini, g_char, li.command, x); Edited(); }
			}
		} else if (mode == 3) {
			if (!g_motionInit[slot]) {
				std::snprintf(g_motionBuf[slot], sizeof g_motionBuf[slot], "%s", UnpackMotion(now[li.motion]).c_str());
				g_motionInit[slot] = true;
			}
			ImGui::SetNextItemWidth(120);
			ImGui::InputText("##mot", g_motionBuf[slot], sizeof g_motionBuf[slot], ImGuiInputTextFlags_CharsUppercase);
			const std::string err = ValidateMotion(g_motionBuf[slot]);
			ImGui::SameLine();
			if (!err.empty()) ImGui::TextColored(kBad, "%s", err.c_str());
			else {
				int32_t p = 0;
				PackMotion(g_motionBuf[slot], p);
				if (p != now[li.motion]) { SetCharLever(g_ini, g_char, li.motion, p); Edited(); }
				ImGui::TextDisabled("ok");
			}
		}
		// what the call will do
		const AssistAction a = ResolveAssistSlot(ForChar(g_ini, r.global, g_char), slot);
		const ActionView av = DescribeAction(a, g_cmds);
		const char* entryNames[] = { "behind", "edge", "drop", "arc" };
		char line[256];
		std::snprintf(line, sizeof line, "  -> %s%s, entry %s", av.text.c_str(),
		              a.fromSlot >= 0 && a.fromSlot != slot ? " (slot 5's action)" : "", entryNames[a.entry & 3]);
		if (av.problem) ImGui::TextColored(kBad, "%s", line); else ImGui::TextUnformatted(line);
		if (av.pattern >= 0 && ActiveMatches(ctx)) {
			ImGui::SameLine();
			if (ImGui::SmallButton("jump")) Jump(ctx, av.pattern);
			if (ctx.patternName) { ImGui::SameLine(); ImGui::TextDisabled("%s", ctx.patternName(av.pattern).c_str()); }
		}
		ImGui::PopID();
	}
	ImGui::SeparatorText("per-character assist levers");
	for (const char* k : { "assistEntry", "assistOffsetX", "assistEntryTicks", "assistCooldownTicks", "assistMaxTicks", "assistDamagePct" }) {
		const int i = FindLever(k);
		ImGui::PushID(i);
		std::string raw;
		bool own = g_ini.Get(SecKind::Char, g_char, k, raw);
		if (ImGui::Checkbox("##own", &own)) {
			if (own) SetCharLever(g_ini, g_char, i, mine[i]); else ClearCharLever(g_ini, g_char, i);
			Edited();
		}
		ImGui::SameLine();
		int32_t v = mine[i];
		if (!own) ImGui::BeginDisabled();
		if (ValueWidget(kLevers[i], v, 200) && own) { SetCharLever(g_ini, g_char, i, v); Edited(); }
		if (!own) ImGui::EndDisabled();
		LeverTooltip(kLevers[i], r.global[i], "global value");
		ImGui::SameLine();
		ImGui::TextUnformatted(k);
		ImGui::PopID();
	}
}

// ================== live ==================

const char* TagStateName(int32_t req)
{
	switch (req) {
	case 0: return "idle";
	case 100: return "exit";
	case 101: return "cooldown";
	case 150: return "forced tag-in pending";
	case 200: return "22D accepted / entering";
	case 254: return "swap hit (S1)";
	case 255: return "swap";
	case 256: return "swap (retry)";
	case 300: return "assist-enter";
	case 301: return "assist-act";
	case 302: return "assist-hit";
	case 303: return "assist-exit";
	}
	return "?";
}

void LiveTab(EditorContext& ctx, gamelink::Client& c, const gamelink::Snapshot& s)
{
	if (!s.connected) {
		ImGui::TextDisabled("Connect the Game Link (Windows > Game Link) to see the live tag state.");
		if (ImGui::Button("Connect")) c.Connect();
		return;
	}
	if (!s.haveState) { ImGui::TextDisabled("waiting for the game's state..."); return; }
	const auto& st = s.state;
	ImGui::Text("scene %s, frame %u, %s", st.scene == 1 ? "battle" : "menu", st.worldTimer, st.tagLive ? "TAG" : "not TAG");
	if (s.haveTag) {
		ImGui::Text("tuning: style '%s', sha %s, %u load(s), %u warning(s)%s", s.tag.activeStyle, s.tag.sha, s.tag.tuningLoads,
		            s.tag.warnings, s.tag.frozen ? ", FROZEN (session)" : "");
		const uint8_t cf = s.tag.tagConfig;
		ImGui::Text("session config: %s%s, partners %s/%s, KO rule %s", (cf & gamelink::wire::kTagCfgTag) ? "TAG" : "not TAG",
		            (cf & gamelink::wire::kTagCfgFromHost) ? " (the host's)" : "", (cf & gamelink::wire::kTagCfgPartner0) ? "P3" : "-",
		            (cf & gamelink::wire::kTagCfgPartner1) ? "P4" : "-", s.tag.koRule ? "allDown" : "oneDown");
	} else if (s.tagUnsupported) {
		ImGui::TextColored(kWarn, "This pchost.dll has no QueryTag (PovertyCaster mbaacc/link-tag): cooldown, raw tag state and");
		ImGui::TextColored(kWarn, "assist state need a newer DLL. Showing LinkState.");
	}
	for (int t = 0; t < 2; ++t) {
		ImGui::SeparatorText(t ? "Team 2" : "Team 1");
		const int point = st.teamActive[t];
		int reserve = -1;
		for (int k = 0; k < 4; ++k)
			if (st.actors[k].exists && st.actors[k].team == t && k != point) reserve = k;
		auto who = [&](int k) { return k >= 0 && k < 4 && st.actors[k].exists ? std::string("P") + std::to_string(k + 1) + " " + st.actors[k].file : std::string("-"); };
		ImGui::Text("point  %s", who(point).c_str());
		if (point >= 0 && point < 4 && st.actors[point].exists) {
			ImGui::SameLine();
			ImGui::TextDisabled("pattern %d frame %d", st.actors[point].pattern, st.actors[point].frame);
			ImGui::SameLine();
			ImGui::PushID(t);
			if (ImGui::SmallButton("edit this character")) g_char = Lower(st.actors[point].file);
			ImGui::PopID();
		}
		ImGui::Text("reserve %s%s", who(reserve).c_str(), reserve >= 0 && st.actors[reserve].tagFlag ? " (parked, tagFlag 1)" : "");
		if (s.haveTag) {
			const auto& tt = s.tag.team[t];
			ImGui::Text("tag state %d %s, counter %d", tt.tagRequest, TagStateName(tt.tagRequest), tt.counter);
			if (tt.tagInTick >= 0 && tt.tagInTick < 0x7FFF) { ImGui::SameLine(); ImGui::Text(", tag-in tick %d", tt.tagInTick); }
			else if (tt.tagInTick >= 0x7FFF) { ImGui::SameLine(); ImGui::TextDisabled(", past the tag-in window"); }
			if (tt.cooldownLeft > 0) { ImGui::SameLine(); ImGui::TextColored(kWarn, ", cooldown %d", tt.cooldownLeft); }
			const bool inAssist = tt.tagRequest >= 300 && tt.tagRequest <= 303;
			const char* modes[] = { "default", "pattern", "command", "motion" };
			const char* places[] = { "behind", "edge", "drop", "arc" };
			if (inAssist)
				ImGui::Text("assist: %s, %d+FN1 (%s, %s entry), tick %d, pattern %d, calls %d%s", TagStateName(tt.tagRequest),
				            kAssistDirs[(tt.assistSlot & 15) % 5], modes[(tt.assistSlot >> 4) & 3], places[tt.assistPlacement & 3],
				            tt.assistTick, tt.assistPattern, tt.assistCalls, (tt.assistFlags & 1) ? ", was hit" : "");
			else
				ImGui::Text("assist: idle, cooldown %d, calls %d this round", tt.assistCooldown, tt.assistCalls);
		} else {
			ImGui::Text("tag in progress: %s", st.teamTagRequest[t] ? "yes" : "no");
		}
	}
	ImGui::SeparatorText("slots");
	if (ImGui::BeginTable("slots", s.haveTag ? 8 : 6, ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit)) {
		const char* cols[] = { "slot", "file", "team", "tagFlag", "pattern/frame", "x,y", "hp/red", "tagIn/Out" };
		for (int k = 0; k < (s.haveTag ? 8 : 6); ++k) ImGui::TableSetupColumn(cols[k]);
		ImGui::TableHeadersRow();
		for (int k = 0; k < 4; ++k) {
			const auto& a = st.actors[k];
			ImGui::TableNextRow();
			ImGui::TableNextColumn(); ImGui::Text("P%d", k + 1);
			if (!a.exists) { ImGui::TableNextColumn(); ImGui::TextDisabled("(empty)"); continue; }
			ImGui::TableNextColumn(); ImGui::TextUnformatted(a.file);
			ImGui::TableNextColumn(); ImGui::Text("%d", a.team);
			ImGui::TableNextColumn(); ImGui::Text("%d", a.tagFlag);
			ImGui::TableNextColumn(); ImGui::Text("%d / %d", a.pattern, a.frame);
			ImGui::TableNextColumn(); ImGui::Text("%d,%d", a.x, a.y);
			if (s.haveTag) {
				ImGui::TableNextColumn(); ImGui::Text("%d/%d", s.tag.slot[k].health, s.tag.slot[k].red);
				ImGui::TableNextColumn(); ImGui::Text("%d/%d", s.tag.slot[k].tagIn, s.tag.slot[k].tagOut);
			}
		}
		ImGui::EndTable();
	}
	// follow point
	ImGui::Checkbox("Follow point", &g_follow);
	ImGui::SameLine();
	ImGui::SetNextItemWidth(90);
	const char* teams[] = { "Team 1", "Team 2" };
	ImGui::Combo("##fteam", &g_followTeam, teams, 2);
	if (!g_follow) return;
	const int point = st.teamActive[g_followTeam];
	if (point < 0 || point > 3 || !st.actors[point].exists) { ImGui::TextDisabled("no point"); return; }
	const auto& a = st.actors[point];
	const bool matches = !ctx.activeKey.empty() && (gamelink::SlotMaskForFile(ctx.activeKey, st) & (1u << point));
	if (!matches) {
		ImGui::TextColored(kWarn, "The point is P%d %s; the editor shows %s. Open that character to follow it.", point + 1,
		                   a.file, ctx.activeKey.empty() ? "nothing" : ctx.activeKey.c_str());
		return;
	}
	ImGui::Text("following P%d %s: pattern %d frame %d", point + 1, a.file, a.pattern, a.frame);
	if (!ctx.activeState || (a.pattern == g_lastFollow && a.frame == g_lastFollowFrame)) return;
	g_lastFollow = a.pattern;
	g_lastFollowFrame = a.frame;
	const int n = ctx.patternCount ? ctx.patternCount() : 0;
	if (a.pattern < 0 || a.pattern >= n) return;
	FrameState& fs = *ctx.activeState;
	fs.animating = false;
	fs.pattern = a.pattern;
	const int frames = ctx.frameCount ? ctx.frameCount(a.pattern) : 0;
	fs.frame = frames > 0 ? std::min(a.frame, frames - 1) : 0;
	fs.currentTick = 0;
}

void RawTab()
{
	ImGui::TextDisabled("The file as it will be written (read-only here; comments and unknown keys are kept byte for byte).");
	std::string t = g_ini.Text();
	ImGui::InputTextMultiline("##raw", t.data(), t.size() + 1, ImVec2(-1, -1), ImGuiInputTextFlags_ReadOnly);
}

void PollDisk()
{
	const uint64_t now = NowMs();
	if (g_path.empty() || now - g_lastStatMs < 1000) return;
	g_lastStatMs = now;
	bool ex = false;
	fs::file_time_type t{};
	uintmax_t sz = 0;
	StatDisk(ex, t, sz);
	if (ex == g_existsOnDisk && (!ex || (t == g_diskTime && sz == g_diskSize))) return;
	if (!g_ini.IsDirty()) LoadFromDisk("reloaded (changed on disk)");
	else g_diskChanged = true;
}

} // namespace

void StartupApply() { RequestApply(); }

void OpenStartup(const std::string& iniPath, const std::string& charFile, const std::string& tab)
{
	showPanel = true;
	if (!iniPath.empty()) { g_path = iniPath; g_pathExplicit = true; LoadFromDisk("opened"); }
	if (!charFile.empty()) g_char = Lower(charFile);
	g_startTab = tab;
}

void DrawPanel(EditorContext& ctx)
{
	static bool wasOpen = false;
	if (!showPanel) {
		if (wasOpen) gamelink::SharedClient().SetTagQuery(false);
		wasOpen = false;
		return;
	}
	wasOpen = true;
	g_activeTxt = ctx.activeTxtPath;
	gamelink::Client& c = gamelink::SharedClient();
	c.SetTagQuery(true);
	const gamelink::Snapshot s = c.Get();
	PollDisk();
	PumpApply(c, s);
	if (g_pendingApply && NowMs() - g_editMs > 400) { g_pendingApply = false; RequestApply(); }

	// relative to the main viewport: with detachable windows a fixed position could open it as its own OS window
	const ImVec2 mainPos = ImGui::GetMainViewport()->Pos;
	ImGui::SetNextWindowPos(ImVec2(mainPos.x + 60.0f, mainPos.y + 40.0f), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(760, 720), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Tag / Team (experimental)", &showPanel)) { ImGui::End(); return; }
	Header(c, s);
	ImGui::Separator();
	if (ImGui::BeginTabBar("tagtabs")) {
		auto tab = [&](const char* name) {
			const bool sel = !g_startTab.empty() && ieq(g_startTab, name);
			return ImGui::BeginTabItem(name, nullptr, sel ? ImGuiTabItemFlags_SetSelected : 0);
		};
		if (tab("Global")) { ImGui::BeginChild("g"); GlobalTab(); ImGui::EndChild(); ImGui::EndTabItem(); }
		if (tab("Character")) { ImGui::BeginChild("c"); CharacterTab(ctx, s); ImGui::EndChild(); ImGui::EndTabItem(); }
		if (tab("Assists")) { ImGui::BeginChild("a"); AssistTab(ctx, s); ImGui::EndChild(); ImGui::EndTabItem(); }
		if (tab("Live")) { ImGui::BeginChild("l"); LiveTab(ctx, c, s); ImGui::EndChild(); ImGui::EndTabItem(); }
		if (tab("Raw")) { RawTab(); ImGui::EndTabItem(); }
		ImGui::EndTabBar();
	}
	g_startTab.clear();
	ImGui::End();
}

} // namespace tagpanel
