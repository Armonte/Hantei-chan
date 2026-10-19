#ifndef PLATFORM_VIEWPORTS_H_GUARD
#define PLATFORM_VIEWPORTS_H_GUARD

// Detached windows through ImGui's multi-viewport support on the SDL3 + GL
// backends (PLAN.md 6 P0 step 5). The Win32 module this replaces
// (src/workspace_viewports.*) had to subclass every secondary HWND and drive
// one shared GL context across several DCs by hand; the SDL3 backend creates
// a native window plus a shared GL context per viewport itself, and routes
// SDL events to the viewport under the cursor. What is left for us is:
//
//  - keep the main context current after rendering the secondary windows,
//  - read each secondary window's back buffer when --capture-windows asks.
//
// Shortcuts in detached windows need no extra routing any more: the event
// pump hands every key to MainFrame::HandleKeys after ImGui has seen it,
// whichever window the key came from, which is what the old per-HWND
// subclass did.

namespace viewports {

// Call after ImGui_ImplSDL3_InitForOpenGL and ImGui_ImplOpenGL3_Init. When
// `enabled` is false everything below is a no-op, so a build without
// detachable windows takes the same path as before.
bool Initialize(bool enabled);

// ImGui::UpdatePlatformWindows + RenderPlatformWindowsDefault, then make the
// main window's context current again so the next frame starts where it
// expects. Called from MainFrame::Draw after the main frame is presented.
void RenderSecondaryWindows();

bool IsEnabled();

// --capture-windows: on the next RenderSecondaryWindows, read back every
// detached window's back buffer after its ImGui draw and before it is
// presented (top-down RGBA, alpha forced to 255 by the caller).
using CaptureFn = void (*)(int index, int width, int height, const unsigned char* rgba, void* user);
void RequestCapture(CaptureFn fn, void* user);

void Shutdown();

}  // namespace viewports

#endif /* PLATFORM_VIEWPORTS_H_GUARD */
