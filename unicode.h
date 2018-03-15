#ifndef UNICODE_H_
#define UNICODE_H_

#include "invalid_index.h"

int utf8_codepoint_count(const char* s);
char32_t utf8_get_codepoint(const char* string, int* bytes_read);
int utf8_get_prior_codepoint(const char* string, int start, char32_t* result);
int utf8_skip_to_prior_codepoint(const char* string, int start);
int utf8_get_next_codepoint(const char* string, int size, int start, char32_t* result);
int utf8_skip_to_next_codepoint(const char* string, int size, int start);
int utf8_to_utf32(const char* from, int from_size, char32_t* to, int to_size);

bool is_whitespace(char32_t codepoint);
bool is_newline(char32_t codepoint);
bool is_default_ignorable(char32_t codepoint);

#endif // UNICODE_H_
