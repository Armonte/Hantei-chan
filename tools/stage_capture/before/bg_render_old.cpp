// Build (throwaway): git worktree add --detach <dir> 9d751c6; copy this file to <dir>/src and png_writer.{h,cpp}
// from feat/stage; add an executable target like bg_render (see CMakeLists bg_render) named bg_render_old.
// "Before" renderer: base-commit bg::Renderer at game camera (0,0), zoom z, after N sim ticks.
#include <windows.h>
#include <glad/glad.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include "context_gl.h"
#include "png_writer.h"
#include "background/bg_file.h"
#include "background/bg_renderer.h"
static LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM w, LPARAM l) { return DefWindowProcW(h, m, w, l); }
int main(int argc, char** argv) {
	std::string stage = argv[1], out = argv[2]; int ticks = atoi(argv[3]); float zoom = argc > 4 ? (float)atof(argv[4]) : 1.0f;
	const int W = 640, H = 480;
	WNDCLASSW wc = {}; wc.lpfnWndProc = WndProc; wc.hInstance = GetModuleHandleW(nullptr); wc.lpszClassName = L"bgold";
	RegisterClassW(&wc);
	HWND hwnd = CreateWindowW(wc.lpszClassName, L"", WS_OVERLAPPEDWINDOW, 0, 0, 64, 64, nullptr, nullptr, wc.hInstance, nullptr);
	ContextGl ctx(hwnd);
	if (!gladLoadGL()) return 2;
	bg::File file;
	if (!file.Load(stage.c_str())) return 3;
	for (int t = 0; t < ticks; ++t) file.TickRuntime();
	GLuint fbo, tex;
	glGenFramebuffers(1, &fbo); glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glGenTextures(1, &tex); glBindTexture(GL_TEXTURE_2D, tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, W, H, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
	glViewport(0, 0, W, H); glClearColor(0, 0, 0, 1); glClear(GL_COLOR_BUFFER_BIT);
	bg::Camera cam; cam.zoom = zoom; cam.SetPan(W * 0.5f / zoom, H * 0.9f / zoom);
	bg::Renderer r; r.SetFile(&file); r.SetEnabled(true); r.SetShowLights(false);
	r.Render(cam, W, H, bg::Pass::All); glFinish();
	std::vector<uint8_t> px((size_t)W * H * 4), fl(px.size());
	glReadPixels(0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
	for (int y = 0; y < H; ++y) { memcpy(&fl[(size_t)y * W * 4], &px[(size_t)(H - 1 - y) * W * 4], (size_t)W * 4); for (int x = 0; x < W; ++x) fl[((size_t)y * W + x) * 4 + 3] = 255; }
	std::string err; WritePngRgba(out, fl.data(), W, H, err);
	_exit(0);
}
