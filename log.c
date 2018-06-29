#include "log.h"

#include <stdio.h>
#include <stdarg.h>

void log_error(Log* log, const char* format, ...)
{
    FILE* stream = stderr;
    va_list arguments;
    va_start(arguments, format);
    vfprintf(stream, format, arguments);
    fputc('\n', stream);
    va_end(arguments);
}
