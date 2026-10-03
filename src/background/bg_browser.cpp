// Stage browser window; see bg_browser.h.
#include "bg_browser.h"
#include "bg_gametex.h"
#include "../bgm_player.h"
#include "../png_writer.h"
#include <imgui.h>
#include <glad/glad.h>
#include <windows.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <map>
#include <memory>
#include "../i18n.h"

namespace bg {

namespace {

struct Thumb { GLuint id = 0; int w = 0, h = 0; };
std::map<std::string, Thumb> g_thumbs;
int g_selected = -1;
std::unique_ptr<BgmPlayer> g_player;
std::string g_status;

std::string SjisToUtf8(const std::string& s) {
	if (s.empty()) return s;
	int wn = MultiByteToWideChar(932, 0, s.data(), (int)s.size(), nullptr, 0);
	std::wstring w(wn > 0 ? wn : 0, L'\0');
	if (wn > 0) MultiByteToWideChar(932, 0, s.data(), (int)s.size(), &w[0], wn);
	int un = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
	std::string u(un > 0 ? un : 0, '\0');
	if (un > 0) WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), &u[0], un, nullptr, nullptr);
	return u;
}

// DDS (DXT5 or 32-bit uncompressed) -> GL texture, cached by path.
const Thumb* LoadDds(const std::string& path) {
	if (path.empty()) return nullptr;
	auto it = g_thumbs.find(path);
	if (it != g_thumbs.end()) return it->second.id ? &it->second : nullptr;
	Thumb& t = g_thumbs[path];
	std::ifstream f(path, std::ios::binary);
	std::vector<uint8_t> d((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
	if (d.size() < 128 || std::memcmp(d.data(), "DDS ", 4) != 0) return nullptr;
	auto rd = [&](size_t o) { uint32_t v; std::memcpy(&v, d.data() + o, 4); return v; };
	int h = (int)rd(12), w = (int)rd(16);
	uint32_t pfFlags = rd(80), fourcc = rd(84), bits = rd(88);
	std::vector<uint8_t> rgba;
	if ((pfFlags & 4) && (fourcc == 0x35545844 /*DXT5*/ || fourcc == 0x33545844 /*DXT3*/ || fourcc == 0x31545844 /*DXT1*/)) {
		const int bs = fourcc == 0x31545844 ? 8 : 16;
		const int bw = (w + 3) / 4, bh = (h + 3) / 4;
		if (d.size() < 128 + (size_t)bw * bh * bs) return nullptr;
		int tw = bw * 4, th = bh * 4;
		std::vector<uint8_t> full;
		if (fourcc == 0x35545844) DecodeDxt5(d.data() + 128, tw, th, full);
		else {
			// DXT3: explicit 4-bit alpha + DXT1 colour; DXT1: colour only.
			full.assign((size_t)tw * th * 4, 0);
			for (int by = 0; by < bh; ++by)
				for (int bx = 0; bx < bw; ++bx) {
					const uint8_t* b = d.data() + 128 + ((size_t)by * bw + bx) * bs;
					const uint8_t* c = bs == 16 ? b + 8 : b;
					uint16_t c0 = c[0] | (c[1] << 8), c1 = c[2] | (c[3] << 8);
					int pal[4][4];
					auto ex = [](uint16_t v, int* o) { o[0] = ((v >> 11) & 31) * 255 / 31; o[1] = ((v >> 5) & 63) * 255 / 63; o[2] = (v & 31) * 255 / 31; o[3] = 255; };
					ex(c0, pal[0]); ex(c1, pal[1]);
					for (int k = 0; k < 3; ++k) {
						if (bs == 16 || c0 > c1) { pal[2][k] = (2 * pal[0][k] + pal[1][k]) / 3; pal[3][k] = (pal[0][k] + 2 * pal[1][k]) / 3; }
						else { pal[2][k] = (pal[0][k] + pal[1][k]) / 2; pal[3][k] = 0; }
					}
					pal[2][3] = 255; pal[3][3] = (bs == 8 && c0 <= c1) ? 0 : 255;
					uint32_t cb = c[4] | (c[5] << 8) | (c[6] << 16) | ((uint32_t)c[7] << 24);
					for (int i = 0; i < 16; ++i) {
						uint8_t* o = &full[(((size_t)by * 4 + i / 4) * tw + bx * 4 + i % 4) * 4];
						const int* p = pal[(cb >> (2 * i)) & 3];
						o[0] = (uint8_t)p[0]; o[1] = (uint8_t)p[1]; o[2] = (uint8_t)p[2];
						o[3] = bs == 16 ? (uint8_t)(((b[i / 2] >> (4 * (i & 1))) & 15) * 17) : (uint8_t)p[3];
					}
				}
		}
		rgba.resize((size_t)w * h * 4);
		for (int y = 0; y < h; ++y) std::memcpy(&rgba[(size_t)y * w * 4], &full[(size_t)y * tw * 4], (size_t)w * 4);
	} else if ((pfFlags & 0x40) && bits == 32) {
		if (d.size() < 128 + (size_t)w * h * 4) return nullptr;
		rgba.assign(d.begin() + 128, d.begin() + 128 + (size_t)w * h * 4);
		for (size_t i = 0; i < rgba.size(); i += 4) std::swap(rgba[i], rgba[i + 2]);   // BGRA -> RGBA
	} else return nullptr;
	glGenTextures(1, &t.id);
	glBindTexture(GL_TEXTURE_2D, t.id);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	t.w = w; t.h = h;
	return &t;
}

std::string BaseLower(const std::string& p) {
	size_t s = p.find_last_of("/\\");
	std::string b = s == std::string::npos ? p : p.substr(s + 1);
	for (char& c : b) c = (char)std::tolower((unsigned char)c);
	return b;
}

bool EditTextKey(const char* label, std::string cur, std::string& out) {
	char buf[128];
	std::snprintf(buf, sizeof(buf), "%s", cur.c_str());
	ImGui::InputText(LBL(label), buf, sizeof(buf));
	if (ImGui::IsItemDeactivatedAfterEdit() && cur != buf) { out = buf; return true; }
	return false;
}

} // namespace

void ClearStageBrowserCache() {
	for (auto& kv : g_thumbs) if (kv.second.id) glDeleteTextures(1, &kv.second.id);
	g_thumbs.clear();
}

void DrawStageBrowser(StageProject& pr, const std::string& currentDat, const BrowserHooks& hooks) {
	// --- project line ---
	static char dirBuf[512] = "";
	static int gameSel = 0;
	if (pr.IsOpen() && dirBuf[0] == 0) std::snprintf(dirBuf, sizeof(dirBuf), "%s", pr.BgDir().c_str());
	ImGui::PushItemWidth(-260);
	ImGui::InputText("##dir", dirBuf, sizeof(dirBuf));
	ImGui::PopItemWidth();
	ImGui::SameLine();
	ImGui::PushItemWidth(80);
	const char* games[] = {"MBAACC", "MBAC"};
	if (pr.IsOpen()) gameSel = pr.GetGame() == Game::MBAC ? 1 : 0;
	i18n::Combo("##game", &gameSel, games, 2);
	ImGui::PopItemWidth();
	ImGui::SameLine();
	if (ImGui::Button(LBL("Browse..."))) {
		std::string d = BrowseForFolderUtf8(dirBuf);
		if (!d.empty()) std::snprintf(dirBuf, sizeof(dirBuf), "%s", d.c_str());
	}
	ImGui::SameLine();
	if (ImGui::Button(LBL("Open"))) {
		ClearStageBrowserCache();
		if (!pr.Open(dirBuf, gameSel ? Game::MBAC : Game::MBAACC)) g_status = "No stage list found there (MBAACC needs bg\\BgList.ini).";
		else g_status.clear();
	}
	if (!g_status.empty()) ImGui::TextColored(ImVec4(1, 0.5f, 0.4f, 1), "%s", g_status.c_str());
	if (!pr.IsOpen()) {
		ImGui::TextDisabled(TXT("Open a game's bg folder (or load a stage) to list its stages."));
		return;
	}

	// --- save / history ---
	const bool mbaacc = pr.GetGame() == Game::MBAACC;
	if (ImGui::Button(LBL("Save BgList.ini")) && pr.BgList().IsDirty()) pr.BgList().Save(pr.BgList().Path());
	ImGui::SameLine();
	if (ImGui::Button(LBL("Save bgm.txt")) && pr.Bgm().IsDirty()) pr.Bgm().Save(pr.Bgm().Path());
	ImGui::SameLine();
	ImGui::BeginDisabled(!pr.CanUndo());
	if (ImGui::Button(LBL("Undo"))) pr.Undo();
	ImGui::EndDisabled();
	ImGui::SameLine();
	ImGui::BeginDisabled(!pr.CanRedo());
	if (ImGui::Button(LBL("Redo"))) pr.Redo();
	ImGui::EndDisabled();
	ImGui::SameLine();
	ImGui::TextDisabled("%s%s", pr.BgList().IsDirty() ? TXT("BgList modified  ") : "", pr.Bgm().IsDirty() ? TXT("bgm modified") : "");

	static bool hideUnlisted = false;
	ImGui::Checkbox(LBL("Hide unlisted files"), &hideUnlisted);
	ImGui::SameLine();
	ImGui::TextDisabled(TXT("PageUp/PageDown in the stage tab: previous/next stage"));

	const auto& ents = pr.Entries();
	const StageEntry* cur = currentDat.empty() ? nullptr : pr.FindByDat(currentDat);

	// --- list ---
	const float listH = ImGui::GetContentRegionAvail().y * 0.55f;
	if (ImGui::BeginTable("stages", 6, ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV |
	                                       ImGuiTableFlags_Resizable, ImVec2(0, listH))) {
		ImGui::TableSetupScrollFreeze(0, 1);
		ImGui::TableSetupColumn("Id", ImGuiTableColumnFlags_WidthFixed, 34);
		ImGui::TableSetupColumn(LBL("Preview"), ImGuiTableColumnFlags_WidthFixed, 132);
		ImGui::TableSetupColumn(LBL("Name"));
		ImGui::TableSetupColumn(LBL("File"), ImGuiTableColumnFlags_WidthFixed, 60);
		ImGui::TableSetupColumn(LBL("BGM"), ImGuiTableColumnFlags_WidthFixed, 70);
		ImGui::TableSetupColumn(LBL("Flags"), ImGuiTableColumnFlags_WidthFixed, 90);
		ImGui::TableHeadersRow();
		for (int i = 0; i < (int)ents.size(); ++i) {
			const StageEntry& e = ents[i];
			if (hideUnlisted && !e.listed) continue;
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			char lab[32];
			std::snprintf(lab, sizeof(lab), "%02d##row%d", e.id, i);
			const bool isCur = cur && cur->id == e.id;
			if (isCur) ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(60, 90, 140, 160));
			if (ImGui::Selectable(lab, g_selected == e.id, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick,
			                      ImVec2(0, 44))) {
				g_selected = e.id;
				if (ImGui::IsMouseDoubleClicked(0) && !e.datPath.empty() && hooks.open) hooks.open(e.datPath);
			}
			ImGui::TableNextColumn();
			if (const Thumb* t = LoadDds(e.previewPath)) ImGui::Image((ImTextureID)(intptr_t)t->id, ImVec2(117, 44));
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(e.name.empty() ? "-" : e.name.c_str());
			if (!e.nameJp.empty() && e.nameJp != e.name) ImGui::TextDisabled("%s", e.nameJp.c_str());
			ImGui::TableNextColumn();
			if (e.datPath.empty()) ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "%s", e.dataFile.empty() ? "-" : e.dataFile.c_str());
			else ImGui::TextUnformatted(e.dataFile.c_str());
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(e.hasBgm ? e.bgmFile.c_str() : "-");
			ImGui::TableNextColumn();
			std::string fl;
			if (!e.listed) fl += mbaacc ? "unlisted " : "off ";
			if (e.giant) fl += "giant ";
			if (e.excludedOther) fl += "!VS ";
			if (e.excludedTraining) fl += "!Tr ";
			ImGui::TextUnformatted(fl.c_str());
		}
		ImGui::EndTable();
	}

	// --- details / editor for the selected stage ---
	const StageEntry* sel = pr.Find(g_selected);
	if (!sel && cur) { g_selected = cur->id; sel = cur; }
	if (!sel) return;
	ImGui::Separator();
	if (const Thumb* t = LoadDds(sel->previewPath)) { ImGui::Image((ImTextureID)(intptr_t)t->id, ImVec2((float)t->w, (float)t->h)); ImGui::SameLine(); }
	ImGui::BeginGroup();
	ImGui::Text(TXT("Stage %02d  %s"), sel->id, sel->datPath.empty() ? "(no .dat: the game drops this entry)" : sel->datPath.c_str());
	if (!sel->bgmComment.empty()) ImGui::TextUnformatted(SjisToUtf8(sel->bgmComment).c_str());
	if (const Thumb* t = LoadDds(sel->nameEnPath)) ImGui::Image((ImTextureID)(intptr_t)t->id, ImVec2((float)t->w, (float)t->h));
	ImGui::BeginDisabled(sel->datPath.empty() || !hooks.open);
	if (ImGui::Button(LBL("Open stage"))) hooks.open(sel->datPath);
	ImGui::EndDisabled();
	ImGui::SameLine();
	ImGui::BeginDisabled(!hooks.showInGame || sel->datPath.empty());
	if (ImGui::Button(LBL("Show in game"))) {
		if (!hooks.showInGame(sel->id, sel->datPath)) g_status = "The game did not accept the stage switch.";
	}
	ImGui::EndDisabled();
	if (!hooks.showInGame && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		ImGui::SetTooltip(TXT("Needs the Game Link stage switch (PovertyCaster mbaacc/stage-link)."));
	if (hooks.pickForSetup) {
		ImGui::SameLine();
		ImGui::BeginDisabled(sel->datPath.empty());
		if (ImGui::Button(LBL("Use for the Authoring setup"))) hooks.pickForSetup(sel->id);
		ImGui::EndDisabled();
	}
	ImGui::EndGroup();

	if (mbaacc) {
		char sec[16];
		std::snprintf(sec, sizeof(sec), "Bg_%03d", sel->id);
		if (!sel->listed) {
			ImGui::TextDisabled(TXT("No [%s] section: the game cannot select this stage."), sec);
			if (ImGui::Button(LBL("Add to BgList.ini"))) pr.AddStage(sel->id, sel->dataFile.empty() ? "bg00" : sel->dataFile);
		} else {
			ImGui::TextDisabled(TXT("BgList.ini [%s]"), sec);
			ImGui::PushItemWidth(160);
			std::string v;
			if (EditTextKey("DataFile", sel->dataFile, v)) pr.SetListValue(sel->id, "DataFile", v);
			bool b = sel->selectable != 0;
			if (ImGui::Checkbox(LBL("IsSelectAble"), &b)) pr.SetListValue(sel->id, "IsSelectAble", b ? "1" : "0");
			if (ImGui::IsItemHovered()) ImGui::SetTooltip(TXT("Parsed by the game but never read (MBAA 0x4b4a80)."));
			ImGui::SameLine();
			b = sel->giant != 0;
			if (ImGui::Checkbox(LBL("IsGiantStage"), &b)) pr.SetListValue(sel->id, "IsGiantStage", b ? "1" : "");
			if (ImGui::IsItemHovered()) ImGui::SetTooltip(TXT("Only effect: the system-effect 2000 quad draws at priority 496 instead of 306\n(in front of the fighters)."));
			ImGui::SameLine();
			b = sel->infoFile != 0;
			if (ImGui::Checkbox(LBL("InfoFile"), &b)) pr.SetListValue(sel->id, "InfoFile", b ? "1" : "");
			if (ImGui::IsItemHovered()) ImGui::SetTooltip(TXT("Parsed but unused: the game opens <DataFile>Info.txt for every stage."));
			float cv = sel->colorVal;
			ImGui::DragFloat(LBL("StageColorVal"), &cv, 0.01f, -1.0f, 2.0f, "%.2f");
			if (ImGui::IsItemDeactivatedAfterEdit()) {
				char t[32]; std::snprintf(t, sizeof(t), "%g", cv);
				pr.SetListValue(sel->id, "StageColorVal", cv == 0.0f ? std::string() : std::string(t));
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip(TXT("fColorHosei of the BgPointBlur post effect (HEAT / BLOOD HEAT):\nthe blurred scene is brightened by 1 + StageColorVal * intensity."));
			static int moveTo = 0;
			ImGui::InputInt("##moveto", &moveTo);
			ImGui::SameLine();
			if (ImGui::Button(LBL("Move to id"))) { pr.MoveStage(sel->id, moveTo); g_selected = moveTo; }
			if (ImGui::IsItemHovered()) ImGui::SetTooltip(TXT("Renumbers [Bg_NNN]. Stage select walks ids in order,\nand stage NN plays [BGM_0NN]."));
			ImGui::SameLine();
			if (ImGui::Button(LBL("Remove entry"))) pr.RemoveStage(sel->id);
			ImGui::PopItemWidth();
		}
		ImGui::TextDisabled(TXT("Hardcoded in MBAA.exe (not data):"));
		ImGui::TextDisabled(TXT("  %s in Training stage select; %s in other modes and the random pool."),
		                    sel->excludedTraining ? "hidden" : "shown", sel->excludedOther ? "hidden" : "shown");
		if (sel->id == 18) ImGui::TextDisabled(TXT("  boss battle always uses stage 18."));
	} else {
		ImGui::TextDisabled(TXT("MBAC: the stage list is g_StageTable in mbacPC.exe (0x491050), not a data file."));
	}

	// --- BGM ---
	ImGui::Separator();
	char bsec[16];
	std::snprintf(bsec, sizeof(bsec), "BGM_%03d", sel->id);
	ImGui::TextDisabled(TXT("Music: bgm.txt [%s] (stage %d plays BGM %d)"), bsec, sel->id, sel->id);
	if (!pr.Bgm().IsLoaded()) ImGui::TextDisabled(TXT("bgm.txt not found"));
	else {
		ImGui::PushItemWidth(160);
		std::string v;
		if (mbaacc && EditTextKey("File##bgm", sel->bgmFile, v)) pr.SetBgmValue(sel->id, "File", v);
		bool loop = sel->bgmLoop != 0;
		if (ImGui::Checkbox(LBL("IsLoop"), &loop)) pr.SetBgmValue(sel->id, "IsLoop", loop ? "1" : "0");
		ImGui::SameLine();
		if (EditTextKey("LoopPos (s)", sel->bgmLoopPos, v)) pr.SetBgmValue(sel->id, "LoopPos", v);
		ImGui::PopItemWidth();
		std::string ogg;
		std::string bgmDir = pr.Bgm().Path().substr(0, pr.Bgm().Path().find_last_of("/\\") + 1);
		if (!sel->bgmFile.empty()) ogg = bgmDir + sel->bgmFile + ".ogg";
		if (!g_player) g_player = std::make_unique<BgmPlayer>();
		const bool playingThis = g_player->playing() && g_player->path() == ogg;
		ImGui::BeginDisabled(ogg.empty());
		if (ImGui::Button(playingThis ? LBL("Stop") : LBL("Play"))) {
			if (playingThis) g_player->stop();
			else {
				std::string err;
				g_player->stop();
				if (g_player->load(ogg, sel->bgmLoop != 0, std::atof(sel->bgmLoopPos.c_str()), &err)) g_player->play();
				else g_status = "BGM: " + err;
			}
		}
		ImGui::EndDisabled();
		if (playingThis) { ImGui::SameLine(); ImGui::Text("%.1f / %.1f s", g_player->position(), g_player->length()); }
	}
	(void)BaseLower;
}

} // namespace bg
