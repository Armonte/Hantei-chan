// MainFrame startup actions (--open / --capture); see startup_args.h.
#include "startup_args.h"
#include "main.h"
#include "fpanim_play.h"
#include "archive_browser.h"
#include "game_table.h"
#include "stage3d/s3d_view.h"

#include <algorithm>
#include <chrono>
#include "authoring/authoring_window.h"
#include "main_frame.h"
#include "character_instance.h"
#include "framedata_han2.h"
#include "han2_pat.h"
#include "han2_character.h"
#include "character_view.h"
#include "han2/pac_archive.h"
#include "game_link_panel.h"
#include "tag_panel.h"
#include "pcmod/pcmod_export_ui.h"
#include "modproject/hmod_ui.h"

#include <glad/glad.h>
#include <windows.h>
#include <fstream>
#include <functional>
#include <filesystem>
#include <vector>
#include <cstdint>
#include <cctype>
#include <cstring>
#include <cstdio>
#include "png_writer.h"
#include "project_manager.h"
#include <imgui.h>
#include "workspace_viewports.h"
#include "paste_notice.h"
#include "copy_data.h"
#include "app_log.h"
#include "render_target.h"
#include <map>

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

// --size WxH | --center X,Y | --bg R,G,B (0-255) | --layer L[,L..]; shared by the command line and --batch lines.
static bool ParseSizeArg(const std::string &v)
{
	int w = 0, h = 0;
	if (sscanf(v.c_str(), "%dx%d", &w, &h) != 2 || w < 1 || h < 1 || w > 8192 || h > 8192) return false;
	gStartup.sizeW = w; gStartup.sizeH = h; return true;
}
static bool ParseCenterArg(const std::string &v)
{
	float x = 0, y = 0;
	if (sscanf(v.c_str(), "%f,%f", &x, &y) != 2) return false;
	gStartup.hasCenter = true; gStartup.centerX = x; gStartup.centerY = y; return true;
}
static bool ParseBgArg(const std::string &v)
{
	int r = 0, g = 0, b = 0;
	if (sscanf(v.c_str(), "%d,%d,%d", &r, &g, &b) != 3) return false;
	gStartup.hasBg = true; gStartup.bg[0] = r / 255.f; gStartup.bg[1] = g / 255.f; gStartup.bg[2] = b / 255.f; return true;
}
static bool ParseLayerArg(const std::string &v)
{
	if (v.empty() || v == "all") { gStartup.layerMask = ~0u; return true; }
	unsigned m = 0;
	for (const char *p = v.c_str(); *p;) {
		char *e = nullptr; const long l = strtol(p, &e, 10);
		if (e == p || l < 0 || l > 31) return false;
		m |= 1u << l; p = *e == ',' ? e + 1 : e;
		if (*e && *e != ',') return false;
	}
	gStartup.layerMask = m; return true;
}

