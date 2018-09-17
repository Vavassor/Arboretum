#include "unicode_break_iterator.h"

#include "assert.h"
#include "invalid_index.h"
#include "unicode.h"

// These values must match the generated table in
// grapheme_cluster_break_stage2.bin.
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
    // Glue After Zero-Width Joiner
    GRAPHEME_CLUSTER_BREAK_GLUE_AFTER_ZWJ = 16,
    // Emoji Base Glue After Zero-Width Joiner
    GRAPHEME_CLUSTER_BREAK_EMOJI_BASE_GAZ = 17,
    GRAPHEME_CLUSTER_BREAK_COUNT = 18,
} GraphemeClusterBreak;

typedef enum PairType
{
    PAIR_TYPE_MANDATORY,
    PAIR_TYPE_NON_PAIR,
    PAIR_TYPE_OPTIONAL,
    PAIR_TYPE_PROHIBITED,
} PairType;

#define M PAIR_TYPE_MANDATORY
#define N PAIR_TYPE_NON_PAIR
#define O PAIR_TYPE_OPTIONAL
#define P PAIR_TYPE_PROHIBITED

static const PairType grapheme_cluster_pairs
        [GRAPHEME_CLUSTER_BREAK_COUNT][GRAPHEME_CLUSTER_BREAK_COUNT] =
{
    {N, O, O, O, P, P, N, N, P, N, N, N, N, N, N, N, N, N}, //  0
    {O, O, P, O, P, P, O, O, P, O, O, O, O, O, O, O, O, O}, //  1
    {O, O, O, O, P, P, O, O, P, O, O, O, O, O, O, O, O, O}, //  2
    {O, O, O, O, P, P, O, O, P, O, O, O, O, O, O, O, O, O}, //  3
    {N, O, O, O, P, P, N, N, P, N, N, N, N, N, N, N, N, N}, //  4
    {N, O, O, O, P, P, N, N, P, N, N, N, N, N, N, N, N, N}, //  5
    {N, O, O, O, P, P, N, N, P, N, N, N, N, N, N, N, N, N}, //  6
    {P, P, P, P, P, P, P, P, P, P, P, P, P, P, P, P, P, P}, //  7
    {N, O, O, O, P, P, N, N, P, N, N, N, N, N, N, N, N, N}, //  8
    {N, O, O, O, P, P, N, N, P, P, P, N, P, P, N, N, N, N}, //  9
    {N, O, O, O, P, P, N, N, P, N, P, P, N, N, N, N, N, N}, // 10
    {N, O, O, O, P, P, N, N, P, N, N, P, N, N, N, N, N, N}, // 11
    {N, O, O, O, P, P, N, N, P, N, P, P, N, N, N, N, N, N}, // 12
    {N, O, O, O, P, P, N, N, P, N, N, P, N, N, N, N, N, N}, // 13
    {N, O, O, O, P, P, N, N, P, N, N, N, N, N, N, N, N, N}, // 14
    {N, O, O, O, P, P, N, N, P, N, N, N, N, N, N, N, N, N}, // 15
    {N, O, O, O, P, P, N, N, P, N, N, N, N, N, N, N, N, N}, // 16
    {N, O, O, O, P, P, N, N, P, N, N, N, N, N, N, N, N, N}, // 17
};
//   0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17

#undef M
#undef N
#undef O
#undef P

static bool is_empty(BreakIterator* iterator)
{
    return iterator->head == iterator->tail;
}

static int get_break_at(BreakIterator* iterator, int start_index,
        int break_index, Break* result)
{
    if(start_index < 0)
    {
        return invalid_index;
    }

    bool first_fetch = is_empty(iterator);

    // Retrieve the break if it's been seen already.
    int wrap_mask = iterator->breaks_cap - 1;
    if(!first_fetch
            && start_index >= iterator->lowest_in_text
            && start_index <= iterator->highest_in_text)
    {
        int index = (break_index) & wrap_mask;
        Break found = iterator->breaks[index];
        *result = found;

        int back_down = utf8_skip_to_prior_codepoint(iterator->text,
                start_index);
        ASSERT(back_down != invalid_index);
        return back_down;
    }

    // Obtain the next break.
    char32_t codepoint;
    int index = utf8_get_prior_codepoint(iterator->text,
            start_index, &codepoint);
    if(index == invalid_index)
    {
        return invalid_index;
    }
    uint16_t value = (uint16_t) unicode_trie_get_value(iterator->trie,
            codepoint);
    Break next_break = {.all = value};

    // Store the break and return it.
    if(index < iterator->lowest_in_text || first_fetch)
    {
        iterator->lowest_in_text = index;
        if(first_fetch)
        {
            iterator->highest_in_text = index;
        }

        int next = (iterator->tail - 1) & wrap_mask;
        if(next == iterator->head)
        {
            // If the deque is full, evict the head so its spot can be used.
            int back_down = utf8_skip_to_prior_codepoint(iterator->text,
                    iterator->highest_in_text - 1);
            ASSERT(back_down != invalid_index);
            iterator->highest_in_text = back_down;

            iterator->head = (iterator->head - 1) & wrap_mask;
        }

        iterator->tail = next;
        iterator->breaks[iterator->tail] = next_break;
    }
    else if(index > iterator->highest_in_text)
    {
        iterator->highest_in_text = index;

        int next = (iterator->head + 1) & wrap_mask;
        if(next == iterator->tail)
        {
            // If the deque is full, evict the tail so its spot can be used.
            int step_up = utf8_skip_to_next_codepoint(iterator->text,
                    iterator->text_size, iterator->lowest_in_text + 1);
            ASSERT(step_up != invalid_index);
            iterator->lowest_in_text = step_up;

            iterator->tail = (iterator->tail + 1) & wrap_mask;
        }

        iterator->breaks[iterator->head] = next_break;
        iterator->head = next;
    }

    *result = next_break;
    return index;
}

