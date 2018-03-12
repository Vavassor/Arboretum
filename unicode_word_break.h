#ifndef UNICODE_WORD_BREAK_H_
#define UNICODE_WORD_BREAK_H_

// These values must match the generated tables in unicode_word_break.cpp.
enum class WordBreak
{
    Other = 0,
    Carriage_Return = 1,
    Line_Feed = 2,
    Newline = 3,
    Extend = 4,
    Zero_Width_Joiner = 5,
    Regional_Indicator = 6,
    Format = 7,
    Katakana = 8,
    Hebrew_Letter = 9,
    A_Letter = 10,
    Single_Quote = 11,
    Double_Quote = 12,
    Mid_Number_Letter = 13,
    Mid_Letter = 14,
    Mid_Number = 15,
    Numeric = 16,
    Extend_Number_Letter = 17,
    Emoji_Base = 18,
    Emoji_Modifier = 19,
    Glue_After_ZWJ = 20, // Glue After Zero-Width Joiner
    Emoji_Base_GAZ = 21, // Emoji Base Glue After Zero-Width Joiner
};

struct Stack;

WordBreak get_word_break(char32_t c);
int find_prior_beginning_of_word(const char* text, int start_index, Stack* stack);
int find_next_end_of_word(const char* text, int start_index, Stack* stack);
bool test_word_break(const char* text, int text_index, Stack* stack);

#endif // UNICODE_WORD_BREAK_H_
