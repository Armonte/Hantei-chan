// SDL3 implementation of src/platform/platform.h (PLAN.md 6 P0 step 2).
//
// P0 keeps the GL backend, so this creates an SDL window with an OpenGL 3.3
// core context (the same attributes ContextGl used: 24-bit depth, 8-bit
// stencil) and loads glad through SDL_GL_GetProcAddress. The Win32 HWND is
// still reachable (NativeWindowHandle) because the vendored
// imgui_impl_win32 backend needs it and because the per-window GL bridge in
// workspace_viewports.cpp is written against it; P2 replaces both with the
// SDL3 ImGui backend and the D3D9 swap chains.
//
// The frame is always drawn into an offscreen target and blitted, so with GL
// on Windows this module only owns the window, the context and the swap.

#include "platform.h"
#include "../app_log.h"

#include <SDL3/SDL.h>
#include <glad/glad.h>

#include <algorithm>
#include <string>

#include <windows.h>

namespace platform {

namespace {

struct WindowState {
	SDL_Window* window = nullptr;
	SDL_GLContext gl = nullptr;
	bool headless = false;
	InitDesc desc;
};
WindowState g;
EventHandlers g_handlers;

}  // namespace

bool Init(const InitDesc& desc, std::string& error)
{
	g.desc = desc;
	g.headless = desc.headless;

	SDL_SetHint(SDL_HINT_APP_NAME, "gonptechan");

	SDL_WindowFlags flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
	if (desc.openGL) flags |= SDL_WINDOW_OPENGL;
	// --headless: hidden and never shown, and no taskbar entry (the old
	// WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE window). SDL_WINDOW_UTILITY keeps it
	// out of the taskbar on Windows the same way.
	if (desc.headless) flags |= SDL_WINDOW_HIDDEN | SDL_WINDOW_UTILITY;

	if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
		error = std::string("SDL_Init failed: ") + SDL_GetError();
		return false;
	}

	if (desc.openGL) {
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
		SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
		SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
		SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	}

	g.window = SDL_CreateWindow(desc.title.c_str(), desc.w, desc.h, flags);
	if (!g.window) {
		error = std::string("SDL_CreateWindow failed: ") + SDL_GetError();
		SDL_Quit();
		return false;
	}
	SDL_SetWindowPosition(g.window, desc.x, desc.y);
	if (desc.maximized && !desc.headless)
		SDL_MaximizeWindow(g.window);

	if (desc.openGL) {
		g.gl = SDL_GL_CreateContext(g.window);
		if (!g.gl) {
			error = std::string("SDL_GL_CreateContext failed: ") + SDL_GetError();
			SDL_DestroyWindow(g.window);
			g.window = nullptr;
			SDL_Quit();
			return false;
		}
		if (!SDL_GL_MakeCurrent(g.window, g.gl)) HC_LOG_WARN("platform: SDL_GL_MakeCurrent: {}", SDL_GetError());
		if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) {
			error = "gladLoadGLLoader failed";
			SDL_GL_DestroyContext(g.gl);
			g.gl = nullptr;
			SDL_DestroyWindow(g.window);
			g.window = nullptr;
			SDL_Quit();
			return false;
		}
		HC_LOG_INFO("platform: OpenGL {} via SDL3", (const char*)glGetString(GL_VERSION));
	}

	if (!desc.headless) SDL_ShowWindow(g.window);
	return true;
}

void Shutdown()
{
	if (g.gl) { SDL_GL_DestroyContext(g.gl); g.gl = nullptr; }
	if (g.window) { SDL_DestroyWindow(g.window); g.window = nullptr; }
	SDL_Quit();
}

AppWindow* Window() { return reinterpret_cast<AppWindow*>(g.window); }
bool IsHeadless() { return g.headless; }

