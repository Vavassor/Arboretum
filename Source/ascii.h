// ASCII stands for the American Standard Code for Information Interchange.
// Generally text is encoded in UTF-8, which ASCII is a subset of. In situations
// where only the ASCII characters are relevant, it's useful to have procedures
// for them.

#ifndef ASCII_H_
#define ASCII_H_

#include "restrict.h"

#include <stdbool.h>

int ascii_compare_alphabetic(const char* RESTRICT a, const char* RESTRICT b);
int ascii_digit_to_int(char c);
bool ascii_is_alphabetic(char c);
bool ascii_is_alphanumeric(char c);
bool ascii_is_lowercase(char c);
bool ascii_is_newline(char c);
bool ascii_is_numeric(char c);
bool ascii_is_space_or_tab(char c);
bool ascii_is_uppercase(char c);
bool ascii_is_whitespace(char c);
void ascii_to_lowercase(char* s);
char ascii_to_lowercase_char(char c);
void ascii_to_uppercase(char* s);
char ascii_to_uppercase_char(char c);

#endif // ASCII_H_
