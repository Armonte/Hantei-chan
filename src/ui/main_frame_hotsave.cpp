// Main-editor saves into the UNI2 / MBTL improvement mod folder, and fpan-im hot reload on save from anywhere
// (HA6 Save / Ctrl+S, the PAT editor, the CG manager's bank / .pat / .pal saves). The path mapping and the request
// choice are sqedit::FpanimPlanSave (squirrel/fpanim_link.h, unit tested); the pipe call runs on a worker thread.
// Settings: gSettings.sqHotReload / sqHotReloadMode (shared with the Script panel's Hot reload row),
// gSettings.imSaveUni2 / imSaveMbtl, and the game folders sqGameDirUni2 / sqGameDirMbtl.
#include "../main_frame.h"
#include "../ini.h"
#include "../i18n.h"
#include "../app_log.h"
#include "../png_writer.h"
#include "../character_instance.h"
#include "../squirrel/fpanim_link.h"
#include "../fpanim_play.h"
#include "../cgm/cgm_texsrc.h"
#include "ui_style.h"

#include <imgui.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <future>
#include <iterator>
#include <set>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

struct HotJob { std::string pipe, request, chr; };
struct HotDone { sqedit::FpanimReply reply; HotJob job; long ms = 0; };

struct HotSaveState {
	std::future<HotDone> job;
	bool busy = false;
	std::vector<HotJob> pending;    // saves while a reload runs (one per distinct request, latest kept)
	std::string saved;              // "Saved into the mod folder: ..." / "Saved ..."
	std::string reload;             // the summarised reply
	std::string raw;                // the raw fpan-im reply (tooltip)
	bool ok = true;
	std::set<std::string> stale;    // chrNNN saved into the mod folder since its last successful reload (Play in game)
} g_hot;

HotDone RunJob(HotJob j)
{
	const auto t0 = Clock::now();
	HotDone d;
	d.job = j;
	// Engine rebuild: fpan-im waits up to 120 s for the training battle; live: up to 30 s (as the Script panel).
	if (j.request == "reloadsyseff") d.reply = sqedit::FpanimRequestSysEffectReload(j.pipe, 35000);
	else d.reply = sqedit::FpanimRequest(j.pipe, j.request, j.request.find(" live") != std::string::npos ? 35000 : 130000);
	d.ms = (long)std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - t0).count();
	return d;
}

void Start(const HotJob& j)
{
	if (g_hot.busy) {
		for (auto& p : g_hot.pending) if (p.request == j.request && p.pipe == j.pipe) return;
		g_hot.pending.push_back(j);
		return;
	}
	g_hot.busy = true;
	g_hot.ok = true;
	g_hot.raw.clear();
	g_hot.reload = std::string(TXT("Hot reload: ")) + j.request + "...";
	g_hot.job = std::async(std::launch::async, RunJob, j);
}

std::string Summarise(const HotDone& d, bool* ok)
{
	const sqedit::FpanimReloadInfo info = sqedit::FpanimParseReloadReply(d.reply.text);
	char line[512];
	*ok = d.reply.ok;
	if (!d.reply.connected) { *ok = false; return TXT("Hot reload: game not running (no fpan-im pipe)"); }
	if (info.refused) { *ok = false; return TXT("Hot reload refused: no training battle is running. Start a training battle in the game, then save again."); }
	if (d.reply.ok && info.sysEffect) {
		snprintf(line, sizeof line, TXT("Hot reload: system effects reloaded in place (%ld ms)"), d.ms);
		return line;
	}
	if (d.reply.ok && info.stdReload) {
		if (info.engine) snprintf(line, sizeof line, TXT("Hot reload: shared scripts reloaded, battle rebuilt in %ld ms"), info.ms);
		else snprintf(line, sizeof line, TXT("Hot reload: shared scripts and every character reloaded in place (%ld ms)"), d.ms);
		return line;
	}
	if (info.engine && info.hasRoots) {
		snprintf(line, sizeof line, info.backInBattle ? TXT("Hot reload: rebuilt %s in %ld ms, HA6 root %08X -> %08X") : TXT("Hot reload: the rebuild of %s did not return to the training battle (%ld ms), HA6 root %08X -> %08X"),
			d.job.chr.c_str(), info.ms, (unsigned)info.rootBefore, (unsigned)info.rootAfter);
		if (!info.backInBattle) *ok = false;
		return line;
	}
	if (d.reply.ok) { snprintf(line, sizeof line, TXT("Hot reload: %s reloaded in place (%ld ms)"), d.job.chr.empty() ? "?" : d.job.chr.c_str(), d.ms); return line; }
	return std::string(TXT("Hot reload: ")) + d.reply.text;
}

