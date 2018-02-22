#include "assert.h"

#include "string_utilities.h"
#include "filesystem.h"

#include <signal.h>

void assert_fail(const char* expression, const char* file, int line)
{
    const int message_size = 128;
    char message[message_size];
    format_string(message, message_size, "Assertion failed: %s file %s line number %i\n", expression, file, line);
    write_to_standard_output(message, true);

    raise(SIGTRAP);
    raise(SIGABRT);
}
