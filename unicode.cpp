#include "unicode.h"

#include "assert.h"
#include "memory.h"

static bool is_heading_byte(char c)
{
    return (c & 0xc0) != 0x80;
}

int utf8_codepoint_count(const char* s)
{
    int count = 0;
    for(; *s; s += 1)
    {
        count += is_heading_byte(*s);
    }
    return count;
}

char32_t utf8_get_codepoint(const char* string, int* bytes_read)
{
    // Check if the next byte is ASCII.
    const char first_char = *string;
    if((first_char & 0x80) == 0)
    {
        *bytes_read = 1;
        return first_char;
    }

    // Otherwise, decode the multi-byte sequence.
    string += 1;
    int to_read = 1;
    char32_t mask = 0x40;
    char32_t ignore_mask = 0xffffff80;
    char32_t codepoint = first_char;
    while(first_char & mask)
    {
        codepoint = (codepoint << 6) + (*string++ & 0x3f);
        to_read += 1;
        ignore_mask |= mask;
        mask >>= 1;
    }
    ignore_mask |= mask;
    codepoint &= ~(ignore_mask << (6 * (to_read - 1)));

    *bytes_read = to_read;
    return codepoint;
}

int utf8_get_prior_codepoint(const char* string, int start, char32_t* result)
{
    for(int i = start; i >= 0; i -= 1)
    {
        if(is_heading_byte(string[i]))
        {
            int bytes_read;
            *result = utf8_get_codepoint(string, &bytes_read);
            return i;
        }
    }
    return -1;
}

int utf8_to_utf32(const char* from, int from_size, char32_t* to, int to_size)
{
    const char* current = from;
    const char* end = from + from_size;
    int i = 0;
    for(; i < to_size - 1 && current < end; i += 1)
    {
        int bytes_read;
        to[i] = utf8_get_codepoint(current, &bytes_read);
        current += bytes_read;
    }
    to[i] = U'\0';

    return i;
}
