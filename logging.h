#ifndef LOGGING_H_
#define LOGGING_H_

typedef enum LogLevel
{
    LOG_LEVEL_ERROR,
    LOG_LEVEL_DEBUG,
} LogLevel;

void logging_add_message(LogLevel level, const char* format, ...);

#define LOG_ERROR(format, ...)\
    logging_add_message(LOG_LEVEL_ERROR, format, ##__VA_ARGS__)

#if defined(NDEBUG)
#define LOG_DEBUG(format, ...) // do nothing
#else
#define LOG_DEBUG(format, ...)\
    logging_add_message(LOG_LEVEL_DEBUG, format, ##__VA_ARGS__)
#endif

#endif // LOGGING_H_
