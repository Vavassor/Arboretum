#ifndef UNICODE_GRAPHEME_CLUSTER_BREAK_H_
#define UNICODE_GRAPHEME_CLUSTER_BREAK_H_

// These values must match the generated table in
// unicode_grapheme_cluster_break.cpp.
enum class GraphemeClusterBreak
{
    Other = 0,
    Carriage_Return = 1,
    Line_Feed = 2,
    Control = 3,
    Extend = 4,
    Zero_Width_Joiner = 5,
    Regional_Indicator = 6,
    Prepend = 7,
    Spacing_Mark = 8,
    Hangul_Syllable_L = 9,
    Hangul_Syllable_V = 10,
    Hangul_Syllable_T = 11,
    Hangul_Syllable_LV = 12,
    Hangul_Syllable_LVT = 13,
    Emoji_Base = 14,
    Emoji_Modifier = 15,
    Glue_After_ZWJ = 16, // Glue After Zero-Width Joiner
    Emoji_Base_GAZ = 17, // Emoji Base Glue After Zero-Width Joiner
};

struct Stack;

GraphemeClusterBreak get_grapheme_cluster_break(char32_t c);
int find_prior_beginning_of_grapheme_cluster(const char* text, int start_index, Stack* stack);
int find_next_end_of_grapheme_cluster(const char* text, int start_index, Stack* stack);

#endif // UNICODE_GRAPHEME_CLUSTER_BREAK_H_
