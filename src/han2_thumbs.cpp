#include "han2_thumbs.h"
#include "character_instance.h"
#include "framestate.h"
#include "han2_browser.h"
#include "han2_export.h"
#include "i18n.h"

#include <glad/glad.h>
#include <imgui.h>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <unordered_map>
#include <vector>

namespace han2ui {

bool showAnimList = false;
unsigned dockAnimListId = 0;

namespace {
struct Entry { unsigned tex = 0; int w = 0, h = 0; uint64_t version = 0; bool failed = false; uint64_t used = 0; };
std::unordered_map<uint64_t, Entry> g_cache;
uint64_t g_clock = 0;
int g_budget = 0;
uint64_t Key(const CharacterInstance *c, int p, int f) { return ((uint64_t)(uintptr_t)c * 1000003ull) ^ ((uint64_t)(uint32_t)p << 20) ^ (uint64_t)(uint32_t)f ^ ((uint64_t)(uintptr_t)c << 40); }
}

void BeginThumbFrame() { g_budget = 3; }

void ForgetCharacterThumbs(const CharacterInstance *)
{
	for (auto &kv : g_cache) if (kv.second.tex) { GLuint t = kv.second.tex; glDeleteTextures(1, &t); }
	g_cache.clear();
}

bool FrameThumb(CharacterInstance &ch, int pattern, int frame, unsigned &tex, int &w, int &h)
{
	const uint64_t k = Key(&ch, pattern, frame);
	auto it = g_cache.find(k);
	const uint64_t ver = ch.frameData.dataVersion;
	if (it != g_cache.end() && it->second.version == ver) {
		it->second.used = ++g_clock;
		if (it->second.failed || !it->second.tex) return false;
		tex = it->second.tex; w = it->second.w; h = it->second.h; return true;
	}
	if (g_budget <= 0) return false;
	g_budget--;
	Entry &e = g_cache[k];
	if (e.tex) { GLuint t = e.tex; glDeleteTextures(1, &t); e.tex = 0; }
	e.version = ver; e.failed = true; e.used = ++g_clock;
	std::vector<uint8_t> px; int tw = 0, th = 0;
	if (han2::RenderFrameThumb(ch.frameData, ch.cg, ch.parts, pattern, frame, 80, px, tw, th)) {
		GLuint t = 0; glGenTextures(1, &t); glBindTexture(GL_TEXTURE_2D, t);
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tw, th, 0, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
		e.tex = t; e.w = tw; e.h = th; e.failed = false;
	}
	if (g_cache.size() > 1500) {   // keep the newest 1000
		std::vector<std::pair<uint64_t, uint64_t>> v; for (auto &kv : g_cache) v.push_back({ kv.second.used, kv.first });
		std::sort(v.begin(), v.end());
		for (size_t i = 0; i < 500 && i < v.size(); i++) { Entry &o = g_cache[v[i].second]; if (o.tex) { GLuint t = o.tex; glDeleteTextures(1, &t); } g_cache.erase(v[i].second); }
	}
	auto again = g_cache.find(k);
	if (again == g_cache.end() || again->second.failed || !again->second.tex) return false;
	tex = again->second.tex; w = again->second.w; h = again->second.h; return true;
}

void DrawAnimListWindow(CharacterInstance *ch, FrameState &state)
{
	if (!showAnimList) return;
	ImGui::SetNextWindowSize(ImVec2(300, 520), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Animations###animlist", &showAnimList)) { ImGui::End(); return; }
	if (!ch || ch->frameData.m_sequences.empty()) { ImGui::TextDisabled("%s", Tr("No character open.", "\xe3\x82\xad\xe3\x83\xa3\xe3\x83\xa9\xe3\x81\x8c\xe9\x96\x8b\xe3\x81\x84\xe3\x81\xa6\xe3\x81\x84\xe3\x81\xbe\xe3\x81\x9b\xe3\x82\x93\xe3\x80\x82")); ImGui::End(); return; }
	static char filter[64] = "";
	ImGui::SetNextItemWidth(-1);
	ImGui::InputTextWithHint("##animfilter", Tr("Search animations", "\xe3\x82\xa2\xe3\x83\x8b\xe3\x83\xa1\xe3\x82\xa4\xe3\x82\xb7\xe3\x83\xa7\xe3\x83\xb3\xe3\x82\x92\xe6\xa4\x9c\xe7\xb4\xa2"), filter, sizeof filter);
	std::string flt(filter); for (auto &c : flt) c = (char)tolower((unsigned char)c);
	std::vector<int> rows;
	for (int i = 0; i < (int)ch->frameData.m_sequences.size(); i++) {
		const Sequence &sq = ch->frameData.m_sequences[i];
		if (sq.frames.empty()) continue;
		if (!flt.empty()) { std::string n = sq.name; for (auto &c : n) c = (char)tolower((unsigned char)c); if (n.find(flt) == std::string::npos && std::to_string(i).find(flt) == std::string::npos) continue; }
		rows.push_back(i);
	}
	ImGui::TextDisabled("%zu %s", rows.size(), Tr("animations", "\xe3\x82\xa2\xe3\x83\x8b\xe3\x83\xa1"));
	ImGui::BeginChild("##animrows", ImVec2(0, 0), false);
	const float rowH = 46.f;
	{   // follow the selection made elsewhere (combo box, playback jumping patterns)
		static int lastSel = -1;
		if (state.pattern != lastSel) {
			lastSel = state.pattern;
			const auto it = std::find(rows.begin(), rows.end(), state.pattern);
			if (it != rows.end()) { const float y = (float)(it - rows.begin()) * rowH; if (y < ImGui::GetScrollY() || y > ImGui::GetScrollY() + ImGui::GetWindowHeight() - 2 * rowH) ImGui::SetScrollY(std::max(0.f, y - ImGui::GetWindowHeight() * 0.35f)); }
		}
	}
	ImGuiListClipper clip; clip.Begin((int)rows.size(), rowH);
	while (clip.Step()) for (int r = clip.DisplayStart; r < clip.DisplayEnd; r++) {
		const int p = rows[r]; const Sequence &sq = ch->frameData.m_sequences[p];
		ImGui::PushID(p);
		const ImVec2 q = ImGui::GetCursorScreenPos();
		if (ImGui::Selectable("##r", state.pattern == p, 0, ImVec2(0, rowH - 2))) { state.pattern = p; state.frame = 0; }
		ImDrawList *dl = ImGui::GetWindowDrawList();
		unsigned tex; int w, h;
		if (FrameThumb(*ch, p, 0, tex, w, h)) { const float z = std::min(40.f / w, 40.f / h); const ImVec2 sz(w * z, h * z); const ImVec2 a(q.x + 2 + (40 - sz.x) * 0.5f, q.y + 2 + (40 - sz.y) * 0.5f); dl->AddImage((ImTextureID)(intptr_t)tex, a, ImVec2(a.x + sz.x, a.y + sz.y)); }
		char l[160]; snprintf(l, sizeof l, "%03d  %s", p, sq.name.c_str());
		dl->AddText(ImVec2(q.x + 48, q.y + 4), ImGui::GetColorU32(ImGuiCol_Text), l);
		snprintf(l, sizeof l, "%zu %s", sq.frames.size(), Tr("frames", "\xe3\x83\x95\xe3\x83\xac\xe3\x83\xbc\xe3\x83\xa0"));
		dl->AddText(ImVec2(q.x + 48, q.y + 4 + ImGui::GetTextLineHeight() + 2), ImGui::GetColorU32(ImGuiCol_TextDisabled), l);
		ImGui::PopID();
	}
	ImGui::EndChild();
	ImGui::End();
}

} // namespace han2ui
