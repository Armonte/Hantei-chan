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

// Key events -> Win32 virtual-key codes (platform_keys.cpp). The shortcut
// registry, the ini bindings and --post-key are all expressed in the codes the
// old WndProc got in WM_KEYDOWN's wParam, so the SDL event loop translates
// instead of changing them. Plain integers here, so this header needs no SDL
// include: scancode is an SDL_Scancode, keycode an SDL_Keycode, mod an
// SDL_Keymod and raw the event's raw (platform) scan code.
struct KeyParts {
	int scancode = 0;
	uint32_t keycode = 0;
	uint16_t mod = 0;
	uint16_t raw = 0;
};
// The VK of a key event. raw goes through vscToVk (on Windows
// MapVirtualKey(raw, MAPVK_VSC_TO_VK_EX): the active keyboard layout, as the
// keyboard driver did for WM_KEYDOWN); the numeric keypad follows NumLock
// (mod) like the driver; the sided modifiers come back as the generic
// VK_SHIFT/VK_CONTROL/VK_MENU. Without a raw code (or a mapper) letters come
// from the keycode and the rest from a fixed scancode table. 0 = no VK.
uint32_t VkFromKeyParts(const KeyParts& k, uint32_t (*vscToVk)(uint32_t raw));
// VkFromKeyParts with the platform's own mapper (MapVirtualKeyW on Windows).
uint32_t VkFromKeyEvent(int sdlScancode, uint32_t keycode, uint16_t mod, uint16_t raw);
// True when NumLock is on and Shift is up: the keypad types digits.
bool NumpadIsDigitMode(uint16_t mod);

// Chord modifier bits, the same values as the shortcut registry's
// shortcutCtrl / shortcutShift / shortcutAlt.
enum : uint32_t { kModCtrl = 1u << 0, kModShift = 1u << 1, kModAlt = 1u << 2 };
// The chord modifiers of a key event, read from the event's own mod state (not
// the keyboard state at processing time, which has moved on by the time a
// batch of queued events is handled). Shift + a keypad digit with NumLock on
// drops Shift, as the driver's fake Shift release did for WM_KEYDOWN.
uint32_t ChordModsFromKey(int sdlScancode, uint16_t mod);
// The chord modifiers as of the event being dispatched now (kMod* bits):
// updated from every key event in queue order, so a mouse event handled after
// a Ctrl release sees Ctrl up, as GetKeyState did inside WM_MOUSEMOVE.
uint32_t EventKeyMods();

// A key that is only a modifier (Shift, Ctrl, Alt or AltGr, either side, and
// the Windows keys): the shortcut recorder waits for the next key instead.
bool IsModifierOnlyVk(uint32_t vk);

// WM_MOUSEWHEEL's rule for the main view: +1 zooms in, -1 out, 0 = not a
// vertical wheel step (a horizontal scroll has y == 0 and must not zoom).
int WheelZoomStep(float wheelY, bool invert);

// The reverse, for --post-key (PLAN.md 6 P0 step 6): the SDL scancode for a
// virtual key. Returns 0 when the key has no scancode.
int ScancodeFromVk(uint32_t vk);
// A key event for --post-key: scancode, mod (NumLock for the keypad digits) and,
// on Windows, the raw scan code under the active layout, chosen so that
// VkFromKeyEvent gives back the same VK (the generic code for a sided
// modifier). False when the key has neither a scancode nor a raw code.
bool PostKeyParts(uint32_t vk, KeyParts& out);

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
