#ifndef STRING_UTILITES_H_
#define STRING_UTILITES_H_

#include "invalid_index.h"
#include "maybe_types.h"
#include "restrict.h"
#include "variable_arguments.h"

#include <stdbool.h>

typedef struct ConvertedInt
{
    char* after;
    int value;
    bool valid;
} ConvertedInt;

int copy_string(char* RESTRICT to, int to_size, const char* RESTRICT from);
int string_size(const char* string);
bool strings_match(const char* RESTRICT a, const char* RESTRICT b);
char* find_string(const char* RESTRICT a, const char* RESTRICT b);
int find_char(const char* s, char c);
int find_last_char(const char* s, char c);
bool string_starts_with(const char* RESTRICT a, const char* RESTRICT b);
bool string_ends_with(const char* RESTRICT a, const char* RESTRICT b);
int count_char_occurrences(const char* string, char c);
int count_substring_occurrences(const char* string, const char* pattern);

bool only_control_characters(const char* string);
void replace_chars(char* s, char original, char replacement);

MaybeInt string_to_int(const char* string);
ConvertedInt string_to_int_extra(const char* string, int base);
MaybeFloat string_to_float(const char* string);
MaybeDouble string_to_double(const char* string);

const char* bool_to_string(bool b);
int int_to_string(char* string, int size, int value);
void float_to_string(char* string, int size, float value, unsigned int precision);

void va_list_format_string(char* buffer, int buffer_size, const char* format, va_list* arguments);
void format_string(char* buffer, int buffer_size, const char* format, ...);

#endif // STRING_UTILITES_H_
