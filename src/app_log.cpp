#include "app_log.h"

#include <quill/Backend.h>
#include <quill/Frontend.h>
#include <quill/sinks/FileSink.h>

#include <chrono>
#include <mutex>

namespace applog {

quill::Logger *Get()
{
	static quill::Logger *logger = nullptr;
	static std::once_flag once;
	std::call_once(once, [] {
		quill::BackendOptions opts;
		opts.thread_name = "hc_log_backend";
		// Sleep between queue polls instead of spinning; log latency does not matter here.
		opts.sleep_duration = std::chrono::milliseconds(2);
		opts.enable_yield_when_idle = true;
		quill::Backend::start(opts);

		quill::FileSinkConfig cfg;
		cfg.set_open_mode('w');
		auto sink = quill::Frontend::create_or_get_sink<quill::FileSink>("hanteichan.log", cfg);
		quill::PatternFormatterOptions pattern{"[%(time)] [%(log_level)] %(message)", "%H:%M:%S.%Qms",
			quill::Timezone::LocalTime};
		logger = quill::Frontend::create_or_get_logger("hc", std::move(sink), pattern);
	});
	return logger;
}

} // namespace applog
