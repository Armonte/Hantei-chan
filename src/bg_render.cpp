// bg_render: headless stage renderer for accuracy checks against the real game
// (docs/bg_research/STAGE_AUDIT.md, tools/stage_capture). Draws a stage through
// the editor's own bg::Renderer into an offscreen W x H target at a GAME camera
// (world -> screen = (world - cam) * zoom + (320, 432) at 640x480) and writes a PNG.
//
//   bg_render <stage.dat> --out <png> [--cam x,y[,zoom]] [--state <prefix>]
//             [--tick N] [--seed S] [--size WxH] [--pass all|back|front]
//             [--lights] [--no-weather] [--game mbaacc|mbac] [--clean]
// --clean draws the plain CG (no DXT5 / game UV insets).
//
// --state <prefix> loads a game snapshot written by stage_capture.sh shot:
//   <prefix>.pool (0x750840, 2000 x 44), <prefix>.drop (0x766000, particles at +8),
//   <prefix>.cam (int32 x, y in 1/128 px), <prefix>.zoom (float). The camera
//   comes from the snapshot unless --cam is given.
// Without --state the stage runtime is reset (seed) and ticked N times.
#include <windows.h>
#include <glad/glad.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>
#include "context_gl.h"
#include "png_writer.h"
#include "background/bg_file.h"
#include "background/bg_renderer.h"
#include "background/bg_gametex.h"

static bool ReadAll(const std::string& p, std::vector<uint8_t>& out) {
	std::ifstream f(p, std::ios::binary);
	if (!f) return false;
	out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
	return true;
}

static LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM w, LPARAM l) { return DefWindowProcW(h, m, w, l); }

