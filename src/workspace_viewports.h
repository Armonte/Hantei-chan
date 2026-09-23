#ifndef WORKSPACE_VIEWPORTS_H_GUARD
#define WORKSPACE_VIEWPORTS_H_GUARD

#include <windows.h>

class ContextGl;

// Raw Win32 + OpenGL needs a small bridge for Dear ImGui platform windows
// (detached, multi-monitor windows). This module owns only the secondary
// windows' device contexts; the application's one OpenGL context (ContextGl)
// renders every window in turn, which is why FBO textures made in the main
// window can be shown in a detached one.
//
// Adapted from Gonptechan EX (drop 1abc27f9). Unlike EX, the vendored ImGui
// Win32 backend is not patched: keyboard shortcuts in detached windows are
// routed by subclassing each platform HWND from app code (SetWindowSubclass),
// installed by wrapping platform_io.Platform_CreateWindow.
namespace WorkspaceViewports
{
	// Called for WM_KEYDOWN / WM_SYSKEYDOWN in a detached window, after ImGui
	// has seen the message. Return true if the key was consumed.
	using KeyHook = bool (*)(HWND window, UINT message, WPARAM wParam, LPARAM lParam);

	bool Initialize(ContextGl& context, HWND mainWindow, bool enabled, KeyHook keyHook);
	// ImGui::UpdatePlatformWindows + render every secondary window, then make
	// the main window current again. No-op when disabled.
	void RenderSecondaryWindows();
	void Shutdown();
	bool IsEnabled();

	// Test hook: on the next RenderSecondaryWindows, read back each detached
	// window's back buffer right before it is presented (top-down RGBA).
	using CaptureFn = void (*)(int index, int width, int height, const unsigned char* rgba, void* user);
	void RequestCapture(CaptureFn fn, void* user);
}

#endif
