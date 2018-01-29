#ifndef LOGGING_H_
#define LOGGING_H_

namespace logging {

enum class Level
{
	Error,
	Debug,
};

void add_message(Level level, const char* format, ...);

} // namespace logging

#define LOG_ERROR(format, ...)\
	logging::add_message(logging::Level::Error, format, ##__VA_ARGS__)

#if defined(NDEBUG)
#define LOG_DEBUG(format, ...) // do nothing
#else
#define LOG_DEBUG(format, ...)\
	logging::add_message(logging::Level::Debug, format, ##__VA_ARGS__)
#endif

#endif // LOGGING_H_
