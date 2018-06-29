#ifndef LOG_H_
#define LOG_H_

typedef struct Log
{
    int placeholder;
} Log;

void log_error(Log* log, const char* format, ...);

#endif // LOG_H_
