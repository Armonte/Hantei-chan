#ifndef MAIN_H_GUARD
#define MAIN_H_GUARD
#include <imgui.h>

#ifdef _WIN32
#include <windows.h>
extern HWND mainWindowHandle;
#else
struct GLFWwindow;
extern GLFWwindow* mainWindowHandle;
#endif

extern ImVec2 clientRect;

// Set by anything that used to call PostQuitMessage (the main loop reads it, and
// the exit code is gStartup.exitCode). SDL has no message queue to post a quit
// into, so the flag replaces it.
extern bool gAppQuit;

#endif /* MAIN_H_GUARD */
