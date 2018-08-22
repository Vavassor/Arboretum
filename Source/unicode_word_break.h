// Word Breaking is finding where in a string the boundaries of words are. It's
// mainly used for Ctrl+Arrow cursor movement, which moves the cursor by whole
// words at a time.

#ifndef UNICODE_WORD_BREAK_H_
#define UNICODE_WORD_BREAK_H_

#include "memory.h"

#include <uchar.h>

// These values must match the generated tables in unicode_word_break.cpp.
typedef enum WordBreak
{
    WORD_BREAK_OTHER = 0,
    WORD_BREAK_CARRIAGE_RETURN = 1,
    WORD_BREAK_LINE_FEED = 2,
    WORD_BREAK_NEWLINE = 3,
    WORD_BREAK_EXTEND = 4,
    WORD_BREAK_ZERO_WIDTH_JOINER = 5,
    WORD_BREAK_REGIONAL_INDICATOR = 6,
    WORD_BREAK_FORMAT = 7,
    WORD_BREAK_KATAKANA = 8,
    WORD_BREAK_HEBREW_LETTER = 9,
    WORD_BREAK_A_LETTER = 10,
    WORD_BREAK_SINGLE_QUOTE = 11,
    WORD_BREAK_DOUBLE_QUOTE = 12,
    WORD_BREAK_MID_NUMBER_LETTER = 13,
    WORD_BREAK_MID_LETTER = 14,
    WORD_BREAK_MID_NUMBER = 15,
    WORD_BREAK_NUMERIC = 16,
    WORD_BREAK_EXTEND_NUMBER_LETTER = 17,
    WORD_BREAK_EMOJI_BASE = 18,
    WORD_BREAK_EMOJI_MODIFIER = 19,
    WORD_BREAK_GLUE_AFTER_ZWJ = 20, // Glue After Zero-Width Joiner
    WORD_BREAK_EMOJI_BASE_GAZ = 21, // Emoji Base Glue After Zero-Width Joiner
} WordBreak;

void set_word_break_tables(uint8_t* stage1, uint8_t* stage2);
void destroy_word_break_tables(Heap* heap);
WordBreak get_word_break(char32_t c);
int find_prior_beginning_of_word(const char* text, int start_index, Stack* stack);
int find_next_end_of_word(const char* text, int start_index, Stack* stack);
bool test_word_break(const char* text, int text_index, Stack* stack);

#endif // UNICODE_WORD_BREAK_H_
