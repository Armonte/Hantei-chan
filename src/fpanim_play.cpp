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

struct Shared {
	std::mutex mu;
	std::string reply;
	bool ok = false;
	std::atomic<bool> busy{false};
};
Shared& shared() { static Shared s; return s; }

void runPlay(std::string pipe, std::string chr, bool reload, bool live, int pattern)
{
	Shared& s = shared();
	std::string out;
	bool ok = true;
	if (reload) {
		const auto r = sqedit::FpanimRequest(pipe, sqedit::FpanimReloadRequest(chr, live), live ? 35000 : 130000);
		out = (r.connected ? r.text : std::string("not connected: ") + r.text) + "\n";
		ok = r.ok;
	}
	if (ok) {
		const auto r = sqedit::FpanimRequest(pipe, PlayRequest(pattern), 8000);
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

std::string PlayRequest(int pattern) { return "playpattern 1 " + std::to_string(pattern); }

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
	ImGui::BeginDisabled(busy);
	bool go = ImGui::Button(LBL("Play in game"));
	ImGui::EndDisabled();
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		ImGui::SetTooltip("%s", TXT("P1 plays this pattern in the running game (fpan-im, training battle only). With script hot reload on, the character is reloaded first so the saved edit plays. Shortcut: F5"));
	if (!busy && !ImGui::GetIO().WantTextInput && ImGui::IsKeyPressed(ImGuiKey_F5, false)) go = true;
	if (go && !busy) {
		const std::string chr = CharaFolderFromPath(g_file);
		const bool reload = gSettings.sqHotReload && !chr.empty();
		s.busy.store(true);
		{
			std::lock_guard<std::mutex> lk(s.mu);
			s.reply = TXT("Sending...");
		}
		std::thread(runPlay, sqedit::FpanimPipeForGame(mbtl ? "mbtl" : "uni2"), chr, reload, gSettings.sqHotReloadMode == 1,
		            pattern).detach();
	}
	std::string reply;
	bool ok;
	{
		std::lock_guard<std::mutex> lk(s.mu);
		reply = s.reply;
		ok = s.ok;
	}
	if (!reply.empty()) {
		ImGui::PushStyleColor(ImGuiCol_Text, busy ? ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled)
		                                          : (ok ? ImVec4(0.4f, 0.9f, 0.4f, 1.0f) : ImVec4(1.0f, 0.45f, 0.4f, 1.0f)));
		ImGui::TextWrapped("%s", reply.c_str());
		ImGui::PopStyleColor();
	}
}

} // namespace fpanimplay
