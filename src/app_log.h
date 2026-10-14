#ifndef APP_LOG_H_GUARD
#define APP_LOG_H_GUARD
// Async application log (quill, third_party/quill). The frontend only copies the arguments into a lock-free queue;
// a backend thread formats them and appends to hanteichan.log next to the working directory. Use from any thread,
// including the render loop:
//   HC_LOG_WARN("Texture {} has no GL id", id);
// Format strings use {} placeholders. The backend starts on the first use; nothing is printed to stdout.
#include <quill/LogMacros.h>
#include <quill/Logger.h>

namespace applog {
quill::Logger *Get();
}

#define HC_LOG_INFO(fmt, ...) QUILL_LOG_INFO(::applog::Get(), fmt, ##__VA_ARGS__)
#define HC_LOG_WARN(fmt, ...) QUILL_LOG_WARNING(::applog::Get(), fmt, ##__VA_ARGS__)
#define HC_LOG_ERROR(fmt, ...) QUILL_LOG_ERROR(::applog::Get(), fmt, ##__VA_ARGS__)
// Once per call site (draw-path warnings that would otherwise repeat every frame).
#define HC_LOG_WARN_ONCE(fmt, ...) do { static bool hc_once_ = false; if (!hc_once_) { hc_once_ = true; HC_LOG_WARN(fmt, ##__VA_ARGS__); } } while (0)

#endif
