#include "wide_char.h"

#include "platform_definitions.h"

#if defined(OS_WINDOWS)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

char* wide_char_to_utf8(const wchar_t* string, Heap* heap)
{
    int bytes = WideCharToMultiByte(CP_UTF8, 0, string, -1, NULL, 0, NULL, NULL);
    char* result = HEAP_ALLOCATE(heap, char, bytes);
    bytes = WideCharToMultiByte(CP_UTF8, 0, string, -1, result, bytes, NULL, NULL);
    if(bytes == 0)
    {
        SAFE_HEAP_DEALLOCATE(heap, result);
        return NULL;
    }
    return result;
}

char* wide_char_to_utf8_stack(const wchar_t* string, Stack* stack)
{
    int bytes = WideCharToMultiByte(CP_UTF8, 0, string, -1, NULL, 0, NULL, NULL);
    char* result = STACK_ALLOCATE(stack, char, bytes);
    bytes = WideCharToMultiByte(CP_UTF8, 0, string, -1, result, bytes, NULL, NULL);
    if(bytes == 0)
    {
        STACK_DEALLOCATE(stack, result);
        return NULL;
    }
    return result;
}

wchar_t* utf8_to_wide_char(const char* string, Heap* heap)
{
    int count = MultiByteToWideChar(CP_UTF8, 0, string, -1, NULL, 0);
    wchar_t* result = HEAP_ALLOCATE(heap, wchar_t, count);
    count = MultiByteToWideChar(CP_UTF8, 0, string, -1, result, count);
    if(count == 0)
    {
        SAFE_HEAP_DEALLOCATE(heap, result);
        return NULL;
    }
    return result;
}

wchar_t* utf8_to_wide_char_stack(const char* string, Stack* stack)
{
    int count = MultiByteToWideChar(CP_UTF8, 0, string, -1, NULL, 0);
    wchar_t* result = STACK_ALLOCATE(stack, wchar_t, count);
    count = MultiByteToWideChar(CP_UTF8, 0, string, -1, result, count);
    if(count == 0)
    {
        if(result)
        {
            STACK_DEALLOCATE(stack, result);
        }
        return NULL;
    }
    return result;
}

#endif // defined(OS_WINDOWS)
