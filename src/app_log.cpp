#include "app_log.h"

#include <quill/Backend.h>
#include <quill/Frontend.h>
#include <quill/sinks/FileSink.h>
#include <quill/sinks/NullSink.h>

#include <chrono>
#include <cstdlib>
#include <memory>
#include <string>
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
		// FileSink canonicalises its path and throws when it can't: a working folder on a UNC path
		// (\\server\share, \\wsl.localhost\...) made the editor die at startup. Fall back to %TEMP%, then to no log.
		std::shared_ptr<quill::Sink> sink;
		try {
			sink = quill::Frontend::create_or_get_sink<quill::FileSink>("hanteichan.log", cfg);
		} catch (const std::exception &) {
			try {
				const char *tmp = std::getenv("TEMP");
				sink = quill::Frontend::create_or_get_sink<quill::FileSink>(std::string(tmp ? tmp : ".") + "\\hanteichan.log", cfg);
			} catch (const std::exception &) {
				sink = quill::Frontend::create_or_get_sink<quill::NullSink>("hc_null");
			}
		}
		quill::PatternFormatterOptions pattern{"[%(time)] [%(log_level)] %(message)", "%H:%M:%S.%Qms",
			quill::Timezone::LocalTime};
		logger = quill::Frontend::create_or_get_logger("hc", std::move(sink), pattern);
		if (const char *dbg = std::getenv("HC_LOG_DEBUG"); dbg && *dbg && *dbg != '0')
			logger->set_log_level(quill::LogLevel::Debug);
	});
	return logger;
}

void EnableDebug()
{
	Get()->set_log_level(quill::LogLevel::Debug);
}

} // namespace applog