bool ParseWave2StartupArg(const char* arg, const char* next, int& i)
{
	// argv is UTF-8 under SDL3 (the Windows backend converts the command line
	// with CP_UTF8, libc's argv is UTF-8 on Linux). No codepage conversion
	// needed; the old CP_ACP/CP_UTF8 split disappears with wWinMain.
	auto value = [&](unsigned) { ++i; return next ? std::string(next) : std::string(); };
	if (!strcmp(arg, "--detach")) { gStartup.detach = true; return true; }
	if (!strcmp(arg, "--quit")) { gStartup.quit = true; return true; }
	if (!strcmp(arg, "--export-boxes")) { gStartup.exportBoxes = true; return true; }
	if (!strcmp(arg, "--export-nospawns")) { gStartup.exportSpawns = false; return true; }
	if (!strcmp(arg, "--hide-boxes")) { gStartup.hideBoxes = true; return true; }
	if (!strcmp(arg, "--hide-spawns")) { gStartup.hideSpawns = true; return true; }
	if (!strcmp(arg, "--headless")) { gStartup.headless = true; return true; }
	if (!strcmp(arg, "--deterministic")) {
		gStartup.deterministic = true;
		abrowser::g_installScanSync = true;   // the install scan runs inline (PLAN.md 4.3)
		abrowser::SetDeterministic(true);     // preview / thumbnail jobs run inline too
		return true;
	}
	if (!next) return false;
	if (!strcmp(arg, "--size")) { const std::string v = value(CP_ACP); return ParseSizeArg(v), true; }
	if (!strcmp(arg, "--center")) { const std::string v = value(CP_ACP); return ParseCenterArg(v), true; }
	if (!strcmp(arg, "--bg")) { const std::string v = value(CP_ACP); return ParseBgArg(v), true; }
	if (!strcmp(arg, "--layer")) { const std::string v = value(CP_ACP); return ParseLayerArg(v), true; }
	if (!strcmp(arg, "--batch")) { gStartup.batch = value(CP_UTF8); return true; }
	if (!strcmp(arg, "--tick")) { gStartup.tick = atoi(value(CP_ACP).c_str()); return true; }
	if (!strcmp(arg, "--onion")) {
		const std::string v = value(CP_ACP);
		int b = 2, a = 2, sp = 3, k = 0;
		sscanf(v.c_str(), "%d,%d,%d,%d", &b, &a, &sp, &k);
		gStartup.onionBefore = b; gStartup.onionAfter = a; gStartup.onionSpacing = sp; gStartup.onionKeyframes = k != 0;
		return true;
	}
	if (!strcmp(arg, "--ui-script")) { gStartup.uiScript = value(CP_UTF8); return true; }
	if (!strcmp(arg, "--capture-view")) { gStartup.captureView = value(CP_UTF8); return true; }
	if (!strcmp(arg, "--capture-windows")) { gStartup.captureWindows = value(CP_UTF8); return true; }
	if (!strcmp(arg, "--export-png")) {   // <x.png>: one view shot (= --capture-view); anything else: the export folder
		std::string v = value(CP_UTF8);
		std::string lower = v; for (auto &c : lower) c = (char)tolower((unsigned char)c);
		if (lower.size() > 4 && lower.compare(lower.size() - 4, 4, ".png") == 0) gStartup.captureView = v; else gStartup.exportDir = v;
		return true;
	}
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

// true for a French-Bread HAN2RBO character or a PAC archive (content, not extension)
static bool han2StartupSniff(const std::string &path)
{
	FILE *f = fopen(path.c_str(), "rb");
	if (!f) return false;
	unsigned char b[0x40]{};
	size_t n = fread(b, 1, 0x40, f);
	fclose(f);
	return han2::IsHan2(b, n) || pac::LooksLikePac(b, n);
}

// French-Bread PAT v3/v4 (magic 3|4, 0x01234567); MBAACC .pat files start differently and stay on the old path
static bool han2StartupSniffPat(const std::string &path)
{
	FILE *f = fopen(path.c_str(), "rb");
	if (!f) return false;
	unsigned char b[8]{};
	size_t n = fread(b, 1, 8, f);
	fclose(f);
	return han2::IsPat(b, n);
}

// The view's own render target as drawn this frame (DrawBack), alpha forced opaque. Reads the FBO, never a back
// buffer, so it works the same with the window hidden (--headless).
static bool WriteViewTargetPng(CharacterView *v, const std::string &path)
{
	if (!v) { HC_LOG_ERROR("view shot {}: no view open", path); return false; }
	std::vector<uint8_t> px;
	if (!v->renderTarget().readRgba(px)) { HC_LOG_ERROR("view shot {}: render target read failed", path); return false; }
	for (size_t k = 3; k < px.size(); k += 4) px[k] = 255;
	std::error_code ec; std::filesystem::create_directories(std::filesystem::u8path(path).parent_path(), ec);
	std::string err;
	if (!WritePngRgba(path, px.data(), v->renderTarget().width(), v->renderTarget().height(), err)) {
		HC_LOG_ERROR("view shot {}: {}", path, err); return false;
	}
	return true;
}

static int pendingWaitMs = 0; static bool pendingWaitS3d = false;
static int pendingPlayWaitS = 0;   // set by playwait <seconds>: until Play in game is idle, then its reply goes to the log   // set by waitms / waits3dchange, armed by RunUiScript
static void LogLine(const char* text)
{
	FILE *lf = fopen((gStartup.uiScript + ".log").c_str(), "a");
	if (lf) { fprintf(lf, "%s\n", text); fclose(lf); }
}

// Scripted UI session (--ui-script): runs between render and swap, so `capture` sees exactly what this frame drew.
static void RunUiScript(MainFrame &mf, const std::function<void(const std::string &)> &openAny, const std::function<void(int, int)> &setView, const std::function<bool()> &saveActive,
                        const std::function<bool(const std::string &, const std::string &)> &editCmd,
	const std::function<bool(const std::string &, const std::string &, std::string *)> &cgmScript)
{
	static std::vector<std::string> lines; static size_t pc = 0; static int waitFrames = 0, idleWaited = 0; static bool loaded = false, waitIdle = false, done = false;
	if (done || gStartup.uiScript.empty()) return;
	if (!loaded) {
		loaded = true;
		std::ifstream f(std::filesystem::u8path(gStartup.uiScript));
		std::string l;
		while (std::getline(f, l)) { while (!l.empty() && (l.back() == '\r' || l.back() == ' ')) l.pop_back(); if (!l.empty() && l[0] != '#') lines.push_back(l); }
		if (lines.empty()) { done = true; gStartup.exitCode = 2; gAppQuit = true; return; }
	}
	if (waitFrames > 0) { waitFrames--; return; }
	{   // wall-clock waits: `waitms <ms>`, `waits3dchange <seconds>` (until the 3D stage's watched file changes, e.g. a Blender export)
		using clk = std::chrono::steady_clock;
		static clk::time_point waitUntil{}; static bool waitingMs = false, waitingS3d = false;
		if (pendingWaitMs > 0) { waitUntil = clk::now() + std::chrono::milliseconds(pendingWaitMs); waitingMs = !pendingWaitS3d; waitingS3d = pendingWaitS3d; pendingWaitMs = 0; pendingWaitS3d = false; }
		if (waitingS3d) {
			if (stage3dview::ChangedOnDiskNow()) {   // then 1 s for the writer to finish (a plain copy is not atomic)
				waitingS3d = false; waitingMs = true; waitUntil = clk::now() + std::chrono::seconds(1); LogLine("waits3dchange -> changed");
			}
			else if (clk::now() >= waitUntil) { waitingS3d = false; LogLine("waits3dchange -> FAILED timeout"); gStartup.exitCode = 1; }
			else return;
		}
		if (waitingMs) { if (clk::now() < waitUntil) return; waitingMs = false; }   // also the settle after waits3dchange
	}
	{   // playwait <seconds>: Play in game (and the save's hot reload it waits for) finished
		using clk = std::chrono::steady_clock;
		static clk::time_point playUntil{}; static bool playWaiting = false;
		if (pendingPlayWaitS > 0) { playUntil = clk::now() + std::chrono::seconds(pendingPlayWaitS); playWaiting = true; pendingPlayWaitS = 0; }
		if (playWaiting) {
			const bool timeout = clk::now() >= playUntil;
			if (fpanimplay::Busy() && !timeout) return;
			playWaiting = false;
			LogLine(((timeout ? "playwait -> FAILED timeout: " : "playwait -> ") + fpanimplay::LastReply()).c_str());
			if (timeout) gStartup.exitCode = 1;
		}
	}
	if (waitIdle) { if (abrowser::Busy() && ++idleWaited < 1800) return; waitIdle = false; idleWaited = 0; waitFrames = 6; return; }   // a few frames for the uploaded textures to draw
	while (pc < lines.size()) {
		const std::string line = lines[pc++];
		const size_t sp = line.find(' ');
		const std::string cmd = line.substr(0, sp), arg = sp == std::string::npos ? "" : line.substr(sp + 1);
		if (cmd == "wait") { waitFrames = atoi(arg.c_str()); return; }
		if (cmd == "waitidle") { waitIdle = true; return; }
		if (cmd == "waitms") { pendingWaitMs = std::max(1, atoi(arg.c_str())); return; }
		if (cmd == "waits3dchange") { pendingWaitMs = std::max(1, atoi(arg.c_str())) * 1000; pendingWaitS3d = true; return; }
		if (cmd == "quit") { done = true; gAppQuit = true; return; }
		if (cmd == "openany") { openAny(arg); continue; }
		if (cmd == "pattern") { setView(0, atoi(arg.c_str())); continue; }
		if (cmd == "savechar") { const bool ok = saveActive(); FILE *lf = fopen((gStartup.uiScript + ".log").c_str(), "a"); if (lf) { fprintf(lf, "savechar -> %s\n", ok ? "ok" : "FAILED"); fclose(lf); } waitFrames = 20; return; }
		if (cmd == "zoomview") { setView(2, (int)(atof(arg.c_str()) * 100.0 + 0.5)); continue; }
		if (cmd == "viewframe") { setView(1, atoi(arg.c_str())); continue; }
		if (cmd == "playwait") { pendingPlayWaitS = std::max(1, atoi(arg.c_str())); waitFrames = 2; return; }
		{   // playingame [loop|noloop] | playloop on|off | playstop (fpanim_play.cpp)
			std::string pmsg;
			if (fpanimplay::ScriptCommand(cmd, arg, &pmsg)) { LogLine((cmd + " " + arg + " -> " + pmsg).c_str()); waitFrames = 3; return; }
		}
		{   // hotsave <status|on|off|mode|tomods|gamedir> | framedur P F N (ui/main_frame_hotsave.cpp)
			std::string hmsg;
			if (mf.HotSaveScriptCommand(cmd, arg, &hmsg)) { LogLine((cmd + " " + arg + " -> " + hmsg).c_str()); waitFrames = 3; return; }
		}
		{   // PACNyx-parity commands (ui/pacnyx_script_impl.h) first: `layer` in an RBO / GOF2 PAT editor tab is the part layer
			std::string pmsg;
			if (mf.PacnyxScriptCommand(cmd, arg, &pmsg)) {   // PAT editor / CG / export commands: result line in the log, a few frames to settle
				FILE *lf = fopen((gStartup.uiScript + ".log").c_str(), "a");
				if (lf) { fprintf(lf, "%s %s -> %s\n", cmd.c_str(), arg.c_str(), pmsg.empty() ? "ok" : pmsg.c_str()); fclose(lf); }
				waitFrames = 4; return;
			}
		}
		// Edits through the same model calls as the UI (S3 evidence): copypattern N | pastepattern N |
		// gameformat auto|mbaacc|uni2|unist|unib|mbtl | layer N | switchview N | opentrees 0/1/anim | tint R G B
		if (cmd == "captureview") { if (!editCmd(cmd, arg)) gStartup.exitCode = 1; continue; }   // the active view's FBO (works under --headless)
		if (cmd == "copypattern" || cmd == "pastepattern" || cmd == "gameformat" || cmd == "layer" || cmd == "switchview" || cmd == "opentrees" || cmd == "tint") {
			if (!editCmd(cmd, arg)) { FILE *lf = fopen((gStartup.uiScript + ".log").c_str(), "a"); if (lf) { fprintf(lf, "script line %zu '%s' failed\n", pc, line.c_str()); fclose(lf); } }
			waitFrames = 3; return;
		}
		if (cmd == "cgm") {   // cgm <open|tab|press|path|select|set|status|fxr ...> [arg]: the CG manager window (cgm_window.h Window::script)
			const size_t s2 = arg.find(' ');
			std::string msg; const bool ok = cgmScript(arg.substr(0, s2), s2 == std::string::npos ? "" : arg.substr(s2 + 1), &msg);
			if (!ok) { FILE *lf = fopen((gStartup.uiScript + ".log").c_str(), "a"); if (lf) { fprintf(lf, "line %zu '%s': %s\n", pc, line.c_str(), msg.c_str()); fclose(lf); } }
			if (arg.compare(0, 5, "press") == 0) { waitFrames = 4; return; }
			continue;
		}
		if (cmd == "framenodes") { extern int gUiScriptOpenFrameNodes; gUiScriptOpenFrameNodes = atoi(arg.c_str()); continue; }
		if (cmd == "capture") {
			int w = 0, h = 0;
			platform::ClientSize(w, h);
			std::vector<uint8_t> rgba((size_t)w * h * 4), rgb((size_t)w * h * 3);
			glReadBuffer(gStartup.headless ? GL_COLOR_ATTACHMENT0 : GL_BACK); glPixelStorei(GL_PACK_ALIGNMENT, 1);   // headless: MainFrame::Draw's frame target
			glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
			for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) for (int k = 0; k < 3; k++) rgb[((size_t)(h - 1 - y) * w + x) * 3 + k] = rgba[((size_t)y * w + x) * 4 + k];
			std::error_code ec; std::filesystem::create_directories(std::filesystem::u8path(arg).parent_path(), ec);
			WritePng(arg, rgb.data(), w, h);
			continue;
		}
		std::string msg;
		if (mf.PatScriptCommand(cmd, arg, &msg)) {   // PAT editor commands (ui/pat_editor_impl.h)
			if (!msg.empty()) { FILE *lf = fopen((gStartup.uiScript + ".log").c_str(), "a"); if (lf) { fprintf(lf, "%s\n", msg.c_str()); fclose(lf); } }
			waitFrames = 3;
			return;
		}
		if (stage3dview::ScriptCommand(cmd, arg, &msg)) {   // stage3d open|frame|cam|set|capture|status (stage3d/s3d_view.h)
			FILE *lf = fopen((gStartup.uiScript + ".log").c_str(), "a");
			if (lf) { fprintf(lf, "%s %s -> %s\n", cmd.c_str(), arg.c_str(), msg.c_str()); fclose(lf); }
			waitFrames = 3;
			return;
		}
		if (hmodui::ScriptCommand(cmd, arg, &msg)) {   // hmod <open|new|gamedir|index|status|check|export> (modproject/hmod_ui.h)
			FILE *lf = fopen((gStartup.uiScript + ".log").c_str(), "a");
			if (lf) { fprintf(lf, "%s %s -> %s\n", cmd.c_str(), arg.c_str(), msg.empty() ? "ok" : msg.c_str()); fclose(lf); }
			waitFrames = 3;
			return;
		}
		if (pcmodui::ScriptCommand(cmd, arg, &msg)) {   // pcmod <open|add|addfolder|set|export|status> (pcmod/pcmod_export_ui.h)
			FILE *lf = fopen((gStartup.uiScript + ".log").c_str(), "a");
			if (lf) { fprintf(lf, "%s %s -> %s\n", cmd.c_str(), arg.c_str(), msg.empty() ? "ok" : msg.c_str()); fclose(lf); }
			waitFrames = 3;
			return;
		}
		if (!abrowser::ScriptCommand(cmd, arg, &msg)) {
			FILE *lf = fopen((gStartup.uiScript + ".log").c_str(), "a");
			if (lf) { fprintf(lf, "script line %zu '%s' failed: %s\n", pc, line.c_str(), msg.c_str()); fclose(lf); }
		}
		if (cmd == "browse" || cmd == "source" || cmd == "select" || cmd == "type" || cmd == "filter" || cmd == "open" || cmd == "key") { waitFrames = 3; return; }   // let the UI process it
	}
	done = true; gAppQuit = true;
}

