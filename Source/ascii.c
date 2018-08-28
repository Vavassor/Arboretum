#include "ascii.h"

#include "assert.h"

int ascii_compare_alphabetic(const char* RESTRICT a, const char* RESTRICT b)
{
    ASSERT(a);
    ASSERT(b);
    while(*a && (*a == *b))
    {
        a += 1;
        b += 1;
    }
    char c0 = ascii_to_uppercase_char(*a);
    char c1 = ascii_to_uppercase_char(*b);
    return c0 - c1;
}

int ascii_digit_to_int(char c)
{
    if     ('0' <= c && c <= '9') return c - '0';
    else if('a' <= c && c <= 'z') return c - 'a' + 10;
    else if('A' <= c && c <= 'Z') return c - 'A' + 10;
    return 0;
}

bool ascii_is_alphabetic(char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

bool ascii_is_alphanumeric(char c)
{
    return ascii_is_alphabetic(c) || ascii_is_numeric(c);
}

bool ascii_is_lowercase(char c)
{
    return c >= 'a' && c <= 'z';
}

bool ascii_is_newline(char c)
{
    return c >= '\n' && c <= '\r';
}

bool ascii_is_numeric(char c)
{
    return c >= '0' && c <= '9';
}

bool ascii_is_space_or_tab(char c)
{
    return c == ' ' || c == '\t';
}

bool ascii_is_uppercase(char c)
{
    return c >= 'A' && c <= 'Z';
}
bool ascii_is_whitespace(char c)
{
    return c == ' ' || c - 9 <= 5;
}

void ascii_to_lowercase(char* s)
{
    for(int i = 0; s[i]; i += 1)
    {
        s[i] = ascii_to_lowercase_char(s[i]);
    }
}

char ascii_to_lowercase_char(char c)
{
    if(ascii_is_uppercase(c))
    {
        return 'A' + (c - 'a');
    }
    else
    {
        return c;
    }
}

void ascii_to_uppercase(char* s)
{
    ASSERT(s);
    for(int i = 0; s[i]; i += 1)
    {
        s[i] = ascii_to_uppercase_char(s[i]);
    }
}

char ascii_to_uppercase_char(char c)
{
    if(ascii_is_lowercase(c))
    {
        return 'A' + (c - 'a');
    }
    else
    {
        return c;
    }
}