void Poll()
{
	if (!g_hot.busy || g_hot.job.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;
	const HotDone d = g_hot.job.get();
	g_hot.busy = false;
	g_hot.raw = d.reply.text;
	g_hot.reload = Summarise(d, &g_hot.ok);
	if (g_hot.ok) {   // what the game now runs: a character reload clears that character, a shared reload every one
		if (d.job.request.rfind("reloadstd", 0) == 0) g_hot.stale.clear();
		else if (!d.job.chr.empty()) g_hot.stale.erase(d.job.chr);
	}
	HC_LOG_INFO("[hotsave] {} -> {} ({} ms)", d.job.request, d.reply.text, d.ms);
	if (!g_hot.pending.empty()) { const HotJob next = g_hot.pending.front(); g_hot.pending.erase(g_hot.pending.begin()); Start(next); }
}

// "uni2" / "mbtl" for a character of those games, "" otherwise (no mod folder, no fpan-im).
std::string GameOf(CharacterInstance* ch, const std::string& u8path)
{
	std::string p = u8path;
	for (auto& c : p) c = c == '\\' ? '/' : (char)tolower((unsigned char)c);
	if (ch && !ch->isPatDocument()) {
		switch (ch->frameData.game()) {
		case Ha6Game::MBTL: return "mbtl";
		case Ha6Game::UNI: case Ha6Game::UNIST: return p.find("unib") != std::string::npos ? "" : "uni2";
		default: break;
		}
	}
	// a PAT tab / a file without a game header: the folder says (an archive working copy is %TEMP%/hantei_archive/<game>_<archive>)
	if (p.find("/uni2") != std::string::npos || p.find("uni2_") != std::string::npos || p.find("/uni2-im/") != std::string::npos ||
	    p.find("under night in-birth ii") != std::string::npos) return "uni2";
	if (p.find("/mbtl") != std::string::npos || p.find("mbtl_") != std::string::npos || p.find("melty blood type lumina") != std::string::npos) return "mbtl";
	return "";
}

bool ReadAll(const std::string& u8, std::vector<uint8_t>& out)
{
	std::ifstream f(std::filesystem::u8path(u8), std::ios::binary);
	if (!f) return false;
	out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
	return true;
}

} // namespace

namespace fpanimplay {
bool HotSaveBusy() { Poll(); return g_hot.busy || !g_hot.pending.empty(); }
bool HotSaveStale(const std::string& chr) { return g_hot.stale.count(chr) != 0; }
void HotSaveMarkReloaded(const std::string& chr) { g_hot.stale.erase(chr); }
} // namespace fpanimplay

std::string HotSaveGameDir(const std::string& game)
{
	const std::string& s = game == "mbtl" ? gSettings.sqGameDirMbtl : gSettings.sqGameDirUni2;
	if (!s.empty()) return s;
	if (game == "mbtl") return "C:\\Program Files (x86)\\Steam\\steamapps\\common\\MELTY BLOOD TYPE LUMINA";
	if (game == "uni2") return "C:\\Program Files (x86)\\Steam\\steamapps\\common\\UNDER NIGHT IN-BIRTH II Sys Celes";
	return "";
}

// Default on when the game folder is known (it exists); the per-game setting turns it off.
static bool HotSaveToMods(const std::string& game, const std::string& gameDir)
{
	const bool on = game == "mbtl" ? gSettings.imSaveMbtl : game == "uni2" ? gSettings.imSaveUni2 : false;
	std::error_code ec;
	return on && !gameDir.empty() && std::filesystem::is_directory(std::filesystem::u8path(gameDir), ec);
}

