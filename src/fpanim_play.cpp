#include "fpanim_play.h"

#include "framedata.h"
#include "i18n.h"
#include "ini.h"
#include "app_log.h"
#include "squirrel/fpanim_link.h"

#include <imgui.h>

#include <atomic>
#include <cctype>
#include <memory>
#include <mutex>
#include <thread>

namespace fpanimplay {
namespace {

std::string g_file;
bool g_loop = false;          // the Loop checkbox
bool g_looping = false;       // a loop request was sent and not stopped
bool g_queued = false;        // pressed while a save's hot reload runs: fires when it ends
bool g_scriptPress = false;   // --ui-script playingame
bool g_scriptStop = false;    // --ui-script playstop
std::string g_reloadedChr;    // set by the worker after its own successful reload (UI thread clears the stale mark)

struct Shared {
	std::mutex mu;
	std::string reply;
	bool ok = false;
	std::atomic<bool> busy{false};
	std::string reloadedChr;
};
Shared& shared() { static Shared s; return s; }

void runStop(std::string pipe)
{
	Shared& s = shared();
	const auto r = sqedit::FpanimRequest(pipe, StopRequest(), 8000);
	const std::string out = r.connected ? r.text : std::string("not connected: ") + r.text;
	HC_LOG_INFO("[fpan-im] play in game: {}", out);
	std::lock_guard<std::mutex> lk(s.mu);
	s.reply = out;
	s.ok = r.ok;
	s.busy.store(false);
}

void runPlay(std::string pipe, std::string chr, bool reload, bool live, int pattern, bool loop)
{
	Shared& s = shared();
	std::string out;
	bool ok = true;
	if (reload) {
		const auto r = sqedit::FpanimRequest(pipe, sqedit::FpanimReloadRequest(chr, live), live ? 35000 : 130000);
		out = (r.connected ? r.text : std::string("not connected: ") + r.text) + "\n";
		ok = r.ok;
		if (ok) { std::lock_guard<std::mutex> lk(s.mu); s.reloadedChr = chr; }
	}
	if (ok) {
		const auto r = sqedit::FpanimRequest(pipe, PlayRequest(pattern, loop), 8000);
		out += r.connected ? r.text : std::string("not connected: ") + r.text;
		ok = r.ok;
	}
	HC_LOG_INFO("[fpan-im] play in game: {}", out);
	std::lock_guard<std::mutex> lk(s.mu);
	s.reply = out;
	s.ok = ok;
	s.busy.store(false);
}

} // namespace

void SetCurrentFile(const std::string& path) { g_file = path; }

std::string CharaFolderFromPath(const std::string& path)
{
	std::string best;
	size_t i = 0;
	while (i < path.size()) {
		size_t j = path.find_first_of("/\\", i);
		if (j == std::string::npos) j = path.size();
		std::string part = path.substr(i, j - i);
		for (char& c : part) c = (char)tolower((unsigned char)c);
		bool chr = part.size() > 3 && part.compare(0, 3, "chr") == 0;
		for (size_t k = 3; chr && k < part.size(); ++k) chr = isdigit((unsigned char)part[k]) != 0;
		if (chr && j < path.size()) best = part;  // a folder, not the file name
		i = j + 1;
	}
	return best;
}

std::string PlayRequest(int pattern, bool loop) { return "playpattern 1 " + std::to_string(pattern) + (loop ? " loop" : ""); }
std::string StopRequest() { return "playpattern stop"; }

bool Busy() { return shared().busy.load() || g_queued || g_scriptPress || g_scriptStop; }
std::string LastReply()
{
	Shared& s = shared();
	std::lock_guard<std::mutex> lk(s.mu);
	return s.reply;
}

bool ScriptCommand(const std::string& cmd, const std::string& arg, std::string* msg)
{
	auto say = [&](const std::string& m) { if (msg) *msg = m; };
	if (cmd == "playingame") {
		if (arg == "loop") g_loop = true;
		else if (arg == "noloop") g_loop = false;
		g_scriptPress = true;
		say(std::string("pressed (loop ") + (g_loop ? "on" : "off") + ")");
		return true;
	}
	if (cmd == "playloop") { g_loop = arg == "on"; say(std::string("loop ") + (g_loop ? "on" : "off")); return true; }
	if (cmd == "playstop") { g_scriptStop = true; say("stop"); return true; }
	return false;
}

bool GameSupported(const FrameData* fd)
{
	if (!fd || !fd->m_loaded) return false;
	const Ha6Game g = fd->detectedGame();
	return g == Ha6Game::UNI || g == Ha6Game::MBTL;
}

void DrawPlayButton(FrameData* fd, int pattern)
{
	if (!GameSupported(fd)) return;
	Shared& s = shared();
	const bool busy = s.busy.load();
	const bool mbtl = fd->detectedGame() == Ha6Game::MBTL;
	const std::string pipe = sqedit::FpanimPipeForGame(mbtl ? "mbtl" : "uni2");
	if (!busy) {
		std::string done;
		{
			std::lock_guard<std::mutex> lk(s.mu);
			done.swap(s.reloadedChr);
		}
		if (!done.empty()) HotSaveMarkReloaded(done);
	}
	ImGui::BeginDisabled(busy || g_queued);
	bool go = ImGui::Button(LBL("Play in game"));
	ImGui::EndDisabled();
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		ImGui::SetTooltip("%s", TXT("P1 plays this pattern in the running game (fpan-im, training battle only). A save already hot reloads the character; Play waits for that reload, and reloads the character itself only when a save was not reloaded yet. Shortcut: F5"));
	ImGui::SameLine();
	bool stop = false;
	if (ImGui::Checkbox(LBL("Loop"), &g_loop) && !g_loop && g_looping) stop = true;
	ImGui::SetItemTooltip("%s", TXT("Replay the pattern each time P1 returns to neutral, until Loop is unchecked or another command is sent."));
	if (g_scriptStop && !busy) { g_scriptStop = false; stop = true; }
	if (!busy && !g_queued && !ImGui::GetIO().WantTextInput && ImGui::IsKeyPressed(ImGuiKey_F5, false)) go = true;
	if (g_scriptPress && !busy) { g_scriptPress = false; go = true; }
	if (stop && !busy) {
		g_looping = false;
		s.busy.store(true);
		{
			std::lock_guard<std::mutex> lk(s.mu);
			s.reply = TXT("Sending...");
		}
		std::thread(runStop, pipe).detach();
	} else if (go && !busy) {
		g_queued = true;
		std::lock_guard<std::mutex> lk(s.mu);
		s.reply = HotSaveBusy() ? TXT("Waiting for the save's hot reload...") : TXT("Sending...");
	}
	if (g_queued && !s.busy.load() && !HotSaveBusy()) {
		g_queued = false;
		const std::string chr = CharaFolderFromPath(g_file);
		const bool reload = !chr.empty() && HotSaveStale(chr);
		g_looping = g_loop;
		s.busy.store(true);
		{
			std::lock_guard<std::mutex> lk(s.mu);
			s.reply = TXT("Sending...");
		}
		std::thread(runPlay, pipe, chr, reload, gSettings.sqHotReloadMode == 1, pattern, g_loop).detach();
	}
	std::string reply;
	bool ok;
	{
		std::lock_guard<std::mutex> lk(s.mu);
		reply = s.reply;
		ok = s.ok;
	}
	if (!reply.empty()) {
		const bool waiting = s.busy.load() || g_queued;
		ImGui::PushStyleColor(ImGuiCol_Text, waiting ? ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled)
		                                             : (ok ? ImVec4(0.4f, 0.9f, 0.4f, 1.0f) : ImVec4(1.0f, 0.45f, 0.4f, 1.0f)));
		ImGui::TextWrapped("%s", reply.c_str());
		ImGui::PopStyleColor();
	}
}

} // namespace fpanimplay
