#include "workspace_viewports.h"

#include "context_gl.h"

#include <commctrl.h>
#include <glad/glad.h>
#include <imgui.h>
#include <algorithm>
#include <vector>

namespace
{
	struct ViewportDevice
	{
		HWND window = nullptr;
		HDC deviceContext = nullptr;
	};

	ContextGl* g_mainContext = nullptr;
	bool g_enabled = false;
	WorkspaceViewports::KeyHook g_keyHook = nullptr;
	void (*g_backendCreateWindow)(ImGuiViewport*) = nullptr;
	constexpr UINT_PTR kSubclassId = 0x48414E54; // 'HANT'
	WorkspaceViewports::CaptureFn g_captureFn = nullptr;
	void* g_captureUser = nullptr;
	int g_captureIndex = 0;

	bool ConfigureDeviceContext(HWND window, HDC deviceContext)
	{
		if (!g_mainContext || !window || !deviceContext)
			return false;

		// A single WGL context may render to another DC only when both DCs use
		// the same pixel format. Copy the primary window's chosen descriptor
		// instead of independently choosing a merely similar format.
		const int pixelFormat = GetPixelFormat(g_mainContext->dc);
		PIXELFORMATDESCRIPTOR descriptor{};
		if (pixelFormat == 0 ||
			DescribePixelFormat(g_mainContext->dc, pixelFormat, sizeof(descriptor), &descriptor) == 0)
			return false;

		return SetPixelFormat(deviceContext, pixelFormat, &descriptor) == TRUE;
	}

	// Keyboard shortcuts for detached windows. The ImGui backend's own window
	// procedure runs first (DefSubclassProc) so ImGui's IO sees every key.
	LRESULT CALLBACK DetachedWindowSubclass(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
		UINT_PTR, DWORD_PTR)
	{
		if (msg == WM_NCDESTROY) {
			RemoveWindowSubclass(hwnd, DetachedWindowSubclass, kSubclassId);
			return DefSubclassProc(hwnd, msg, wParam, lParam);
		}
		// A WM_CHAR the key hook swallows (the key already ran a shortcut from
		// inside a text field, e.g. Num* / Num/ keyframe step) never reaches ImGui.
		if (msg == WM_CHAR && g_keyHook && g_keyHook(hwnd, msg, wParam, lParam))
			return 0;
		const LRESULT result = DefSubclassProc(hwnd, msg, wParam, lParam);
		if ((msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN) && g_keyHook && g_keyHook(hwnd, msg, wParam, lParam))
			return 0;
		return result;
	}

	void CreatePlatformWindowHooked(ImGuiViewport* viewport)
	{
		if (g_backendCreateWindow)
			g_backendCreateWindow(viewport);
		HWND hwnd = viewport ? static_cast<HWND>(viewport->PlatformHandle) : nullptr;
		if (hwnd)
			SetWindowSubclass(hwnd, DetachedWindowSubclass, kSubclassId, 0);
	}

	void CreateRendererWindow(ImGuiViewport* viewport)
	{
		if (!viewport || viewport->RendererUserData != nullptr)
			return;

		auto* device = IM_NEW(ViewportDevice);
		device->window = static_cast<HWND>(viewport->PlatformHandle);
		device->deviceContext = device->window ? GetDC(device->window) : nullptr;
		if (!device->deviceContext || !ConfigureDeviceContext(device->window, device->deviceContext))
		{
			if (device->deviceContext)
				ReleaseDC(device->window, device->deviceContext);
			IM_DELETE(device);
			return;
		}

		viewport->RendererUserData = device;
	}

	void DestroyRendererWindow(ImGuiViewport* viewport)
	{
		if (!viewport || !viewport->RendererUserData)
			return;

		auto* device = static_cast<ViewportDevice*>(viewport->RendererUserData);
		if (wglGetCurrentDC() == device->deviceContext && g_mainContext)
			g_mainContext->MakeCurrent();
		if (device->deviceContext && device->window)
			ReleaseDC(device->window, device->deviceContext);
		IM_DELETE(device);
		viewport->RendererUserData = nullptr;
	}

