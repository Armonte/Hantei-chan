// SDL3 implementation of src/platform/platform.h (PLAN.md 6 P0 step 2).
//
// P0 keeps the GL backend, so this creates an SDL window with an OpenGL 3.3
// context (the same attributes ContextGl used: 24-bit depth, 8-bit stencil)
// and loads glad through SDL_GL_GetProcAddress. The Win32 HWND is still
// reachable (NativeWindowHandle) for the file dialogs' owner window. Detached
// windows are ImGui viewports, created by imgui_impl_sdl3 with their own
// shared GL context (src/platform/viewports.cpp).
//
// The frame is always drawn into an offscreen target and blitted, so with GL
// on Windows this module only owns the window, the context and the swap.

#include "platform.h"
#include "platform_message.h"
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
	g_platformHeadless = desc.headless;

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
		// The old ContextGl used wglCreateContext, which is a compatibility
		// profile; the app's GLSL is #version 330 core, which both profiles
		// accept, but the default vertex array object and the client-array
		// entry points only work under compatibility. Keeping it is what makes
		// P0 output-identical.
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
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
	// Client-size correction. The old CreateWindowEx took the ini's winSizeX/Y
	// as the OUTER window size, so a 1280x800 setting gave a 1264x761 client
	// area (16 px of side border, 39 px of title bar on Windows 10). SDL's
	// SDL_CreateWindow takes the CLIENT size, so the requested size has to be
	// reduced by the border/title size to land on the same client area and keep
	// the corpus byte-identical. "--size WxH" and the headless FBO follow
	// clientRect, so both sides see the same number. The position is the outer
	// rectangle's too, and SDL positions the client area, so it moves by the
	// top-left border.
	{
		int top = 0, left = 0, bottom = 0, right = 0;
		if (SDL_GetWindowBordersSize(g.window, &top, &left, &bottom, &right)) {
			const int clientW = std::max(1, desc.w - (left + right));
			const int clientH = std::max(1, desc.h - (top + bottom));
			if (clientW != desc.w || clientH != desc.h)
				SDL_SetWindowSize(g.window, clientW, clientH);
		}
		SDL_SetWindowPosition(g.window, desc.x + left, desc.y + top);
	}
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

void WindowOuterRect(int& x, int& y, int& w, int& h)
{
	x = y = w = h = 0;
	if (!g.window) return;
	SDL_GetWindowPosition(g.window, &x, &y);
	SDL_GetWindowSize(g.window, &w, &h);
	int top = 0, left = 0, bottom = 0, right = 0;
	if (SDL_GetWindowBordersSize(g.window, &top, &left, &bottom, &right)) {
		x -= left; y -= top;
		w += left + right; h += top + bottom;
	}
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
	return (void*)g.gl;
}

uint32_t MainWindowId()
{
	return g.window ? (uint32_t)SDL_GetWindowID(g.window) : 0;
}

void SetEventHandlers(const EventHandlers& h) { g_handlers = h; }


bool PollEvents()
{
	if (!g.window) return false;
	bool running = true;
	const SDL_WindowID mainId = SDL_GetWindowID(g.window);
	SDL_Event e;
	while (SDL_PollEvent(&e)) {
		// Window events from a detached window (an ImGui viewport) are the SDL3
		// backend's business: its close button, size and position are not the
		// main window's, and must not quit the app or overwrite the ini.
		const bool mainWindow = e.window.windowID == mainId;
		switch (e.type) {
		case SDL_EVENT_QUIT:
			running = false;
			break;
		case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
			if (mainWindow) running = false;
			break;
		case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
		case SDL_EVENT_WINDOW_RESIZED:
			if (mainWindow && g_handlers.onResize) {
				int w = 0, h = 0;
				SDL_GetWindowSizeInPixels(g.window, &w, &h);
				g_handlers.onResize(w, h, g_handlers.user);
			}
			break;
		case SDL_EVENT_WINDOW_MOVED:
			if (mainWindow && g_handlers.onMove) g_handlers.onMove(e.window.data1, e.window.data2, g_handlers.user);
			break;
		case SDL_EVENT_DROP_POSITION:
		case SDL_EVENT_DROP_FILE:
			if (g_handlers.onDropPoint) {
				// SDL gives the point relative to the window under it; the app
				// wants it on the desktop (ImGui viewports work in desktop
				// coordinates) and in the main window's client area, as
				// WM_DROPFILES' DragQueryPoint + ClientToScreen gave it. A drop
				// on a detached window is routed by the desktop point.
				int wx = 0, wy = 0, mx = 0, my = 0;
				if (SDL_Window* w = SDL_GetWindowFromID(e.drop.windowID)) SDL_GetWindowPosition(w, &wx, &wy);
				SDL_GetWindowPosition(g.window, &mx, &my);
				const float sx = (float)wx + e.drop.x, sy = (float)wy + e.drop.y;
				g_handlers.onDropPoint(sx, sy, sx - (float)mx, sy - (float)my, g_handlers.user);
			}
			if (e.type == SDL_EVENT_DROP_FILE && g_handlers.onDropFile && e.drop.data)
				g_handlers.onDropFile(e.drop.data, g_handlers.user);
			break;
		default:
			break;
		}
		if (g_handlers.onEvent) g_handlers.onEvent(&e, g_handlers.user);
	}
	return running;
}

}  // namespace platform
