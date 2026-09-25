// MainFrame startup actions (--open / --capture); see startup_args.h.
#include "startup_args.h"
#include "main_frame.h"
#include "character_instance.h"
#include "game_link_panel.h"
#include "tag_panel.h"

#include <glad/glad.h>
#include <windows.h>
#include <fstream>
#include <vector>
#include <cstdint>
#include <cctype>
#include <cstring>
#include <cstdio>
#include "png_writer.h"
#include "project_manager.h"
#include <imgui.h>
#include "workspace_viewports.h"

StartupArgs gStartup;

static std::string WideArg(const wchar_t* w, unsigned codePage)
{
	if (!w) return std::string();
	const int n = WideCharToMultiByte(codePage, 0, w, -1, nullptr, 0, nullptr, nullptr);
	std::string v(n > 0 ? n : 0, '\0');
	if (n > 0) WideCharToMultiByte(codePage, 0, w, -1, v.data(), n, nullptr, nullptr);
	if (!v.empty() && v.back() == 0) v.pop_back();
	return v;
}

bool ParseWave2StartupArg(const char* arg, const wchar_t* next, int& i)
{
	auto value = [&](unsigned cp) { ++i; return WideArg(next, cp); };
	if (!strcmp(arg, "--detach")) { gStartup.detach = true; return true; }
	if (!strcmp(arg, "--quit")) { gStartup.quit = true; return true; }
	if (!strcmp(arg, "--export-boxes")) { gStartup.exportBoxes = true; return true; }
	if (!strcmp(arg, "--export-nospawns")) { gStartup.exportSpawns = false; return true; }
	if (!next) return false;
	if (!strcmp(arg, "--tick")) { gStartup.tick = atoi(value(CP_ACP).c_str()); return true; }
	if (!strcmp(arg, "--onion")) {
		const std::string v = value(CP_ACP);
		int b = 2, a = 2, sp = 3, k = 0;
		sscanf(v.c_str(), "%d,%d,%d,%d", &b, &a, &sp, &k);
		gStartup.onionBefore = b; gStartup.onionAfter = a; gStartup.onionSpacing = sp; gStartup.onionKeyframes = k != 0;
		return true;
	}
	if (!strcmp(arg, "--capture-view")) { gStartup.captureView = value(CP_UTF8); return true; }
	if (!strcmp(arg, "--capture-windows")) { gStartup.captureWindows = value(CP_UTF8); return true; }
	if (!strcmp(arg, "--export-png")) { gStartup.exportDir = value(CP_UTF8); return true; }
	if (!strcmp(arg, "--export-range")) { gStartup.exportRange = value(CP_ACP); return true; }
	if (!strcmp(arg, "--export-scale")) { gStartup.exportScale = atoi(value(CP_ACP).c_str()); return true; }
	if (!strcmp(arg, "--export-bg")) { gStartup.exportTransparent = value(CP_ACP) != "editor"; return true; }
	if (!strcmp(arg, "--export-crop")) { gStartup.exportFitEach = value(CP_ACP) == "each"; return true; }
	if (!strcmp(arg, "--save-project")) { gStartup.saveProject = value(CP_ACP); return true; }
	if (!strcmp(arg, "--stats")) { gStartup.stats = value(CP_UTF8); return true; }
	if (!strcmp(arg, "--post-key")) { gStartup.postKey = (int)strtol(value(CP_ACP).c_str(), nullptr, 0); return true; }
	return false;
}

