#ifndef WIDE_CHAR_H_
#define WIDE_CHAR_H_

struct Heap;
struct Stack;

char* wide_char_to_utf8(const wchar_t* string, Heap* heap);
char* wide_char_to_utf8(const wchar_t* string, Stack* stack);
wchar_t* utf8_to_wide_char(const char* string, Heap* heap);
wchar_t* utf8_to_wide_char(const char* string, Stack* stack);

#endif // WIDE_CHAR_H_
