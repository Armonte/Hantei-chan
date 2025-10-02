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

#endif /* MAIN_H_GUARD */
