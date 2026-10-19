// SDL3 multi-viewport support for detached windows (PLAN.md 6 P0 step 5).
//
// The backends do the work: imgui_impl_sdl3 creates a native window and a GL
// context that shares objects with the main one
// (SDL_GL_SHARE_WITH_CURRENT_CONTEXT), and imgui_impl_opengl3 renders each
// viewport's draw data into it. This module re-establishes the main context
// afterwards and implements --capture-windows through the renderer's
// per-viewport hook (Renderer_RenderWindow), where the Win32 bridge read
// GL_BACK in its own swap hook.

#include "viewports.h"

#include "platform.h"

#include <glad/glad.h>
#include <imgui.h>
#include <SDL3/SDL.h>

#include <algorithm>
#include <vector>

namespace viewports {
namespace {

bool g_enabled = false;
CaptureFn g_captureFn = nullptr;
void* g_captureUser = nullptr;
int g_captureIndex = 0;
// The renderer hook we wrap (restored in Shutdown).
void (*g_backendRenderWindow)(ImGuiViewport*, void*) = nullptr;

// Reads the back buffer of the viewport just drawn (its context is current),
// flips it to top-down RGBA (GL reads bottom-up) and hands it to the capture
// callback.
void CaptureViewport(ImGuiViewport* viewport)
{
	SDL_Window* window = SDL_GetWindowFromID((SDL_WindowID)(intptr_t)viewport->PlatformHandle);
	if (!window) return;
	int w = 0, h = 0;
	SDL_GetWindowSizeInPixels(window, &w, &h);
	if (w <= 0 || h <= 0) return;
	std::vector<unsigned char> up((size_t)w * h * 4), down((size_t)w * h * 4);
	glReadBuffer(GL_BACK);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, up.data());
	for (int y = 0; y < h; ++y)
		std::copy_n(up.data() + (size_t)(h - 1 - y) * w * 4, (size_t)w * 4,
		            down.data() + (size_t)y * w * 4);
	g_captureFn(++g_captureIndex, w, h, down.data(), g_captureUser);
}

// RenderPlatformWindowsDefault calls, per secondary viewport:
// Platform_RenderWindow (make its context current), Renderer_RenderWindow
// (draw), Platform_SwapBuffers (present). Reading after the draw and before
// the present is exactly the frame the window shows.
void RenderWindowHooked(ImGuiViewport* viewport, void* arg)
{
	if (g_backendRenderWindow) g_backendRenderWindow(viewport, arg);
	if (g_captureFn && viewport && viewport->PlatformHandle)
		CaptureViewport(viewport);
}

}  // namespace

bool Initialize(bool enabled)
{
	g_enabled = enabled;
	if (!enabled) return true;
	ImGuiPlatformIO& io = ImGui::GetPlatformIO();
	// Without both backends' hooks there is nothing to open or draw a
	// detached window with.
	if (!io.Platform_CreateWindow || !io.Platform_SwapBuffers || !io.Renderer_RenderWindow) {
		g_enabled = false;
		return false;
	}
	g_backendRenderWindow = io.Renderer_RenderWindow;
	io.Renderer_RenderWindow = RenderWindowHooked;
	return true;
}

void RenderSecondaryWindows()
{
	if (!g_enabled) return;
	ImGui::UpdatePlatformWindows();
	ImGui::RenderPlatformWindowsDefault();
	// Each viewport's render made its own context current; the main window's
	// present and the next frame expect the main one.
	platform::MakeGlCurrent();
	g_captureFn = nullptr;
	g_captureUser = nullptr;
}

bool IsEnabled()
{
	return g_enabled;
}

void RequestCapture(CaptureFn fn, void* user)
{
	g_captureFn = fn;
	g_captureUser = user;
	g_captureIndex = 0;
}

void Shutdown()
{
	if (g_enabled && ImGui::GetCurrentContext()) {
		ImGuiPlatformIO& io = ImGui::GetPlatformIO();
		if (io.Renderer_RenderWindow == RenderWindowHooked)
			io.Renderer_RenderWindow = g_backendRenderWindow;
		ImGui::DestroyPlatformWindows();
		platform::MakeGlCurrent();
	}
	g_enabled = false;
	g_backendRenderWindow = nullptr;
	g_captureFn = nullptr;
	g_captureUser = nullptr;
}

}  // namespace viewports
