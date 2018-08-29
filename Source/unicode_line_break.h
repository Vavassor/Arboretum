// Line Breaking consists of finding where in a string it is legal to cut off
// and start a new line. It's used in word wrapping to divide the text into
// runs that it can lay out, without cutting between characters that should be
// written together.

#ifndef UNICODE_LINE_BREAK_H_
#define UNICODE_LINE_BREAK_H_

#include "memory.h"

#include <uchar.h>

typedef enum LineBreak
{
    LINE_BREAK_AMBIGUOUS = 0, // AI
    LINE_BREAK_ORDINARY_ALPHABETIC_OR_SYMBOL = 1, // AL
    LINE_BREAK_BREAK_OPPORTUNITY_BEFORE_AND_AFTER = 2, // B2
    LINE_BREAK_BREAK_AFTER = 3, // BA
    LINE_BREAK_BREAK_BEFORE = 4, // BB
    LINE_BREAK_MANDATORY_BREAK = 5, // BK
    LINE_BREAK_CONTINGENT_BREAK_OPPORTUNITY = 6, // CB
    LINE_BREAK_CONDITIONAL_JAPANESE_STARTER = 7, // CJ
    LINE_BREAK_CLOSE_PUNCTUATION = 8, // CL
    LINE_BREAK_COMBINING_MARK = 9, // CM
    LINE_BREAK_CLOSING_PARENTHESIS = 10, // CP
    LINE_BREAK_CARRIAGE_RETURN = 11, // CR
    LINE_BREAK_EMOJI_BASE = 12, // EB
    LINE_BREAK_EMOJI_MODIFIER = 13, // EM
    LINE_BREAK_EXCLAMATION_INTERROGATION = 14, // EX
    LINE_BREAK_NON_BREAKING = 15, // GL
    LINE_BREAK_HANGUL_LV_SYLLABLE = 16, // H2
    LINE_BREAK_HANGUL_LVT_SYLLABLE = 17, // H3
    LINE_BREAK_HEBREW_LETTER = 18, // HL
    LINE_BREAK_HYPHEN = 19, // HY
    LINE_BREAK_IDEOGRAPHIC = 20, // ID
    LINE_BREAK_INSEPARABLE_CHARACTERS = 21, // IN
    LINE_BREAK_INFIX_NUMERIC_SEPARATOR = 22, // IS
    LINE_BREAK_HANGUL_L_JAMO = 23, // JL
    LINE_BREAK_HANGUL_T_JAMO = 24, // JT
    LINE_BREAK_HANGUL_V_JAMO = 25, // JV
    LINE_BREAK_LINE_FEED = 26, // LF
    LINE_BREAK_NEXT_LINE = 27, // NL
    LINE_BREAK_NONSTARTERS = 28, // NS
    LINE_BREAK_NUMERIC = 29, // NU
    LINE_BREAK_OPEN_PUNCTUATION = 30, // OP
    LINE_BREAK_POSTFIX_NUMERIC = 31, // PO
    LINE_BREAK_PREFIX_NUMERIC = 32, // PR
    LINE_BREAK_QUOTATION = 33, // QU
    LINE_BREAK_REGIONAL_INDICATOR = 34, // RI
    LINE_BREAK_COMPLEX_CONTEXT_DEPENDENT = 35, // SA
    LINE_BREAK_SURROGATE = 36, // SG
    LINE_BREAK_SPACE = 37, // SP
    LINE_BREAK_SYMBOLS = 38, // SY
    LINE_BREAK_WORD_JOINER = 39, // WJ
    LINE_BREAK_UNKNOWN = 40, // XX
    LINE_BREAK_ZERO_WIDTH_SPACE = 41, // ZW
    LINE_BREAK_ZERO_WIDTH_JOINER = 42, // ZWJ
} LineBreak;

typedef enum LineBreakCategory
{
    LINE_BREAK_CATEGORY_MANDATORY,
    LINE_BREAK_CATEGORY_OPTIONAL,
    LINE_BREAK_CATEGORY_PROHIBITED,
} LineBreakCategory;

void set_line_break_tables(uint8_t* stage1, uint8_t* stage2);
void destroy_line_break_tables(Heap* heap);
LineBreak get_line_break(char32_t c);
int find_next_line_break(const char* text, int start_index, bool* mandatory, Stack* stack);
int find_next_mandatory_line_break(const char* text, int start_index, Stack* stack);
LineBreakCategory test_line_break(const char* text, int start_index, Stack* stack);

#endif // UNICODE_LINE_BREAK_H_
