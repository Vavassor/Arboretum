#ifndef PLATFORM_DEFINITIONS_H_
#define PLATFORM_DEFINITIONS_H_

#if defined(__linux__)
#define OS_LINUX
#elif defined(_WIN32)
#define OS_WINDOWS
#else
#error Failed to figure out what operating system this is.
#endif

#if defined(_MSC_VER)
#define COMPILER_MSVC
#elif defined(__GNUC__)
#define COMPILER_GCC
#else
#error Failed to figure out the compiler used.
#endif

#if defined(__i386__) || defined(_M_IX86)
#define INSTRUCTION_SET_X86
#elif defined(__amd64__) || defined(_M_X64)
#define INSTRUCTION_SET_X64
#elif defined(__arm__) || defined(_M_ARM)
#define INSTRUCTION_SET_ARM
#else
#error Failed to figure out what instruction set the CPU on this computer uses.
#endif

#endif // PLATFORM_DEFINITIONS_H_