void MainFrame::onFileSaved(CharacterInstance* ch, const std::string& u8path)
{
	if (u8path.empty()) return;
	const std::string game = GameOf(ch, u8path);
	if (game.empty()) return;
	const std::string dir = HotSaveGameDir(game);
	const sqedit::FpanimSavePlan plan = sqedit::FpanimPlanSave(game, dir, u8path, HotSaveToMods(game, dir), gSettings.sqHotReload, gSettings.sqHotReloadMode == 1);
	if (plan.key.empty()) return;
	if (!plan.modsDst.empty() && !plan.alreadyInMods) {
		std::vector<uint8_t> bytes;
		std::string err;
		std::string dst = plan.modsDst;
		for (auto& c : dst) if (c == '/') c = '\\';
		if (!ReadAll(u8path, bytes)) err = "cannot read " + u8path;
		else if (!cgm::WriteModFile(dst, bytes, &err)) {}
		if (err.empty()) {
			if (!plan.chr.empty()) g_hot.stale.insert(plan.chr);
			g_hot.saved = std::string(TXT("Saved into the mod folder: ")) + dst;
			HC_LOG_INFO("[hotsave] {} -> {} ({} bytes)", u8path, dst, bytes.size());
		} else {
			g_hot.saved = std::string(TXT("Mod folder copy failed: ")) + err;
			g_hot.ok = false;
			HC_LOG_WARN("[hotsave] {}: {}", u8path, err);
			return;
		}
	} else {
		if (plan.alreadyInMods && !plan.chr.empty()) g_hot.stale.insert(plan.chr);
		g_hot.saved = std::string(TXT("Saved ")) + (plan.alreadyInMods ? plan.modsDst : u8path);
	}
	if (plan.request.empty()) {
		g_hot.reload = gSettings.sqHotReload ? TXT("Hot reload: not a character file (data/<character>/<character>...), shared file or system effect file, not reloaded") : "";
		return;
	}
	if (plan.modsDst.empty()) {   // the game reads its archives: a reload without the mod-folder copy would load the old file
		g_hot.reload = TXT("Hot reload skipped: the file is not in the IM mod folder (turn on \"UNI2/MBTL: save into the IM mod folder\" in Preferences, or set the game folder)");
		g_hot.ok = false;
		return;
	}
	Start({plan.pipe, plan.request, plan.chr});
}

