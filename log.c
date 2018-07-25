#include "log.h"

#include "variable_arguments.h"
#include "string_utilities.h"
#include "filesystem.h"

#define BUFFER_SIZE 256

void log_error(Log* logger, const char* format, ...)
{
    va_list arguments;
    va_start(arguments, format);
    char buffer[BUFFER_SIZE];
    va_list_format_string(buffer, BUFFER_SIZE, format, &arguments);
    va_end(arguments);
    bool output_to_stderr = true;
    write_to_standard_output(buffer, output_to_stderr);
    write_to_standard_output("\n", output_to_stderr);
}

void log_debug(Log* logger, const char* format, ...)
{
    va_list arguments;
    va_start(arguments, format);
    char buffer[BUFFER_SIZE];
    va_list_format_string(buffer, BUFFER_SIZE, format, &arguments);
    va_end(arguments);
    bool output_to_stderr = false;
    write_to_standard_output(buffer, output_to_stderr);
    write_to_standard_output("\n", output_to_stderr);
}