void MainFrame::ProcessStartupArgs()
{
	int n = ++gStartup.frameCounter;
	if (!gStartup.uiScript.empty() && n > 3)
		RunUiScript(*this, [this](const std::string &p) { openAnyFile(p); },
			[this](int what, int v) { if (auto *vw = getActiveView()) { if (what == 0) vw->getState().pattern = v; else if (what == 2) vw->setZoom(v / 100.f); else vw->getState().frame = v; } },
			[this]() { return getActiveCharacter() ? saveCharacter(getActiveCharacter()) : false; },
			[this](const std::string &cmd, const std::string &arg) -> bool {
				if (cmd == "switchview") { const int i = atoi(arg.c_str()); if (i < 0 || i >= (int)views.size()) return false; setActiveView(i); return true; }
				if (cmd == "captureview") return WriteViewTargetPng(getActiveView(), arg);
				CharacterView *vw = getActiveView();
				CharacterInstance *ch = getActiveCharacter();
				if (!vw || !ch) return false;
				FrameState &st = vw->getState();
				FrameData &fd = ch->frameData;
				if (cmd == "layer") { st.selectedLayer = atoi(arg.c_str()); return true; }
				if (cmd == "opentrees") { st.scriptOpenTrees = arg == "anim" ? 2 : arg != "0" ? 1 : 0; return true; }
				if (cmd == "tint") {
					// tint R G B (0..1): AFRG of every layer with a sprite in the current pattern
					float c[3] = {1, 1, 1};
					if (sscanf(arg.c_str(), "%f %f %f", &c[0], &c[1], &c[2]) != 3) return false;
					Sequence *cur = fd.get_sequence(st.pattern);
					if (!cur) return false;
					for (auto &f : cur->frames) for (auto &l : f.AF.layers) if (l.spriteId >= 0) { l.rgba[0] = c[0]; l.rgba[1] = c[1]; l.rgba[2] = c[2]; }
					fd.mark_modified(st.pattern);
					return true;
				}
				if (cmd == "gameformat") {
					const std::string &g = arg;
					fd.setGameOverride(g == "mbaacc" ? Ha6Game::MBAACC : g == "uni2" ? Ha6Game::UNI : g == "mbtl" ? Ha6Game::MBTL
						: g == "unib" ? Ha6Game::UNIB : g == "unist" ? Ha6Game::UNIST : Ha6Game::Auto);
					return true;
				}
				Sequence *seq = fd.get_sequence(atoi(arg.c_str()));
				if (!seq || !st.copied) return false;
				if (cmd == "copypattern") { st.copied->pattern = *seq; return true; }
				Sequence pasted; pasted = st.copied->pattern;
				st.pasteNotice = FormatConvertNotice(fd.conformPasted(pasted));
				*seq = std::move(pasted);
				fd.mark_modified(atoi(arg.c_str()));
				st.pattern = atoi(arg.c_str()); st.frame = 0;
				return true;
			},
			[this](const std::string &c, const std::string &a, std::string *m) { return m_cgm.script(c, a, m); });
	if (n == 3 && !gStartup.cg.empty()) { if (auto *c = getActiveCharacter()) c->loadCG(gStartup.cg); }
	// [tag-panel] after --open (frame 2), so the pickers see the loaded character; works without --open too
	// [authoring] --tool authoring (and --tool tag without --tag-ini: the old panel's entry now opens Authoring > Tuning)
	const bool authoringTool = gStartup.tool == "authoring" || (gStartup.tool == "tag" && gStartup.tagIni.empty());
	if (n == 3 && authoringTool) {
		authoring::StartupOptions o = gStartup.authoring;
		if (gStartup.tool == "tag" && o.tab.empty()) o.tab = "Tuning";
		authoring::ApplyStartup(o);
	}
	if (n == 60 && authoringTool && gStartup.authoring.loadInGame) authoring::StartupLoadInGame();
	if (n == 3 && !authoringTool && (gStartup.tool == "tag" || !gStartup.tagIni.empty() || !gStartup.tagTab.empty() || gStartup.tagLink))
		tagpanel::OpenStartup(gStartup.tagIni, gStartup.tagChar, gStartup.tagTab);
	if (n == 3 && gStartup.tagLink) gamelink::SharedClient().Connect();
	if (n == 150 && gStartup.tagApply) tagpanel::StartupApply();
	if (n == 200 && gStartup.tagLink && !gStartup.tagTab.empty())   // re-select the tab once the link has state
		tagpanel::OpenStartup("", "", gStartup.tagTab);
	if (n == 3 && gStartup.tool == "gamelink") gamelink::showPanel = true;
	if (n == 4 && !gStartup.openAny.empty()) {
		size_t from = 0;
		while (from <= gStartup.openAny.size()) {
			size_t bar = gStartup.openAny.find('|', from);
			if (bar == std::string::npos) bar = gStartup.openAny.size();
			if (bar > from) openAnyFile(gStartup.openAny.substr(from, bar - from));
			from = bar + 1;
		}
	}
	// Open one file the way --open does (type by extension / content). True if a new view came up.
	auto openPath = [this](const std::string &path) -> bool {
		const size_t viewsBefore = views.size();
		std::string ext;
		size_t dot = path.find_last_of('.');
		if (dot != std::string::npos) ext = path.substr(dot);
		for (auto &c : ext) c = (char)tolower((unsigned char)c);
		const size_t cc = path.find("::");
		if (cc != std::string::npos) {   // <archive.p>::<ENTRY.DAT>: a GOF1 character straight out of its archive
			auto character = std::make_unique<CharacterInstance>(); std::string err;
			if (character->loadGof1(path.substr(0, cc), path.substr(cc + 2), err)) { characters.push_back(std::move(character)); createViewForCharacter(characters.back().get()); }
			else requestErrorPopup("Load Error", err);
		} else if (ext == ".hproj") {
			loadProjectFromPath(path, false);
		} else if (ext == ".dt2" || ext == ".pac" || ext == ".p" || ext == ".chp" || (ext == ".pat" && han2StartupSniffPat(path)) || (ext == ".dat" && han2StartupSniff(path))) {
			openHan2File(path);   // RBO / GOF2 character or PAC archive
		} else if (ext == ".dat") {
			loadStageFile(path);   // [stage-link] a bgmake stage (with --game-link: the Stage section's files)
		} else {
			auto character = std::make_unique<CharacterInstance>();
			bool ok = ext == ".txt" ? character->loadFromTxt(path) : character->loadHA6(path, false);
			if (ok) {
				characters.push_back(std::move(character));
				createViewForCharacter(characters.back().get());
			} else {
				requestErrorPopup("Load Error", std::string(TXT("Could not open ")) + path);
			}
		}
		return views.size() > viewsBefore;
	};
	// --tick / batch `tick`: the root frame follows the simulated flow at that tick.
	auto applyTick = [](CharacterView *v, int tick) {
		if (!v || !v->getCharacter() || tick < 0) return;
		auto &st = v->getState();
		CharacterInstance *c = v->getCharacter();
		FrameData *fx = c->effectCharacter ? &c->effectCharacter->frameData : nullptr;
		auto &sim = st.BindPreviewSim(&c->frameData, fx);
		sim.ensureSimulatedTo(tick);
		st.currentTick = std::min(tick, sim.horizon());
		const auto &track = sim.rootFrameTrack();
		if (st.currentTick < (int)track.size()) st.frame = track[st.currentTick];
	};
	// --center X,Y: that world point at the middle of the view (Render::BeginPass: screen = origin + world * zoom).
	auto applyCenter = [](CharacterView *v) {
		if (!v || !gStartup.hasCenter || v->isStageView()) return;
		ViewCamera &cam = v->camera();
		cam.panX = -gStartup.centerX * cam.zoom;
		cam.panY = -gStartup.centerY * cam.zoom;
	};
	if (n == 2 && !gStartup.open.empty()) {
		openPath(gStartup.open);
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
			g_ha6GameOverride = g == "mbaacc" ? Ha6Game::MBAACC : (g == "uni" || g == "uni2") ? Ha6Game::UNI
				: g == "mbtl" ? Ha6Game::MBTL : g == "unib" ? Ha6Game::UNIB : g == "unist" ? Ha6Game::UNIST : Ha6Game::Auto;
		}
		const std::string& t = gStartup.tool;
		if (t == "hud") m_showHud = true;
		else if (t == "bgm") m_showBgm = true;
		else if (t == "compare") m_showCompare = true;
		else if (t == "patterns") m_patMgr.open = true;
		else if (t == "cgm") m_cgm.open = true;
		else if (t == "notes") m_showNotes = true;
		else if (t == "vars") m_varRefs.open = true;
		else if (t == "keys") m_showKeyBindings = true;
		else if (t == "han2anim") { han2ui::showAnimWindow = true; if (auto *v = getActiveView()) v->onion().enabled = true; }

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
			if (gStartup.tick >= 0) applyTick(v, gStartup.tick);
			applyCenter(v);
			if (gStartup.onionBefore >= 0) {
				auto &o = v->onion();
				o.enabled = true;
				o.before = gStartup.onionBefore;
				o.after = gStartup.onionAfter;
				o.spacing = std::max(1, gStartup.onionSpacing);
				o.keyframesOnly = gStartup.onionKeyframes;
			}
			if (gStartup.detach && !gStartup.headless) {   // headless: no OS windows besides the hidden main one
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
	if (n == 2 && gStartup.hideBoxes) m_showBoxes = false;
	if (n >= 2 && n < 20 && gStartup.hideSpawns)   // views open over the first frames; keep it off until the capture
		for (auto &v : views) { v->getState().vizSettings.showSpawnedPatterns = false; v->getState().forceSpawnTreeRebuild = true; }
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
	if (!gStartup.captureView.empty() && n == 20 && !WriteViewTargetPng(findViewById(m_startupViewId), gStartup.captureView))
		gStartup.exitCode = 1;
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
	if (gStartup.capture.empty() && anyWave2 && n == 22 && gStartup.batch.empty())
		gAppQuit = true;
	// --batch <file>: one shot per line, commands split by ';' (see startup_args.h). Settings carry over between
	// lines; each `export` waits a few frames (more after an open) so the view has drawn the new state, then writes
	// the view's render target. Exit code: 0 all shots written, 1 a shot failed, 2 the batch could not be read.
	if (!gStartup.batch.empty() && n >= 3) {
		static std::vector<std::string> lines;
		static size_t pc = 0;
		static int settle = -1;
		static std::string pendingOut;
		static bool loaded = false, done = false;
		static std::map<std::string, uint64_t> openedViews;   // path -> view id: a repeated `open` reuses the view
		auto finish = [](int code) { done = true; if (code > gStartup.exitCode) gStartup.exitCode = code; gAppQuit = true; };
		if (!done && !loaded) {
			loaded = true;
			std::ifstream f(std::filesystem::u8path(gStartup.batch));
			std::string l;
			while (std::getline(f, l)) {
				while (!l.empty() && (l.back() == '\r' || l.back() == ' ' || l.back() == '\t')) l.pop_back();
				size_t st = l.find_first_not_of(" \t");
				if (st != std::string::npos && l[st] != '#') lines.push_back(l.substr(st));
			}
			if (lines.empty()) { HC_LOG_ERROR("--batch {}: no lines", gStartup.batch); finish(2); }
		}
		if (!done && settle > 0) settle--;
		else if (!done && settle == 0) {
			if (!WriteViewTargetPng(getActiveView(), pendingOut)) gStartup.exitCode = 1;
			settle = -1;
		}
		while (!done && settle < 0 && pc < lines.size()) {
			const std::string line = lines[pc++];
			bool opened = false;
			std::string exportPath;
			size_t from = 0;
			while (from <= line.size()) {
				size_t semi = line.find(';', from);
				if (semi == std::string::npos) semi = line.size();
				std::string c = line.substr(from, semi - from);
				from = semi + 1;
				const size_t b0 = c.find_first_not_of(" \t");
				if (b0 == std::string::npos) continue;
				c = c.substr(b0);
				while (!c.empty() && (c.back() == ' ' || c.back() == '\t')) c.pop_back();
				const size_t sp = c.find_first_of(" \t");
				const std::string key = c.substr(0, sp);
				std::string val = sp == std::string::npos ? "" : c.substr(c.find_first_not_of(" \t", sp));
				if (val.size() >= 2 && val.front() == '"' && val.back() == '"') val = val.substr(1, val.size() - 2);
				bool ok = true;
				CharacterView *v = getActiveView();
				if (key == "open") {
					auto it = openedViews.find(val);
					int idx = -1;
					if (it != openedViews.end())
						for (size_t k = 0; k < views.size(); k++) if (views[k]->getId() == it->second) idx = (int)k;
					if (idx >= 0) setActiveView(idx);
					else if (openPath(val)) {
						opened = true;
						if (CharacterView *nv = getActiveView()) {
							openedViews[val] = nv->getId();
							if (gStartup.hideSpawns) { nv->getState().vizSettings.showSpawnedPatterns = false; nv->getState().forceSpawnTreeRebuild = true; }
						}
					} else ok = false;
				}
				else if (key == "pattern" && v) { v->getState().pattern = atoi(val.c_str()); v->getState().frame = 0; }
				else if (key == "frame" && v) v->getState().frame = atoi(val.c_str());
				else if (key == "tick") applyTick(v, atoi(val.c_str()));
				else if (key == "zoom") { const float z = (float)atof(val.c_str()); if (z > 0.f) SetZoom(z); else ok = false; }
				else if (key == "center") ok = ParseCenterArg(val);
				else if (key == "bg") ok = ParseBgArg(val);
				else if (key == "size") ok = ParseSizeArg(val);
				else if (key == "layer") ok = ParseLayerArg(val);
				else if (key == "boxes") m_showBoxes = val != "0";
				else if (key == "spawns") {
					gStartup.hideSpawns = val == "0";
					for (auto &vw : views) { vw->getState().vizSettings.showSpawnedPatterns = !gStartup.hideSpawns; vw->getState().forceSpawnTreeRebuild = true; }
				}
				else if (key == "export") exportPath = val;
				else ok = false;
				if (!ok) { HC_LOG_ERROR("--batch line {} '{}': '{}' failed", pc, line, c); gStartup.exitCode = 1; }
			}
			applyCenter(getActiveView());
			if (!exportPath.empty()) { pendingOut = exportPath; settle = opened ? 18 : 3; }
		}
		if (!done && settle < 0 && pc >= lines.size()) finish(0);
	}
	if (n == 2 && gStartup.gameLinkSlot >= 1 && gStartup.gameLinkSlot <= 4)
		gamelink::StartFollowing(gStartup.gameLinkSlot - 1);
	if (!gStartup.capture.empty() && n == ((gStartup.gameLinkSlot || gStartup.tagLink || gStartup.authoring.link) ? 240 : 20)) {
		int w = 0, h = 0;
		platform::ClientSize(w, h);
		std::vector<uint8_t> rgba((size_t)w * h * 4), rgb((size_t)w * h * 3);
		glReadBuffer(gStartup.headless ? GL_COLOR_ATTACHMENT0 : GL_BACK);
		glPixelStorei(GL_PACK_ALIGNMENT, 1);
		glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
		for (int y = 0; y < h; y++)
			for (int x = 0; x < w; x++)
				for (int k = 0; k < 3; k++)
					rgb[((size_t)(h - 1 - y) * w + x) * 3 + k] = rgba[((size_t)y * w + x) * 4 + k];
		WritePng(gStartup.capture, rgb.data(), w, h);
		if (gStartup.saveProject.empty()) gAppQuit = true;
	}
	if (!gStartup.capture.empty() && !gStartup.saveProject.empty() && n == 22)
		gAppQuit = true;
}