int main(int argc, char** argv) {
	if (argc < 2) {
		fprintf(stderr, "usage: bg_render <stage.dat> --out <png> [--cam x,y[,zoom]] [--state prefix] [--tick N] "
		                "[--seed S] [--size WxH] [--pass all|back|front] [--lights] [--no-weather] [--game g]\n");
		return 1;
	}
	std::string stage = argv[1], out, statePrefix, passName = "all", gameName;
	float camX = 0, camY = 0, zoom = 1.0f;
	bool camGiven = false, lights = false, weather = true, cleanTex = false, standIns = false;
	float sx0 = -128.0f, sx1 = 128.0f;
	int solo = -1;   // file slot to draw alone
	float heatV = 0.0f, heatT = 0.0f;   // BgPointBlur preview value, time (s)
	bool patAuthoring = false;   // default Game-exact (accuracy checks)
	bool panGiven = false; float panX = 0, panY = 0;   // editor-style anchor: screen = (world + pan) * zoom
	int ticks = 0, seed = 0, W = 640, H = 480;
	bool seedGiven = false;
	for (int i = 2; i < argc; ++i) {
		std::string a = argv[i];
		auto next = [&]() -> const char* { return i + 1 < argc ? argv[++i] : ""; };
		if (a == "--out") out = next();
		else if (a == "--cam") { camGiven = true; sscanf(next(), "%f,%f,%f", &camX, &camY, &zoom); }
		else if (a == "--state") statePrefix = next();
		else if (a == "--tick") ticks = atoi(next());
		else if (a == "--seed") { seed = atoi(next()); seedGiven = true; }
		else if (a == "--size") sscanf(next(), "%dx%d", &W, &H);
		else if (a == "--pass") passName = next();
		else if (a == "--lights") lights = true;
		else if (a == "--no-weather") weather = false;
		else if (a == "--game") gameName = next();
		else if (a == "--clean") cleanTex = true;
		else if (a == "--solo") solo = atoi(next());
		else if (a == "--heat") sscanf(next(), "%f,%f", &heatV, &heatT);
		else if (a == "--pat") patAuthoring = std::string(next()) == "authoring";
		else if (a == "--pan") { panGiven = true; sscanf(next(), "%f,%f", &panX, &panY); }
		else if (a == "--standins") { standIns = true; sscanf(next(), "%f,%f", &sx0, &sx1); }
		else if (a == "--dxt-mode") bg::SetDxtHelperMode(next());
	}
	if (out.empty()) { fprintf(stderr, "--out required\n"); return 1; }

	WNDCLASSW wc = {};
	wc.lpfnWndProc = WndProc;
	wc.hInstance = GetModuleHandleW(nullptr);
	wc.lpszClassName = L"bg_render_hidden";
	RegisterClassW(&wc);
	HWND hwnd = CreateWindowW(wc.lpszClassName, L"bg_render", WS_OVERLAPPEDWINDOW, 0, 0, 64, 64,
	                          nullptr, nullptr, wc.hInstance, nullptr);
	ContextGl ctx(hwnd);
	if (!gladLoadGL()) { fprintf(stderr, "gladLoadGL failed\n"); return 2; }

	bg::File file;
	if (!file.Load(stage.c_str())) { fprintf(stderr, "load failed: %s\n", stage.c_str()); return 3; }
	if (gameName == "mbac") file.SetGame(bg::Game::MBAC);
	else if (gameName == "mbaacc") file.SetGame(bg::Game::MBAACC);
	if (seedGiven) file.SetSeed(seed);
	for (int t = 0; t < ticks; ++t) file.TickRuntime();

	if (!statePrefix.empty()) {
		std::vector<uint8_t> pool, drop, cam, zm;
		if (!ReadAll(statePrefix + ".pool", pool)) { fprintf(stderr, "no %s.pool\n", statePrefix.c_str()); return 4; }
		std::vector<uint8_t> dropParticles;
		if (ReadAll(statePrefix + ".drop", drop) && drop.size() > 8)
			dropParticles.assign(drop.begin() + 8, drop.end());
		int live = file.ImportGameState(pool, dropParticles.empty() ? nullptr : &dropParticles);
		printf("imported %d live instances\n", live);
		if (!camGiven) {
			if (ReadAll(statePrefix + ".cam", cam) && cam.size() >= 8) {
				int32_t cx, cy; std::memcpy(&cx, cam.data(), 4); std::memcpy(&cy, cam.data() + 4, 4);
				camX = cx / 128.0f; camY = cy / 128.0f;
			}
			if (ReadAll(statePrefix + ".zoom", zm) && zm.size() >= 4) std::memcpy(&zoom, zm.data(), 4);
		}
	}

	// Offscreen target.
	GLuint fbo = 0, tex = 0;
	glGenFramebuffers(1, &fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, W, H, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) { fprintf(stderr, "fbo incomplete\n"); return 5; }
	glViewport(0, 0, W, H);
	glClearColor(0, 0, 0, 1);
	glClear(GL_COLOR_BUFFER_BIT);

	// Game camera: world (0,0) lands at (W/2 - camX*z, H*0.9 - camY*z).
	bg::Camera camera;
	camera.zoom = zoom;
	if (panGiven) camera.SetPan(panX, panY);
	else camera.SetPan(W * 0.5f / zoom - camX, H * 0.9f / zoom - camY);
	camera.SetGameCam(camX, camY);

	if (solo >= 0) for (auto& o : file.GetObjects()) o.visible = o.originalIndex == solo;
	bg::Renderer r;
	r.SetFile(&file);
	r.SetEnabled(true);
	r.SetShowLights(lights);
	r.SetShowWeather(weather);
	r.SetGameTextures(!cleanTex);
	r.SetHeatPreview(heatV, heatT);
	r.SetPatPlacement(patAuthoring ? bg::Renderer::PatPlacement::Authoring : bg::Renderer::PatPlacement::Game);
	r.GetStandIns().enabled = standIns;
	r.GetStandIns().x[0] = sx0; r.GetStandIns().x[1] = sx1;
	bg::Pass pass = passName == "back" ? bg::Pass::Back : passName == "front" ? bg::Pass::Front : bg::Pass::All;
	r.Render(camera, W, H, pass);
	glFinish();

	std::vector<uint8_t> px((size_t)W * H * 4), flip((size_t)W * H * 4);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadPixels(0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
	for (int y = 0; y < H; ++y) {
		std::memcpy(&flip[(size_t)y * W * 4], &px[(size_t)(H - 1 - y) * W * 4], (size_t)W * 4);
		for (int x = 0; x < W; ++x) flip[((size_t)y * W + x) * 4 + 3] = 255;
	}
	std::string err;
	if (!WritePngRgba(out, flip.data(), W, H, err)) { fprintf(stderr, "png: %s\n", err.c_str()); return 6; }
	printf("wrote %s cam=(%.2f,%.2f) zoom=%.3f tick=%llu budget=%ldKB encoder=%s\n", out.c_str(), camX, camY, zoom,
	       (unsigned long long)file.GetTick(), r.GetTextureBudgetKB(), bg::EncoderName());
	_exit(0);
}
