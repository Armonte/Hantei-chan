#ifndef PLATFORM_MESSAGE_H_GUARD
#define PLATFORM_MESSAGE_H_GUARD

// A user-facing message, header only. A visible run shows a real box; a
// --headless run only logs it, because a modal box in a batch or ui-script run
// blocks forever with nobody to click it (PLAN.md 6 P0 step 7).
//
// Self-contained on purpose: src/shader.cpp and src/vao.cpp are also linked
// into the small tool targets, which do not build the platform layer.

#include <cstdio>

#ifdef _WIN32
// windows.h defines MessageBox as a macro (MessageBoxA/W). Our helper is a
// function of the same name, so the macro is removed here, after the header
// that needs it. The real Win32 call is spelled ::MessageBoxA below.
#include <windows.h>
#undef MessageBox
#endif

namespace platform {

// --headless. An inline variable (C++17), so the header needs no .cpp and the
// tool targets that share shader.cpp / vao.cpp link nothing new.
inline bool g_platformHeadless = false;

inline void MessageBox(const char* title, const char* text, int severity)
{
	if (g_platformHeadless) {
		const char* level = severity >= 2 ? "ERROR" : (severity >= 1 ? "WARN" : "INFO");
		std::fprintf(stderr, "[%s] %s: %s%c", level, title ? title : "", text ? text : "", 10);
		return;
	}
#ifdef _WIN32
	// The same call the code used before this phase.
	const unsigned type = severity >= 2 ? 0x10u : (severity >= 1 ? 0x30u : 0x40u);
	::MessageBoxA(nullptr, text ? text : "", title ? title : "", type);
#else
	const char* level = severity >= 2 ? "ERROR" : (severity >= 1 ? "WARN" : "INFO");
	std::fprintf(stderr, "[%s] %s: %s%c", level, title ? title : "", text ? text : "", 10);
#endif
}

}  // namespace platform

#endif /* PLATFORM_MESSAGE_H_GUARD */

