#ifndef TLOG_H_GUARD
#define TLOG_H_GUARD
// Stage timing for the archive browser (HC_TIMING=1 or a han2_settings.ini [browser] Timing=1): one line per stage appended to hc_timing.log, ms from process start.
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <mutex>
namespace tlog {
using Clock = std::chrono::steady_clock;
inline Clock::time_point Now() { return Clock::now(); }
inline double Ms(Clock::time_point a, Clock::time_point b) { return std::chrono::duration<double, std::milli>(b - a).count(); }
inline bool On() { static const bool on = getenv("HC_TIMING") != nullptr; return on; }
inline void Log(const char *fmt, ...)
{
	if (!On()) return;
	static std::mutex mx; static const Clock::time_point t0 = Clock::now();
	char b[400]; va_list ap; va_start(ap, fmt); vsnprintf(b, sizeof b, fmt, ap); va_end(ap);
	std::lock_guard<std::mutex> lk(mx);
	if (FILE *f = fopen("hc_timing.log", "a")) { fprintf(f, "%9.1f %s\n", Ms(t0, Clock::now()), b); fclose(f); }
}
struct Scope { const char *what; Clock::time_point t; Scope(const char *w) : what(w), t(Now()) {} ~Scope() { Log("%s: %.1f ms", what, Ms(t, Now())); } };
}
#endif
