#ifndef UNICODE_LINE_BREAK_H_
#define UNICODE_LINE_BREAK_H_

enum class LineBreak
{
    Ambiguous = 0, // AI
    Ordinary_Alphabetic_Or_Symbol = 1, // AL
    Break_Opportunity_Before_and_After = 2, // B2
    Break_After = 3, // BA
    Break_Before = 4, // BB
    Mandatory_Break = 5, // BK
    Contingent_Break_Opportunity = 6, // CB
    Conditional_Japanese_Starter = 7, // CJ
    Close_Punctuation = 8, // CL
    Combining_Mark = 9, // CM
    Closing_Parenthesis = 10, // CP
    Carriage_Return = 11, // CR
    Emoji_Base = 12, // EB
    Emoji_Modifier = 13, // EM
    Exclamation_Interrogation = 14, // EX
    Non_Breaking = 15, // GL
    Hangul_LV_Syllable = 16, // H2
    Hangul_LVT_Syllable = 17, // H3
    Hebrew_Letter = 18, // HL
    Hyphen = 19, // HY
    Ideographic = 20, // ID
    Inseparable_Characters = 21, // IN
    Infix_Numeric_Separator = 22, // IS
    Hangul_L_Jamo = 23, // JL
    Hangul_T_Jamo = 24, // JT
    Hangul_V_Jamo = 25, // JV
    Line_Feed = 26, // LF
    Next_Line = 27, // NL
    Nonstarters = 28, // NS
    Numeric = 29, // NU
    Open_Punctuation = 30, // OP
    Postfix_Numeric = 31, // PO
    Prefix_Numeric = 32, // PR
    Quotation = 33, // QU
    Regional_Indicator = 34, // RI
    Complex_Context_Dependent = 35, // SA
    Surrogate = 36, // SG
    Space = 37, // SP
    Symbols = 38, // SY
    Word_Joiner = 39, // WJ
    Unknown = 40, // XX
    Zero_Width_Space = 41, // ZW
    Zero_Width_Joiner = 42, // ZWJ
};

enum class LineBreakCategory
{
    Mandatory,
    Optional,
    Prohibited,
};

struct Stack;

LineBreak get_line_break(char32_t c);
int find_next_line_break(const char* text, int start_index, bool* mandatory, Stack* stack);

#endif // UNICODE_LINE_BREAK_H_
