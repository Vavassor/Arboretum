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

#define MESSAGE_SIZE 256

void assert_fail(const char* expression, const char* file, int line)
{
    char message[MESSAGE_SIZE];
    format_string(message, MESSAGE_SIZE, "Assertion failed: %s file %s line number %i\n", expression, file, line);
    write_to_standard_output(message, true);

    break_debugger();
    raise(SIGABRT);
}
