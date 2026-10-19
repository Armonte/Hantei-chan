// SDL3 multi-viewport support for detached windows (PLAN.md 6 P0 step 5).
//
// The backends do the work: imgui_impl_sdl3 creates a native window and a GL
// context that shares objects with the main one
// (SDL_GL_SHARE_WITH_CURRENT_CONTEXT), and imgui_impl_opengl3 renders each
// viewport's draw data into it. This module only re-establishes the main
// context afterwards and implements --capture-windows, which the old
// PrintWindow bridge used to do.

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
// The backend hooks we wrap (restored in Shutdown).
void (*g_backendRenderWindow)(ImGuiViewport*, void*) = nullptr;
void (*g_backendSwapBuffers)(ImGuiViewport*, void*) = nullptr;

// Reads the back buffer of the window currently current, flips it to
// top-down RGBA (GL reads bottom-up) and hands it to the capture callback.
void CaptureCurrentWindow(SDL_Window* window)
{
	if (!g_captureFn || !window) return;
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

// Wraps the backend's swap so the capture happens after the ImGui draw and
// before the present, exactly where the Win32 bridge read GL_BACK.
void SwapBuffersHooked(ImGuiViewport* viewport, void* arg)
{
	if (g_captureFn && viewport && viewport->PlatformHandle)
		CaptureCurrentWindow(SDL_GetWindowFromID((SDL_WindowID)(intptr_t)viewport->PlatformHandle));
	if (g_backendSwapBuffers) g_backendSwapBuffers(viewport, arg);
}

}  // namespace

bool Initialize(bool enabled)
{
	g_enabled = enabled;
	if (!enabled) return true;
	ImGuiPlatformIO& io = ImGui::GetPlatformIO();
	// Without both backends there is nothing to render a detached window
	// with; the caller then turns ViewportsEnable back off.
	if (!io.Platform_CreateWindow || !io.Platform_SwapBuffers || !io.Renderer_RenderWindow) {
		g_enabled = false;
		return false;
	}
	g_backendSwapBuffers = io.Platform_SwapBuffers;
	io.Platform_SwapBuffers = SwapBuffersHooked;
	return true;
}

void RenderSecondaryWindows()
{
	if (!g_enabled) return;
	ImGui::UpdatePlatformWindows();
	ImGui::RenderPlatformWindowsDefault();
	// The per-viewport render made that viewport's context current; the rest
	// of the frame (and the next one) expects the main window's.
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
		if (io.Platform_SwapBuffers == SwapBuffersHooked)
			io.Platform_SwapBuffers = g_backendSwapBuffers;
		ImGui::DestroyPlatformWindows();
	}
	g_enabled = false;
	g_backendSwapBuffers = nullptr;
	g_backendRenderWindow = nullptr;
	g_captureFn = nullptr;
	g_captureUser = nullptr;
}

}  // namespace viewports
