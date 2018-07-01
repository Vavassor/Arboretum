#ifndef UNICODE_GRAPHEME_CLUSTER_BREAK_H_
#define UNICODE_GRAPHEME_CLUSTER_BREAK_H_

#if defined(__cplusplus)
extern "C" {
#endif

#include "memory.h"

#include <uchar.h>

// These values must match the generated table in
// unicode_grapheme_cluster_break.cpp.
typedef enum GraphemeClusterBreak
{
    GRAPHEME_CLUSTER_BREAK_OTHER = 0,
    GRAPHEME_CLUSTER_BREAK_CARRIAGE_RETURN = 1,
    GRAPHEME_CLUSTER_BREAK_LINE_FEED = 2,
    GRAPHEME_CLUSTER_BREAK_CONTROL = 3,
    GRAPHEME_CLUSTER_BREAK_EXTEND = 4,
    GRAPHEME_CLUSTER_BREAK_ZERO_WIDTH_JOINER = 5,
    GRAPHEME_CLUSTER_BREAK_REGIONAL_INDICATOR = 6,
    GRAPHEME_CLUSTER_BREAK_PREPEND = 7,
    GRAPHEME_CLUSTER_BREAK_SPACING_MARK = 8,
    GRAPHEME_CLUSTER_BREAK_HANGUL_SYLLABLE_L = 9,
    GRAPHEME_CLUSTER_BREAK_HANGUL_SYLLABLE_V = 10,
    GRAPHEME_CLUSTER_BREAK_HANGUL_SYLLABLE_T = 11,
    GRAPHEME_CLUSTER_BREAK_HANGUL_SYLLABLE_LV = 12,
    GRAPHEME_CLUSTER_BREAK_HANGUL_SYLLABLE_LVT = 13,
    GRAPHEME_CLUSTER_BREAK_EMOJI_BASE = 14,
    GRAPHEME_CLUSTER_BREAK_EMOJI_MODIFIER = 15,
    GRAPHEME_CLUSTER_BREAK_GLUE_AFTER_ZWJ = 16, // Glue After Zero-Width Joiner
    GRAPHEME_CLUSTER_BREAK_EMOJI_BASE_GAZ = 17, // Emoji Base Glue After Zero-Width Joiner
} GraphemeClusterBreak;

GraphemeClusterBreak get_grapheme_cluster_break(char32_t c);
int find_prior_beginning_of_grapheme_cluster(const char* text, int start_index, Stack* stack);
int find_next_end_of_grapheme_cluster(const char* text, int start_index, Stack* stack);
bool test_grapheme_cluster_break(const char* text, int text_index, Stack* stack);

#if defined(__cplusplus)
} // extern "C"
#endif

#endif // UNICODE_GRAPHEME_CLUSTER_BREAK_H_