static bool allow_grapheme_cluster_break(BreakIterator* iterator,
        int text_index, int break_index)
{
    // Always break at the beginning and end of text.
    if(text_index == 0 || text_index >= iterator->text_size)
    {
        return true;
    }

    // Get the break information for the index and the spot immediately to its
    // left.
    Break a_break;
    Break b_break;
    int a_index = get_break_at(iterator, text_index - 1, break_index - 1,
            &a_break);
    int b_index = get_break_at(iterator, text_index, break_index, &b_break);
    if(a_index == invalid_index || b_index == invalid_index)
    {
        return true;
    }

    // Look the pair up in a table to see if it fits any of the cases that
    // only require a pair, instead of a more complicated sequence.
    int pair_left = a_break.grapheme_cluster;
    int pair_right = b_break.grapheme_cluster;
    PairType pair_type = grapheme_cluster_pairs[pair_left][pair_right];
    if(pair_type != PAIR_TYPE_NON_PAIR)
    {
        ASSERT(pair_type != PAIR_TYPE_MANDATORY);
        return pair_type == PAIR_TYPE_OPTIONAL;
    }

    // None of the pair cases fit. Continue on to search some sequences.

    GraphemeClusterBreak a = (GraphemeClusterBreak) a_break.grapheme_cluster;
    GraphemeClusterBreak b = (GraphemeClusterBreak) b_break.grapheme_cluster;

    // Do not break within emoji modifier sequences.
    if(b == GRAPHEME_CLUSTER_BREAK_EMOJI_MODIFIER)
    {
        for(int i = a_index, j = break_index - 1; i >= 0; j -= 1)
        {
            GraphemeClusterBreak value;
            int index = get_break_at(iterator, i, j, &value);
            if(index == invalid_index)
            {
                break;
            }
            i = index - 1;
            bool next = value == GRAPHEME_CLUSTER_BREAK_EMOJI_BASE
                    || value == GRAPHEME_CLUSTER_BREAK_EMOJI_BASE_GAZ;
            if(next)
            {
                return false;
            }
            else if(value != GRAPHEME_CLUSTER_BREAK_EXTEND)
            {
                break;
            }
        }
    }

    // Do not break within emoji zero-width joiner sequences.
    bool left = a == GRAPHEME_CLUSTER_BREAK_ZERO_WIDTH_JOINER;
    bool right = b == GRAPHEME_CLUSTER_BREAK_GLUE_AFTER_ZWJ
            || b == GRAPHEME_CLUSTER_BREAK_EMOJI_BASE_GAZ;
    if(left && right)
    {
        return false;
    }

    // Do not break between regional indicator (RI) symbols if there is an odd
    // number of RI characters before the break point.
    left = a == GRAPHEME_CLUSTER_BREAK_REGIONAL_INDICATOR;
    right = b == GRAPHEME_CLUSTER_BREAK_REGIONAL_INDICATOR;
    if(left && right)
    {
        int count = 0;
        for(int i = a_index, j = break_index - 1; i >= 0; j -= 1)
        {
            GraphemeClusterBreak value;
            int index = get_break_at(iterator, i, j, &value);
            if(index == invalid_index
                    || value != GRAPHEME_CLUSTER_BREAK_REGIONAL_INDICATOR)
            {
                break;
            }
            i = index - 1;
            count += 1;
        }
        if(count & 1)
        {
            return false;
        }
    }

    return true;
}

bool test_grapheme_cluster_break(const char* text, int text_index, Stack* stack)
{
    const int breaks_cap = 64;

    BreakIterator iterator =
    {
        .text = text,
        .breaks = STACK_ALLOCATE(stack, Break, breaks_cap),
        .lowest_in_text = text_index,
        .highest_in_text = text_index,
        .text_size = string_size(text),
        .breaks_cap = breaks_cap,
    };

    bool allowed = allow_grapheme_cluster_break(&iterator, text_index, 0);

    STACK_DEALLOCATE(stack, iterator.breaks);

    return allowed;
}
