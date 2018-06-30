#include "logging.h"

#include "variable_arguments.h"
#include "string_utilities.h"
#include "filesystem.h"

void logging_add_message(LogLevel level, const char* format, ...)
{
    va_list arguments;
    va_start(arguments, format);
    bool output_to_stderr;
    switch(level)
    {
        case LOG_LEVEL_ERROR:
        {
            output_to_stderr = true;
            break;
        }
        case LOG_LEVEL_DEBUG:
        {
            output_to_stderr = false;
            break;
        }
    }
    const int buffer_size = 256;
    char buffer[256];
    va_list_format_string(buffer, buffer_size, format, &arguments);
    va_end(arguments);
    write_to_standard_output(buffer, output_to_stderr);
    write_to_standard_output("\n", output_to_stderr);
}
