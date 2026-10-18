#ifndef PLATFORM_PLATFORM_H_GUARD
#define PLATFORM_PLATFORM_H_GUARD

// src/platform: the only place that knows the windowing API (PLAN.md 5). P0
// moves the Win32 window and the wgl GL context here onto SDL3, with the
// renderer output unchanged; P2 adds the D3D9 device on the same window handle.
//
// The signatures keep the old Win32 names where callers already used them
// (mainWindowHandle, clientRect), so the port stays a small mechanical diff.

#include <cstdint>
#include <string>

struct ImVec2;

namespace platform {

// Opaque handle to the app's single main window. On Windows the underlying HWND
// is still available (see NativeWindowHandle): P0 keeps the Win32 ImGui
// backend's expectations, P2 hands the same handle to D3D9.
struct AppWindow;

// Window + GL context + ImGui platform state. headless creates a hidden window
// (SDL_WINDOW_HIDDEN) that is never shown and gets no taskbar entry, exactly
// like the old WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE window.
struct InitDesc {
	std::string title;      // UTF-8
	int x = 0, y = 0;
	int w = 1280, h = 720;
	bool headless = false;
	bool maximized = false;
	bool openGL = true;     // P2: false creates the window without SDL_WINDOW_OPENGL
	// Renamed from the ini's detachableWindows: multi-viewport needs native
	// windows, which headless never creates.
	bool allowViewports = false;
};

// Creates the window, loads GL (unless openGL is false) and initialises SDL.
// Returns false and fills the error string when the window or GL context failed.
bool Init(const InitDesc& desc, std::string& error);
void Shutdown();

AppWindow* Window();
bool IsHeadless();

// Framebuffer size in pixels (SDL_GetWindowSizeInPixels). PLAN.md 6 P0 uses
// this wherever clientRect was used: SDL_WINDOW_HIGH_PIXEL_DENSITY makes the
// pixel size differ from the window size at fractional display scales.
void ClientSize(int& w, int& h);
// Window size in screen coordinates (what the OS window drag/resize changes).
void WindowSize(int& w, int& h);

// Window position, saved into the ini on move/resize.
void WindowPos(int& x, int& y);
bool IsMaximized();
void SetTitle(const std::string& utf8Title);

// The native window: HWND on Windows, nullptr elsewhere. P2 creates the D3D9
// device on it through SDL_PROP_WINDOW_WIN32_HWND_POINTER.
void* NativeWindowHandle();

// GL context (P0 only; deleted with the GL backend in P6).
void MakeGlCurrent();
void SwapGlBuffers();
// wglSwapIntervalEXT(0) / SDL_GL_SetSwapInterval: vsync off, as before.
void SetSwapInterval(int interval);

// DPI scale of the display the window is on (1.0 when unknown).
float DisplayScale();

// Event pump: the caller keeps its own message handling; this only translates
// SDL events into the app's callbacks.
struct EventHandlers {
	void (*onResize)(int fbW, int fbH, void* user) = nullptr;
	void (*onMove)(int x, int y, void* user) = nullptr;
	void (*onDropFile)(const char* utf8Path, void* user) = nullptr;
	void (*onDropPoint)(float screenX, float screenY, float clientX, float clientY, void* user) = nullptr;
	void* user = nullptr;
};
void SetEventHandlers(const EventHandlers& h);

// One pump step: translates every queued SDL event (input goes to ImGui through
// imgui_impl_sdl3 first, then to the app's own switch) and returns false once a
// quit was requested.
bool PollEvents();

}  // namespace platform

#endif /* PLATFORM_PLATFORM_H_GUARD */