void MainFrame::DrawHotSaveStatus()
{
	Poll();
	if (g_hot.saved.empty() && g_hot.reload.empty()) return;
	ImGui::SameLine();
	const ImVec4 good(0.4f, 0.8f, 0.4f, 1.0f), bad(1.0f, 0.45f, 0.4f, 1.0f);
	const std::string line = g_hot.saved + (g_hot.reload.empty() ? "" : (g_hot.saved.empty() ? "" : " | ") + g_hot.reload);
	ImGui::TextColored(g_hot.ok ? good : bad, "%s", line.c_str());
	if (!g_hot.raw.empty() && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", g_hot.raw.c_str());   // the raw fpan-im reply
}

void MainFrame::DrawHotSavePrefsItems()
{
	if (ImGui::Checkbox(LBL("UNI2: save into the IM mod folder"), &gSettings.imSaveUni2)) ImGui::MarkIniSettingsDirty();
	ImGui::SetItemTooltip("%s", TXT("Every save of a UNI2 character file (HA6, .pat, .cg, .pal) is also written to <game folder>\\UNI2-IM\\Mods at the same data\\... path, which the improvement mod and fpan-im load in place of the read-only archive. Needs the game folder."));
	if (ImGui::Checkbox(LBL("MBTL: save into the IM mod folder"), &gSettings.imSaveMbtl)) ImGui::MarkIniSettingsDirty();
	ImGui::SetItemTooltip("%s", TXT("The same for MBTL: <game folder>\\MBTL-IM\\Mods."));
	for (const char* g : {"uni2", "mbtl"}) {
		std::string& s = std::string(g) == "mbtl" ? gSettings.sqGameDirMbtl : gSettings.sqGameDirUni2;
		char buf[512];
		snprintf(buf, sizeof buf, "%s", HotSaveGameDir(g).c_str());
		ImGui::SetNextItemWidth(ui::Px(360));
		if (ImGui::InputText(std::string(g) == "mbtl" ? LBL("MBTL game folder") : LBL("UNI2 game folder"), buf, sizeof buf)) { s = buf; ImGui::MarkIniSettingsDirty(); }
	}
	if (ImGui::Checkbox(LBL("Hot reload"), &gSettings.sqHotReload)) ImGui::MarkIniSettingsDirty();
	ImGui::SetItemTooltip("%s", TXT("After a save of a UNI2 / MBTL file, reload it in the running game through fpan-im: data/chrNNN files reload that character, shared files (script/*.txt, data/_combase, VectorTable, BattleInfo, BoundEff) reload the shared battle scripts, and the system effects (data/effect.ha6 / .cg / .txt, sys_effect.pat) reload in place. The same switch as the Script panel's."));
	const char* modes[] = {TXT("Rebuild (engine)"), TXT("Live (keep state)")};
	int mode = gSettings.sqHotReloadMode == 1 ? 1 : 0;
	ImGui::SetNextItemWidth(ui::Px(180));
	if (ImGui::Combo(LBL("Reload mode"), &mode, modes, 2)) { gSettings.sqHotReloadMode = mode; ImGui::MarkIniSettingsDirty(); }
}

// --ui-script: hotsave status | on | off | mode rebuild|live | tomods uni2|mbtl on|off | gamedir uni2|mbtl <dir>
//              framedur <pattern> <frame> <ticks> (the active character, marks the pattern edited)
bool MainFrame::HotSaveScriptCommand(const std::string& cmd, const std::string& arg, std::string* msg)
{
	auto say = [&](const std::string& m) { if (msg) *msg = m; };
	if (cmd == "framedur") {
		int p = 0, f = 0, n = 0;
		CharacterInstance* ch = getActiveCharacter();
		if (!ch || sscanf(arg.c_str(), "%d %d %d", &p, &f, &n) != 3) { say("usage: framedur <pattern> <frame> <ticks>"); return true; }
		Sequence* s = ch->frameData.get_sequence(p);
		if (!s || f < 0 || f >= (int)s->frames.size()) { say("no such frame"); return true; }
		const int was = s->frames[f].AF.duration;
		s->frames[f].AF.duration = n;
		ch->frameData.mark_modified(p);
		ch->markModified();
		say("pattern " + std::to_string(p) + " frame " + std::to_string(f) + " duration " + std::to_string(was) + " -> " + std::to_string(n));
		return true;
	}
	if (cmd != "hotsave") return false;
	Poll();
	const size_t sp = arg.find(' ');
	const std::string verb = arg.substr(0, sp), rest = sp == std::string::npos ? "" : arg.substr(sp + 1);
	if (verb == "on" || verb == "off") gSettings.sqHotReload = verb == "on";
	else if (verb == "mode") gSettings.sqHotReloadMode = rest == "live" ? 1 : 0;
	else if (verb == "tomods") { const bool on = rest.find(" on") != std::string::npos; (rest.rfind("mbtl", 0) == 0 ? gSettings.imSaveMbtl : gSettings.imSaveUni2) = on; }
	else if (verb == "gamedir") { const size_t s2 = rest.find(' '); if (s2 != std::string::npos) (rest.substr(0, s2) == "mbtl" ? gSettings.sqGameDirMbtl : gSettings.sqGameDirUni2) = rest.substr(s2 + 1); }
	say(std::string(g_hot.busy ? "busy: " : "") + (g_hot.ok ? "ok: " : "error: ") + g_hot.saved + " | " + g_hot.reload + (g_hot.raw.empty() ? "" : " | reply: " + g_hot.raw) +
	    " [hot reload " + (gSettings.sqHotReload ? "on" : "off") + ", mode " + (gSettings.sqHotReloadMode == 1 ? "live" : "rebuild") + "]");
	return true;
}