namespace {

uint32_t Crc(const uint8_t *d, size_t n, uint32_t c = 0xFFFFFFFFu)
{
	static uint32_t t[256]; static bool init = false;
	if (!init) { for (uint32_t i = 0; i < 256; i++) { uint32_t v = i; for (int k = 0; k < 8; k++) v = (v & 1) ? 0xEDB88320u ^ (v >> 1) : v >> 1; t[i] = v; } init = true; }
	for (size_t i = 0; i < n; i++) c = t[(c ^ d[i]) & 0xFF] ^ (c >> 8);
	return c;
}

// Uncompressed (stored-deflate) RGB PNG.
void WritePng(const std::string &path, const uint8_t *rgb, int w, int h)
{
	std::vector<uint8_t> raw;
	for (int y = 0; y < h; y++) { raw.push_back(0); raw.insert(raw.end(), rgb + (size_t)y * w * 3, rgb + (size_t)(y + 1) * w * 3); }
	std::vector<uint8_t> z{0x78, 0x01};
	for (size_t pos = 0; pos < raw.size();) {
		size_t n = std::min<size_t>(65535, raw.size() - pos);
		z.push_back(pos + n >= raw.size());
		z.push_back(n & 0xFF); z.push_back(n >> 8); z.push_back(~n & 0xFF); z.push_back((~n >> 8) & 0xFF);
		z.insert(z.end(), raw.begin() + pos, raw.begin() + pos + n);
		pos += n;
	}
	uint32_t a = 1, b = 0;
	for (uint8_t v : raw) { a = (a + v) % 65521; b = (b + a) % 65521; }
	uint32_t ad = (b << 16) | a;
	for (int s = 24; s >= 0; s -= 8) z.push_back((ad >> s) & 0xFF);

	std::ofstream f(path, std::ios::binary);
	auto be = [&](uint32_t v) { uint8_t x[4] = {uint8_t(v >> 24), uint8_t(v >> 16), uint8_t(v >> 8), uint8_t(v)}; f.write((char *)x, 4); };
	auto chunk = [&](const char *type, const std::vector<uint8_t> &d) {
		be((uint32_t)d.size());
		std::vector<uint8_t> td(type, type + 4); td.insert(td.end(), d.begin(), d.end());
		f.write((const char *)td.data(), td.size());
		be(Crc(td.data(), td.size()) ^ 0xFFFFFFFFu);
	};
	f.write("\x89PNG\r\n\x1a\n", 8);
	std::vector<uint8_t> ihdr = {uint8_t(w >> 24), uint8_t(w >> 16), uint8_t(w >> 8), uint8_t(w),
	                             uint8_t(h >> 24), uint8_t(h >> 16), uint8_t(h >> 8), uint8_t(h), 8, 2, 0, 0, 0};
	chunk("IHDR", ihdr); chunk("IDAT", z); chunk("IEND", {});
}

} // namespace

