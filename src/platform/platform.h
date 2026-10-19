#ifndef PLATFORM_PLATFORM_H_GUARD
#define PLATFORM_PLATFORM_H_GUARD

// src/platform: the only place that knows the windowing API (PLAN.md 5). P0
// moves the Win32 window and its GL context here onto SDL3, with the
// renderer output unchanged; P2 adds the D3D9 device on the same window handle.
//
// The signatures keep the old Win32 names where callers already used them
// (mainWindowHandle, clientRect), so the port stays a small mechanical diff.

#include <cstdint>
#include <cstdio>
#include <string>

struct ImVec2;

namespace platform {

// Opaque handle to the app's main window (an SDL_Window). On Windows the
// underlying HWND is still available (see NativeWindowHandle) for the file
// dialogs' owner, and P2 hands the same handle to D3D9.
struct AppWindow;

// Window + GL context + ImGui platform state. headless creates a hidden window
// (SDL_WINDOW_HIDDEN) that is never shown and gets no taskbar entry, exactly
// like the old WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE window.
struct InitDesc {
	std::string title;      // UTF-8
	int x = 0, y = 0;       // outer window rectangle, as the ini stores it
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

// Window position (of the client area, SDL's convention).
void WindowPos(int& x, int& y);
// The outer window rectangle, title bar and borders included (GetWindowRect):
// what the ini's posX/posY/sizeX/sizeY hold and InitDesc x/y/w/h take.
void WindowOuterRect(int& x, int& y, int& w, int& h);
bool IsMaximized();
// Title of a window this layer created (MainFrame holds the main one).
void SetTitle(AppWindow* window, const std::string& utf8Title);

// The native window: HWND on Windows, nullptr elsewhere. P2 creates the D3D9
// device on it through SDL_PROP_WINDOW_WIN32_HWND_POINTER.
void* NativeWindowHandle();

// GL context (P0 only; deleted with the GL backend in P6).
void MakeGlCurrent();
void SwapGlBuffers();
// SDL_GL_SetSwapInterval: the app passes 0 (vsync off), as it always has.
void SetSwapInterval(int interval);
// The main window's SDL_GLContext, for ImGui_ImplSDL3_InitForOpenGL: the SDL3
// backend creates each detached window's context sharing objects with it
// (SDL_GL_SHARE_WITH_CURRENT_CONTEXT), so the views' FBO textures made here
// can be drawn into a detached window.
void* GlContextHandle();

// The main window's SDL_WindowID. Every SDL event names the window it belongs
// to; with detached windows (ImGui viewports) the queue carries their events
// too, and the app's own handling (resize, move, close, view mouse drags) is
// for the main window only, like the old WndProc.
uint32_t MainWindowId();

// DPI scale of the display the window is on (1.0 when unknown).
float DisplayScale();

// SDL scancode -> Win32 virtual-key code. The shortcut registry, the ini
// bindings and --post-key are all expressed in virtual-key codes, so the SDL
// event loop translates instead of changing them (PLAN.md 6 P0 step 3 keeps the
// wWinMain argument semantics). Takes the scancode as int to avoid an SDL
// include here; returns 0 for a key the app has no binding for.
uint32_t VkFromScancode(int sdlScancode);
// The reverse, for --post-key (PLAN.md 6 P0 step 6): builds an SDL_EVENT_KEY_DOWN
// for the virtual key. Returns 0 when the key has no scancode.
int ScancodeFromVk(uint32_t vk);

// MessageBox() lives in platform_message.h (header only, no link dependency):
// the small tool targets share src/shader.cpp and src/vao.cpp with the app but
// do not build this layer. platform_message.h is the include for callers.

// Event pump: the caller keeps its own message handling; this only translates
// SDL events into the app's callbacks.
struct EventHandlers {
	void (*onResize)(int fbW, int fbH, void* user) = nullptr;
	void (*onMove)(int x, int y, void* user) = nullptr;
	void (*onDropFile)(const char* utf8Path, void* user) = nullptr;
	// The drop point in desktop coordinates and in the main window's client
	// coordinates (a drop on a detached window lands outside the client area).
	void (*onDropPoint)(float screenX, float screenY, float clientX, float clientY, void* user) = nullptr;
	// Runs for every event, from every window. It feeds ImGui first
	// (ImGui_ImplSDL3_ProcessEvent) and then runs the app's own switch, which
	// is the order the old WndProc used.
	void (*onEvent)(const void* sdlEvent, void* user) = nullptr;
	// True once the app wants out of the loop (window close request).
	bool (*onQuitRequested)(void* user) = nullptr;
	void* user = nullptr;
};
void SetEventHandlers(const EventHandlers& h);

// One pump step: translates every queued SDL event into the callbacks above and
// returns false once a quit was requested (SDL_EVENT_QUIT, or the main window's
// close button; a detached window's close button only closes that window).
bool PollEvents();

}  // namespace platform

#endif /* PLATFORM_PLATFORM_H_GUARD */
