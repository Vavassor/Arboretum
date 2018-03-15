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
            *result = utf8_get_codepoint(&string[i], &bytes_read);
            return i;
        }
    }
    return invalid_index;
}

int utf8_skip_to_prior_codepoint(const char* string, int start)
{
    char32_t dummy;
    return utf8_get_prior_codepoint(string, start, &dummy);
}

int utf8_get_next_codepoint(const char* string, int size, int start, char32_t* result)
{
    for(int i = start; i < size; i += 1)
    {
        if(is_heading_byte(string[i]))
        {
            int bytes_read;
            *result = utf8_get_codepoint(&string[i], &bytes_read);
            return i;
        }
    }
    return invalid_index;
}

int utf8_skip_to_next_codepoint(const char* string, int size, int start)
{
    char32_t dummy;
    return utf8_get_next_codepoint(string, size, start, &dummy);
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

// This is based on the White_Space property in Unicode Standard Annex #44
// revision 20 for Unicode version 10.0.0.
// http://www.unicode.org/reports/tr44/tr44-20.html
//
// The codepoint ranges are pulled directly from the data file:
// https://unicode.org/Public/UNIDATA/PropList.txt
bool is_whitespace(char32_t codepoint)
{
    return (codepoint >= U'\u0009' && codepoint <= U'\u000d') // <control-0009>..<control-000D>
        || codepoint == U'\u0020'                             // SPACE
        || codepoint == U'\u0085'                             // <control-0085>
        || codepoint == U'\u00a0'                             // NO-BREAK SPACE
        || codepoint == U'\u1680'                             // OGHAM SPACE MARK
        || (codepoint >= U'\u2000' && codepoint <= U'\u200a') // EN QUAD..HAIR SPACE
        || codepoint == U'\u2028'                             // LINE SEPARATOR
        || codepoint == U'\u2029'                             // PARAGRAPH SEPARATOR
        || codepoint == U'\u202f'                             // NARROW NO-BREAK SPACE
        || codepoint == U'\u205f'                             // MEDIUM MATHEMATICAL SPACE
        || codepoint == U'\u3000';                            // IDEOGRAPHIC SPACE
}

bool is_newline(char32_t codepoint)
{
    return (codepoint >= U'\u000a' && codepoint <= U'\u000d') // <control-000a>..<control-000d>
        || codepoint == U'\u0085'                             // <control-0085>
        || codepoint == U'\u2028'                             // LINE SEPARATOR
        || codepoint == U'\u2029';                            // PARAGRAPH SEPARATOR
}

// This is based on the Default_Ignorable_Code_Point property in Unicode
// Standard Annex #44 revision 20 for Unicode version 10.0.0.
// http://www.unicode.org/reports/tr44/tr44-20.html
//
// The codepoint ranges are pulled directly from the data file:
// https://unicode.org/Public/UNIDATA/DerivedCoreProperties.txt
bool is_default_ignorable(char32_t codepoint)
{
    return codepoint == U'\u00ad'                                      // SOFT HYPHEN
        || codepoint == U'\u034f'                                      // COMBINING GRAPHEME JOINER
        || codepoint == U'\u061c'                                      // ARABIC LETTER MARK
        || (codepoint >= U'\u115f' && codepoint <= U'\u1160')          // HANGUL CHOSEONG FILLER..HANGUL JUNGSEONG FILLER
        || (codepoint >= U'\u17b4' && codepoint <= U'\u17b5')          // KHMER VOWEL INHERENT AQ..KHMER VOWEL INHERENT AA
        || (codepoint >= U'\u180b' && codepoint <= U'\u180d')          // MONGOLIAN FREE VARIATION SELECTOR ONE..MONGOLIAN FREE VARIATION SELECTOR THREE
        || codepoint == U'\u180e'                                      // MONGOLIAN VOWEL SEPARATOR
        || (codepoint >= U'\u200b' && codepoint <= U'\u200f')          // ZERO WIDTH SPACE..RIGHT-TO-LEFT MARK
        || (codepoint >= U'\u202a' && codepoint <= U'\u202e')          // LEFT-TO-RIGHT EMBEDDING..RIGHT-TO-LEFT OVERRIDE
        || (codepoint >= U'\u2060' && codepoint <= U'\u2064')          // WORD JOINER..INVISIBLE PLUS
        || codepoint == U'\u2065'                                      // <reserved-2065>
        || (codepoint >= U'\u2066' && codepoint <= U'\u206f')          // LEFT-TO-RIGHT ISOLATE..NOMINAL DIGIT SHAPES
        || codepoint == U'\u3164'                                      // HANGUL FILLER
        || (codepoint >= U'\ufe00' && codepoint <= U'\ufe0f')          // VARIATION SELECTOR-1..VARIATION SELECTOR-16
        || codepoint == U'\ufeff'                                      // ZERO WIDTH NO-BREAK SPACE
        || codepoint == U'\uffa0'                                      // HALFWIDTH HANGUL FILLER
        || (codepoint >= U'\ufff0' && codepoint <= U'\ufff8')          // <reserved-FFF0>..<reserved-FFF8>
        || (codepoint >= U'\U0001bca0' && codepoint <= U'\U0001bca3')  // SHORTHAND FORMAT LETTER OVERLAP..SHORTHAND FORMAT UP STEP
        || (codepoint >= U'\U000e0000' && codepoint <= U'\U000e0fff'); // a bunch of stuff, mostly reserved
}
