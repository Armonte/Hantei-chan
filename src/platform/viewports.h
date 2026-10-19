#ifndef PLATFORM_VIEWPORTS_H_GUARD
#define PLATFORM_VIEWPORTS_H_GUARD

// Detached windows through ImGui's multi-viewport support on the SDL3 + GL3
// backends (PLAN.md 6 P0 step 5). The Win32 module this replaces
// (src/workspace_viewports.*) subclassed every secondary HWND for its keys and
// drove one GL context across several DCs by hand. Now imgui_impl_sdl3 creates
// a native window plus a GL context that shares objects with the main one
// (SDL_GL_SHARE_WITH_CURRENT_CONTEXT) per viewport, and SDL tags every event
// with the window it came from. What is left here:
//
//  - render the secondary windows and make the main context current again,
//  - read each secondary window's frame when --capture-windows asks.
//
// Keys typed in a detached window reach MainFrame::HandleKeys from the app's
// event switch (main.cpp), which accepts a key when its window is one of the
// app's viewports; ImGui's own focus decides which view the shortcut acts on.

namespace viewports {

// Call after ImGui_ImplSDL3_InitForOpenGL and ImGui_ImplOpenGL3_Init. When
// `enabled` is false everything below is a no-op. Returns false when the
// backends registered no multi-viewport hooks; the caller then leaves
// ImGuiConfigFlags_ViewportsEnable off (tabs detach inside the main window).
bool Initialize(bool enabled);

// ImGui::UpdatePlatformWindows + RenderPlatformWindowsDefault, then make the
// main window's context current again so the rest of the frame (the main
// window's present) and the next frame start where they expect.
void RenderSecondaryWindows();

bool IsEnabled();

// --capture-windows: on the next RenderSecondaryWindows, read back every
// detached window's frame right after the renderer drew it, before it is
// presented (top-down RGBA; the caller forces alpha to 255). Index 1 is the
// first secondary viewport in ImGui's order.
using CaptureFn = void (*)(int index, int width, int height, const unsigned char* rgba, void* user);
void RequestCapture(CaptureFn fn, void* user);

void Shutdown();

}  // namespace viewports

#endif /* PLATFORM_VIEWPORTS_H_GUARD */
