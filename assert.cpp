#include "assert.h"

#include "filesystem.h"
#include "platform_definitions.h"
#include "string_utilities.h"

#include <signal.h>

#if defined(OS_LINUX)

static void break_debugger()
{
    raise(SIGTRAP);
}

#elif defined(OS_WINDOWS)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

static void break_debugger()
{
    DebugBreak();
}

#endif // defined(OS_WINDOWS)

void assert_fail(const char* expression, const char* file, int line)
{
    const int message_size = 128;
    char message[message_size];
    format_string(message, message_size, "Assertion failed: %s file %s line number %i\n", expression, file, line);
    write_to_standard_output(message, true);

    break_debugger();
    raise(SIGABRT);
}