void MainFrame::ProcessStartupArgs()
{
	int n = ++gStartup.frameCounter;
	// [tag-panel] after --open (frame 2), so the pickers see the loaded character; works without --open too
	if (n == 3 && (gStartup.tool == "tag" || !gStartup.tagIni.empty() || !gStartup.tagTab.empty() || gStartup.tagLink))
		tagpanel::OpenStartup(gStartup.tagIni, gStartup.tagChar, gStartup.tagTab);
	if (n == 3 && gStartup.tagLink) gamelink::SharedClient().Connect();
	if (n == 150 && gStartup.tagApply) tagpanel::StartupApply();
	if (n == 200 && gStartup.tagLink && !gStartup.tagTab.empty())   // re-select the tab once the link has state
		tagpanel::OpenStartup("", "", gStartup.tagTab);
	if (n == 3 && gStartup.tool == "gamelink") gamelink::showPanel = true;
	if (n == 2 && !gStartup.open.empty()) {
		std::string path = gStartup.open, ext;
		size_t dot = path.find_last_of('.');
		if (dot != std::string::npos) ext = path.substr(dot);
		for (auto &c : ext) c = (char)tolower((unsigned char)c);
		if (ext == ".hproj") {
			loadProjectFromPath(path, false);
		} else if (ext == ".dat") {
			loadStageFile(path);   // [stage-link] a bgmake stage (with --game-link: the Stage section's files)
		} else {
			auto character = std::make_unique<CharacterInstance>();
			bool ok = ext == ".txt" ? character->loadFromTxt(path) : character->loadHA6(path, false);
			if (ok) {
				characters.push_back(std::move(character));
				createViewForCharacter(characters.back().get());
			} else {
				requestErrorPopup("Load Error", "Could not open " + path);
			}
		}
		if (auto *v = getActiveView()) {
			if (gStartup.pattern >= 0) v->getState().pattern = gStartup.pattern;
			if (gStartup.frame >= 0) v->getState().frame = gStartup.frame;
		}
		if (gStartup.noPups)
			render.followPups = false;
		if (gStartup.zoom > 0.f)
			SetZoom(gStartup.zoom);
		if (!gStartup.game.empty()) {
			const std::string &g = gStartup.game;
			g_ha6GameOverride = g == "mbaacc" ? Ha6Game::MBAACC : g == "uni" ? Ha6Game::UNI
				: g == "mbtl" ? Ha6Game::MBTL : Ha6Game::Auto;
		}
		const std::string& t = gStartup.tool;
		if (t == "hud") m_showHud = true;
		else if (t == "bgm") m_showBgm = true;
		else if (t == "compare") m_showCompare = true;
		else if (t == "patterns") m_patMgr.open = true;
		else if (t == "notes") m_showNotes = true;
		else if (t == "vars") m_varRefs.open = true;
		else if (t == "keys") m_showKeyBindings = true;

		if (auto *c = getActiveCharacter(); c && gStartup.compare >= 0) {
			m_compare.enabled = true;
			m_compare.character = c;
			m_compare.pattern = gStartup.compare;
			m_compare.offsetX = 60;
		}
		if (auto *c = getActiveCharacter(); c && gStartup.palette >= 0) {
			c->palette = gStartup.palette;
			c->cg.changePaletteNumber(gStartup.palette);
		}
		if (auto *v = getActiveView(); v && v->getCharacter()) {
			auto &st = v->getState();
			if (gStartup.tick >= 0) {
				// The root frame follows the simulated flow at that tick.
				CharacterInstance *c = v->getCharacter();
				FrameData *fx = c->effectCharacter ? &c->effectCharacter->frameData : nullptr;
				auto &sim = st.BindPreviewSim(&c->frameData, fx);
				sim.ensureSimulatedTo(gStartup.tick);
				st.currentTick = std::min(gStartup.tick, sim.horizon());
				const auto &track = sim.rootFrameTrack();
				if (st.currentTick < (int)track.size()) st.frame = track[st.currentTick];
			}
			if (gStartup.onionBefore >= 0) {
				auto &o = v->onion();
				o.enabled = true;
				o.before = gStartup.onionBefore;
				o.after = gStartup.onionAfter;
				o.spacing = std::max(1, gStartup.onionSpacing);
				o.keyframesOnly = gStartup.onionKeyframes;
			}
			if (gStartup.detach) {
				const ImVec2 p = ImGui::GetMainViewport()->Pos;
				DetachViewToNewHost(v->getId(), ImVec2(p.x + 200.f, p.y + 150.f));
				m_startupViewId = v->getId();
			}
		}
		if (!m_startupViewId) {
			if (auto *v = getActiveView()) m_startupViewId = v->getId();
			else if (!views.empty()) m_startupViewId = views.front()->getId();
		}
	}
	if (n == 8 && !gStartup.exportDir.empty()) {
		if (CharacterView *v = findViewById(m_startupViewId)) {
			ExportSettings s = m_export;
			s.folder = gStartup.exportDir;
			PrepareExportSettings(v);
			s.baseName = m_export.baseName;
			s.scale = gStartup.exportScale;
			s.background = gStartup.exportTransparent ? ExportSettings::transparent : ExportSettings::editorColor;
			s.boxes = gStartup.exportBoxes;
			s.spawns = gStartup.exportSpawns;
			s.crop = gStartup.exportFitEach ? ExportSettings::fitEach : ExportSettings::fitAll;
			int a = 0, b = 0;
			if (gStartup.exportRange == "current") s.range = ExportSettings::currentFrame;
			else if (sscanf(gStartup.exportRange.c_str(), "%d:%d", &a, &b) == 2) { s.range = ExportSettings::tickRange; s.fromTick = a; s.toTick = b; }
			else s.range = ExportSettings::wholePattern;
			ExportResult r;
			RunPngExport(v, s, r);
			// Result line for scripted checks (a file next to the export).
			const std::string report = gStartup.exportDir + "\\export_result.txt";
			if (FILE *f = _wfopen(Utf8ToWide(report).c_str(), L"wb")) {
				fprintf(f, "%s\n%s\n", r.ok ? "OK" : "FAIL", r.message.c_str());
				fclose(f);
			}
		}
	}
	if (gStartup.postKey && n == 12) {
		HWND target = nullptr;
		for (ImGuiViewport *vp : ImGui::GetPlatformIO().Viewports)
			if (vp != ImGui::GetMainViewport() && vp->PlatformHandle) { target = (HWND)vp->PlatformHandle; break; }
		if (target) PostMessage(target, WM_KEYDOWN, (WPARAM)gStartup.postKey, 1);
	}
	if (!gStartup.stats.empty() && n >= 10 && n < 20) {
		gStartup.sceneMs += m_lastSceneMs;
		gStartup.onionSimMs += m_onionStats.simMs;
		gStartup.onionTotalMs += m_onionStats.totalMs;
		gStartup.onionSamples = m_onionStats.samples;
		++gStartup.statFrames;
	}
	if (!gStartup.stats.empty() && n == 20 && gStartup.statFrames) {
		if (FILE *f = _wfopen(Utf8ToWide(gStartup.stats).c_str(), L"wb")) {
			const double k = 1.0 / gStartup.statFrames;
			fprintf(f, "frames %d\nscene_ms %.4f\nonion_samples %d\nonion_sim_ms %.4f\nonion_total_ms %.4f\n",
				gStartup.statFrames, gStartup.sceneMs * k, gStartup.onionSamples,
				gStartup.onionSimMs * k, gStartup.onionTotalMs * k);
			if (CharacterView *v = findViewById(m_startupViewId)) {
				const auto owner = m_session.owner(v->getId());
				fprintf(f, "view_host %llu\nview_onion_enabled %d\nshortcut_view %llu\nfocused_host %llu\n",
					(unsigned long long)owner.value_or(0), v->onion().enabled ? 1 : 0,
					(unsigned long long)shortcuts.focusedViewId(), (unsigned long long)m_focusedHostId);
			}
			fclose(f);
		}
	}
	if (!gStartup.captureView.empty() && n == 20) {
		if (CharacterView *v = findViewById(m_startupViewId)) {
			std::vector<uint8_t> px;
			std::string err;
			if (v->renderTarget().readRgba(px)) {
				for (size_t k = 3; k < px.size(); k += 4) px[k] = 255;
				WritePngRgba(gStartup.captureView, px.data(), v->renderTarget().width(), v->renderTarget().height(), err);
			}
		}
	}
	if (!gStartup.captureWindows.empty() && n == 20) {
		// Every detached (platform) window's back buffer, read just before
		// it is presented this frame (after this function returns).
		WorkspaceViewports::RequestCapture([](int index, int w, int h, const unsigned char *rgba, void *) {
			std::vector<uint8_t> px(rgba, rgba + (size_t)w * h * 4);
			for (size_t k = 3; k < px.size(); k += 4) px[k] = 255;
			std::string err;
			WritePngRgba(gStartup.captureWindows + "_" + std::to_string(index) + ".png", px.data(), w, h, err);
		}, nullptr);
	}
	if (!gStartup.saveProject.empty() && n == 21) {
		ProjectManager::SetCurrentProjectPath(gStartup.saveProject);
		saveProject();
	}
	const bool anyWave2 = !gStartup.captureView.empty() || !gStartup.saveProject.empty() ||
		!gStartup.captureWindows.empty() || !gStartup.stats.empty() ||
		!gStartup.exportDir.empty() || gStartup.quit;
	if (gStartup.capture.empty() && anyWave2 && n == 22)
		PostQuitMessage(0);
	if (n == 2 && gStartup.gameLinkSlot >= 1 && gStartup.gameLinkSlot <= 4)
		gamelink::StartFollowing(gStartup.gameLinkSlot - 1);
	if (!gStartup.capture.empty() && n == ((gStartup.gameLinkSlot || gStartup.tagLink) ? 240 : 20)) {
		RECT r; GetClientRect(WindowFromDC(context->dc), &r);
		int w = r.right - r.left, h = r.bottom - r.top;
		std::vector<uint8_t> rgba((size_t)w * h * 4), rgb((size_t)w * h * 3);
		glReadBuffer(GL_BACK);
		glPixelStorei(GL_PACK_ALIGNMENT, 1);
		glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
		for (int y = 0; y < h; y++)
			for (int x = 0; x < w; x++)
				for (int k = 0; k < 3; k++)
					rgb[((size_t)(h - 1 - y) * w + x) * 3 + k] = rgba[((size_t)y * w + x) * 4 + k];
		WritePng(gStartup.capture, rgb.data(), w, h);
		if (gStartup.saveProject.empty()) PostQuitMessage(0);
	}
	if (!gStartup.capture.empty() && !gStartup.saveProject.empty() && n == 22)
		PostQuitMessage(0);
}