	void MakeRendererWindowCurrent(ImGuiViewport* viewport, void*)
	{
		if (!viewport || !viewport->RendererUserData || !g_mainContext)
			return;
		auto* device = static_cast<ViewportDevice*>(viewport->RendererUserData);
		wglMakeCurrent(device->deviceContext, g_mainContext->GetRenderContext());
	}

	void SwapRendererWindow(ImGuiViewport* viewport, void*)
	{
		if (!viewport || !viewport->RendererUserData)
			return;
		auto* device = static_cast<ViewportDevice*>(viewport->RendererUserData);
		if (g_captureFn) {
			RECT rc{};
			GetClientRect(device->window, &rc);
			const int w = rc.right - rc.left, h = rc.bottom - rc.top;
			if (w > 0 && h > 0) {
				std::vector<unsigned char> up((size_t)w * h * 4), down((size_t)w * h * 4);
				glReadBuffer(GL_BACK);
				glPixelStorei(GL_PACK_ALIGNMENT, 1);
				glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, up.data());
				for (int y = 0; y < h; ++y)
					std::copy_n(up.data() + (size_t)(h - 1 - y) * w * 4, (size_t)w * 4, down.data() + (size_t)y * w * 4);
				g_captureFn(++g_captureIndex, w, h, down.data(), g_captureUser);
			}
		}
		SwapBuffers(device->deviceContext);
	}
}

bool WorkspaceViewports::Initialize(ContextGl& context, HWND mainWindow, bool enabled, KeyHook keyHook)
{
	g_mainContext = &context;
	g_enabled = enabled;
	g_keyHook = keyHook;
	(void)mainWindow;
	if (!enabled)
		return true;

	ImGuiPlatformIO& platform = ImGui::GetPlatformIO();
	if (platform.Renderer_CreateWindow || platform.Renderer_DestroyWindow ||
		platform.Renderer_SwapBuffers || platform.Platform_RenderWindow ||
		!platform.Platform_CreateWindow)
	{
		g_enabled = false;
		return false;
	}

	g_backendCreateWindow = platform.Platform_CreateWindow;
	platform.Platform_CreateWindow = CreatePlatformWindowHooked;
	platform.Renderer_CreateWindow = CreateRendererWindow;
	platform.Renderer_DestroyWindow = DestroyRendererWindow;
	platform.Renderer_SwapBuffers = SwapRendererWindow;
	platform.Platform_RenderWindow = MakeRendererWindowCurrent;
	return true;
}

void WorkspaceViewports::RenderSecondaryWindows()
{
	if (!g_enabled || !g_mainContext)
		return;

	ImGui::UpdatePlatformWindows();
	ImGui::RenderPlatformWindowsDefault();
	g_mainContext->MakeCurrent();
	g_captureFn = nullptr;
	g_captureUser = nullptr;
}

void WorkspaceViewports::RequestCapture(CaptureFn fn, void* user)
{
	g_captureFn = fn;
	g_captureUser = user;
	g_captureIndex = 0;
}

void WorkspaceViewports::Shutdown()
{
	if (g_enabled && ImGui::GetCurrentContext())
	{
		ImGui::DestroyPlatformWindows();
		ImGuiPlatformIO& platform = ImGui::GetPlatformIO();
		if (platform.Platform_CreateWindow == CreatePlatformWindowHooked)
			platform.Platform_CreateWindow = g_backendCreateWindow;
		platform.Renderer_CreateWindow = nullptr;
		platform.Renderer_DestroyWindow = nullptr;
		platform.Renderer_SwapBuffers = nullptr;
		platform.Platform_RenderWindow = nullptr;
	}
	if (g_mainContext)
		g_mainContext->MakeCurrent();
	g_enabled = false;
	g_mainContext = nullptr;
	g_keyHook = nullptr;
	g_backendCreateWindow = nullptr;
}

bool WorkspaceViewports::IsEnabled()
{
	return g_enabled;
}
