#ifndef PLATFORM_MESSAGE_H_GUARD
#define PLATFORM_MESSAGE_H_GUARD

// A user-facing message, header only. Every message is logged through quill
// (app_log.h); a visible app also shows it in a box, through the SDL box the
// platform layer installs (platform.cpp, SDL_ShowSimpleMessageBox). A --headless
// run only logs it, because a modal box in a batch or ui-script run blocks
// forever with nobody to click it (PLAN.md 6 P0 step 7).
//
// Self-contained on purpose: src/shader.cpp and src/vao.cpp are also linked
// into the small tool targets, which do not build the platform layer (they all
// link app_log.cpp). With no box installed (those tools) the message is logged
// only.

#include "../app_log.h"

#ifdef _WIN32
// windows.h defines MessageBox as a macro (MessageBoxA/W). Our helper is a
// function of the same name, so the macro is removed here, after the header
// that defines it, whichever order the includer has.
#include <windows.h>
#undef MessageBox
#endif

namespace platform {

// --headless. An inline variable (C++17), so the header needs no .cpp and the
// tool targets that share shader.cpp / vao.cpp link nothing new.
inline bool g_platformHeadless = false;

// WM_CANCELMODE's job under SDL: a modal box or dialog is about to open, so any
// mouse gesture in progress (a view pan or draw drag, a box / position / UV
// drag) has to end now; the button release will go to the dialog. The app sets
// the callback (main.cpp); every modal entry point (MessageBox below, the file
// and folder dialogs) calls NotifyModalOpening first.
inline void (*g_onModalOpening)() = nullptr;
inline void NotifyModalOpening()
{
	if (g_onModalOpening) g_onModalOpening();
}

// Shows the box; set by platform::Init (the SDL message box). Severity 2 =
// error, 1 = warning, 0 = information.
inline void (*g_showMessageBox)(const char* title, const char* text, int severity) = nullptr;

inline void MessageBox(const char* title, const char* text, int severity)
{
	title = title ? title : "";
	text = text ? text : "";
	if (severity >= 2) HC_LOG_ERROR("{}: {}", title, text);
	else if (severity == 1) HC_LOG_WARN("{}: {}", title, text);
	else HC_LOG_INFO("{}: {}", title, text);
	if (g_platformHeadless || !g_showMessageBox) return;
	NotifyModalOpening();
	g_showMessageBox(title, text, severity);
}

}  // namespace platform

#endif /* PLATFORM_MESSAGE_H_GUARD */