void ClientSize(int& w, int& h)
{
	w = 0; h = 0;
	if (!g.window) return;
	if (!SDL_GetWindowSizeInPixels(g.window, &w, &h)) {
		// Offscreen / dummy backends may not report a pixel size: fall back to
		// the window size, which is what the old GetClientRect gave.
		SDL_GetWindowSize(g.window, &w, &h);
	}
}

void WindowSize(int& w, int& h)
{
	w = 0; h = 0;
	if (g.window) SDL_GetWindowSize(g.window, &w, &h);
}

void WindowPos(int& x, int& y)
{
	x = 0; y = 0;
	if (g.window) SDL_GetWindowPosition(g.window, &x, &y);
}

bool IsMaximized()
{
	return g.window && (SDL_GetWindowFlags(g.window) & SDL_WINDOW_MAXIMIZED);
}

void SetTitle(const std::string& utf8Title)
{
	if (g.window) SDL_SetWindowTitle(g.window, utf8Title.c_str());
}

void* NativeWindowHandle()
{
#ifdef _WIN32
	if (!g.window) return nullptr;
	return SDL_GetPointerProperty(SDL_GetWindowProperties(g.window),
	                              SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
#else
	return nullptr;
#endif
}

void MakeGlCurrent()
{
	if (g.window && g.gl) SDL_GL_MakeCurrent(g.window, g.gl);
}

void SwapGlBuffers()
{
	if (g.window) SDL_GL_SwapWindow(g.window);
}

void SetSwapInterval(int interval)
{
	if (g.window) SDL_GL_SetSwapInterval(interval);
}

float DisplayScale()
{
	if (!g.window) return 1.0f;
	const float s = SDL_GetWindowDisplayScale(g.window);
	return s > 0.0f ? s : 1.0f;
}

void* GlContextHandle()
{
#ifdef _WIN32
	// SDL's Windows GL context is the HGLRC itself (see
	// SDL_windowsopengl.c: GL_CreateContext stores the HGLRC as SDL_GLContext).
	return (void*)g.gl;
#else
	return nullptr;
#endif
}

void* WindowDeviceContext()
{
#ifdef _WIN32
	if (!g.window) return nullptr;
	// SDL exposes the window's HDC directly; it is stable for the window's life.
	return SDL_GetPointerProperty(SDL_GetWindowProperties(g.window),
	                              SDL_PROP_WINDOW_WIN32_HDC_POINTER, nullptr);
#else
	return nullptr;
#endif
}

void SetEventHandlers(const EventHandlers& h) { g_handlers = h; }

bool PollEvents()
{
	if (!g.window) return false;
	bool running = true;
	SDL_Event e;
	while (SDL_PollEvent(&e)) {
		// The app's own switch runs after ImGui has seen the event, matching the
		// old WndProc (ImGui_ImplWin32_WndProcHandler first, then the switch).
		switch (e.type) {
		case SDL_EVENT_QUIT:
		case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
			running = false;
			break;
		case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
		case SDL_EVENT_WINDOW_RESIZED:
			if (g_handlers.onResize) {
				int w = 0, h = 0;
				SDL_GetWindowSizeInPixels(g.window, &w, &h);
				g_handlers.onResize(w, h, g_handlers.user);
			}
			break;
		case SDL_EVENT_WINDOW_MOVED:
			if (g_handlers.onMove) g_handlers.onMove(e.window.data1, e.window.data2, g_handlers.user);
			break;
		case SDL_EVENT_DROP_POSITION:
			if (g_handlers.onDropPoint) g_handlers.onDropPoint(e.drop.x, e.drop.y, e.drop.x, e.drop.y, g_handlers.user);
			break;
		case SDL_EVENT_DROP_FILE:
			if (g_handlers.onDropFile) g_handlers.onDropFile(e.drop.data, g_handlers.user);
			break;
		default:
			break;
		}
		if (g_handlers.onEvent) g_handlers.onEvent(&e, g_handlers.user);
	}
	return running;
}

}  // namespace platform
