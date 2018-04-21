#ifndef STRING_UTILITES_H_
#define STRING_UTILITES_H_

#include "invalid_index.h"
#include "restrict.h"
#include "variable_arguments.h"

int string_size(const char* string);
bool strings_match(const char* RESTRICT a, const char* RESTRICT b);
int copy_string(char* RESTRICT to, int to_size, const char* RESTRICT from);
char* find_string(const char* RESTRICT a, const char* RESTRICT b);
int find_char(const char* s, char c);
bool string_starts_with(const char* RESTRICT a, const char* RESTRICT b);
bool string_ends_with(const char* RESTRICT a, const char* RESTRICT b);
int count_char_occurrences(const char* string, char c);
int count_substring_occurrences(const char* string, const char* pattern);

int compare_alphabetic_ascii(const char* RESTRICT a, const char* RESTRICT b);
bool only_control_characters(const char* string);
void to_upper_case_ascii(char* string);
void replace_chars(char* s, char original, char replacement);

bool string_to_int(const char* string, int* value);
bool string_to_float(const char* string, float* value);
bool string_to_double(const char* string, double* value);

const char* bool_to_string(bool b);
int int_to_string(char* string, int size, int value);
void float_to_string(char* string, int size, float value, unsigned int precision);

void va_list_format_string(char* buffer, int buffer_size, const char* format, va_list* arguments);
void format_string(char* buffer, int buffer_size, const char* format, ...);

#endif // STRING_UTILITES_H_
