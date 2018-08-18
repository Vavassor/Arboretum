#include "unicode_word_break.h"

#include "assert.h"
#include "memory.h"
#include "string_utilities.h"
#include "unicode.h"

// The rules and tables in this file are based on the default word boundary
// rules described in Unicode Standard Annex #29 revision 31
// http://www.unicode.org/reports/tr29/tr29-31.html#Word_Boundaries.
// for Unicode version 10.0.0

// word_break table, 31744 bytes
static uint8_t* word_break_stage1;

// @Optimize: This only uses 5 bits of each value in the table. It could
// be combined with the second stage tables from grapheme cluster and line
// breaking to be more memory-efficient.
static uint8_t* word_break_stage2;

void set_word_break_tables(uint8_t* stage1, uint8_t* stage2)
{
    word_break_stage1 = stage1;
    word_break_stage2 = stage2;
}

void destroy_word_break_tables(Heap* heap)
{
    SAFE_HEAP_DEALLOCATE(heap, word_break_stage1);
    SAFE_HEAP_DEALLOCATE(heap, word_break_stage2);
}

WordBreak get_word_break(char32_t c)
{
    const uint32_t BLOCK_SIZE = 256;
    ASSERT(c < 0x110000);
    uint32_t block_offset = word_break_stage1[c / BLOCK_SIZE] * BLOCK_SIZE;
    uint8_t word_break = word_break_stage2[block_offset + c % BLOCK_SIZE];

    ASSERT(word_break >= 0 && word_break <= 21);
    return (WordBreak) word_break;
}

typedef struct WordBreakContext
{
    const char* text;
    WordBreak* breaks;

    int lowest_in_text;
    int highest_in_text;
    int text_size;

    int breaks_cap;
    int head;
    int tail;
} WordBreakContext;

static bool is_empty(WordBreakContext* context)
{
    return context->head == context->tail;
}

static int get_break_at(WordBreakContext* context, int start_index, int break_index, WordBreak* result)
{
    if(start_index < 0)
    {
        return invalid_index;
    }

    bool first_fetch = is_empty(context);

    // Retrieve the break if it's been seen already.
    int wrap_mask = context->breaks_cap - 1;
    if(!first_fetch && start_index >= context->lowest_in_text && start_index <= context->highest_in_text)
    {
        int index = (break_index) & wrap_mask;
        WordBreak word_break = context->breaks[index];
        *result = word_break;

        int back_down = utf8_skip_to_prior_codepoint(context->text, start_index);
        ASSERT(back_down != invalid_index);
        return back_down;
    }

    // Obtain the next break.
    char32_t codepoint;
    int index = utf8_get_prior_codepoint(context->text, start_index, &codepoint);
    if(index == invalid_index)
    {
        return invalid_index;
    }
    WordBreak word_break = get_word_break(codepoint);

    // Store the break and return it.
    if(index < context->lowest_in_text || first_fetch)
    {
        context->lowest_in_text = index;
        if(first_fetch)
        {
            context->highest_in_text = index;
        }

        int next = (context->tail - 1) & wrap_mask;
        if(next == context->head)
        {
            // If the deque is full, evict the head so its spot can be used.
            int back_down = utf8_skip_to_prior_codepoint(context->text, context->highest_in_text - 1);
            ASSERT(back_down != invalid_index);
            context->highest_in_text = back_down;

            context->head = (context->head - 1) & wrap_mask;
        }

        context->tail = next;
        context->breaks[context->tail] = word_break;
    }
    else if(index > context->highest_in_text)
    {
        context->highest_in_text = index;

        int next = (context->head + 1) & wrap_mask;
        if(next == context->tail)
        {
            // If the deque is full, evict the tail so its spot can be used.
            int step_up = utf8_skip_to_next_codepoint(context->text, context->text_size, context->lowest_in_text + 1);
            ASSERT(step_up != invalid_index);
            context->lowest_in_text = step_up;

            context->tail = (context->tail + 1) & wrap_mask;
        }

        context->breaks[context->head] = word_break;
        context->head = next;
    }

    *result = word_break;
    return index;
}

static WordBreak resolve_ignore_sequence_before(WordBreakContext* context, WordBreak word_break, int text_index, int break_index, int* result_text, int* result_break)
{
    bool check = word_break != WORD_BREAK_EXTEND
        && word_break != WORD_BREAK_FORMAT
        && word_break != WORD_BREAK_ZERO_WIDTH_JOINER;
    if(check)
    {
        return word_break;
    }

    for(int i = text_index - 1, j = break_index - 1; i >= 0; j -= 1)
    {
        WordBreak value;
        int index = get_break_at(context, i, j, &value);
        if(index == invalid_index)
        {
            break;
        }
        check = value != WORD_BREAK_EXTEND
            && value != WORD_BREAK_FORMAT
            && value != WORD_BREAK_ZERO_WIDTH_JOINER;
        if(check)
        {
            if(result_text)
            {
                *result_text = index;
            }
            if(result_break)
            {
                *result_break = j;
            }
            return value;
        }
        i = index - 1;
    }

    // Return the break unresolved.
    return word_break;
}

static WordBreak resolve_ignore_sequence_after(WordBreakContext* context, WordBreak word_break, int text_index, int break_index)
{
    bool check = word_break != WORD_BREAK_EXTEND
        && word_break != WORD_BREAK_FORMAT
        && word_break != WORD_BREAK_ZERO_WIDTH_JOINER;
    if(check)
    {
        return word_break;
    }

    int start = utf8_skip_to_next_codepoint(context->text, context->text_size, text_index + 1);
    if(start == invalid_index)
    {
        return word_break;
    }

    for(int i = start, j = break_index + 1; i != invalid_index; j += 1)
    {
        WordBreak value;
        int index = get_break_at(context, i, j, &value);
        if(index == invalid_index)
        {
            break;
        }

        check = value != WORD_BREAK_EXTEND
            && value != WORD_BREAK_FORMAT
            && value != WORD_BREAK_ZERO_WIDTH_JOINER;
        if(check)
        {
            return value;
        }

        i = utf8_skip_to_next_codepoint(context->text, context->text_size, index + 1);
    }

    // Return the break unresolved.
    return word_break;
}

// @Optimize: Checking each rule one after another is unnecessary as many of
// them aren't dependent on prior rules having been checked.
//
// Pair cases could use a 2D table that cross-compares all possible break types.
// Each cell would then have a possible value of "allowed", "disallowed", or
// "indeterminate". If the lookup returns a determined value, return it.
// Otherwise, move on to the cases involving runs of codepoints, like for emoji
// sequences and regional indicators.
//
// Note that ignore sequences would need to be resolved before looking into the
// pair table.
static bool allow_word_break(WordBreakContext* context, int text_index, int break_index)
{
    // Always break at the beginning and end of text.
    if(text_index == 0 || text_index >= context->text_size)
    {
        return true;
    }

    WordBreak a;
    WordBreak b;
    int a_index = get_break_at(context, text_index - 1, break_index - 1, &a);
    int b_index = get_break_at(context, text_index, break_index, &b);
    if(a_index == invalid_index || b_index == invalid_index)
    {
        return true;
    }

    // Do not break between a carriage return and line feed.
    bool left = a == WORD_BREAK_CARRIAGE_RETURN;
    bool right = b == WORD_BREAK_LINE_FEED;
    if(left && right)
    {
        return false;
    }

    // Break before and after newlines that don't violate the prior
    // carriage return/line feed rule.
    left = a == WORD_BREAK_CARRIAGE_RETURN
        || a == WORD_BREAK_LINE_FEED
        || a == WORD_BREAK_NEWLINE;
    right = b == WORD_BREAK_CARRIAGE_RETURN
            || b == WORD_BREAK_LINE_FEED
            || b == WORD_BREAK_NEWLINE;
    if(left || right)
    {
        return true;
    }

    // Do not break within emoji zero-width joiner sequences.
    left = a == WORD_BREAK_ZERO_WIDTH_JOINER;
    right = b == WORD_BREAK_GLUE_AFTER_ZWJ
        || b == WORD_BREAK_EMOJI_BASE_GAZ;
    if(left && right)
    {
        return false;
    }

    // Ignore Format and Extend characters.
    right = b == WORD_BREAK_FORMAT
        || b == WORD_BREAK_EXTEND
        || b == WORD_BREAK_ZERO_WIDTH_JOINER;
    if(right)
    {
        return false;
    }
    int a_break_index;
    a = resolve_ignore_sequence_before(context, a, a_index, break_index - 1, &a_index, &a_break_index);

    // Do not break between most letters.
    left = a == WORD_BREAK_A_LETTER
        || a == WORD_BREAK_HEBREW_LETTER;
    right = b == WORD_BREAK_A_LETTER
        || b == WORD_BREAK_HEBREW_LETTER;
    if(left && right)
    {
        return false;
    }

    // Do not break letters across certain punctuation.
    left = a == WORD_BREAK_A_LETTER
        || a == WORD_BREAK_HEBREW_LETTER;
    bool center = b == WORD_BREAK_MID_LETTER
        || b == WORD_BREAK_MID_NUMBER_LETTER
        || b == WORD_BREAK_SINGLE_QUOTE;
    if(left && center)
    {
        int c_index = utf8_skip_to_next_codepoint(context->text, context->text_size, b_index + 1);
        if(c_index != invalid_index)
        {
            WordBreak c;
            c_index = get_break_at(context, c_index, break_index + 1, &c);
            if(c_index != invalid_index)
            {
                c = resolve_ignore_sequence_after(context, c, c_index, break_index + 1);
                right = c == WORD_BREAK_A_LETTER
                    || c == WORD_BREAK_HEBREW_LETTER;
                if(right)
                {
                    return false;
                }
            }
        }
    }

    center = a == WORD_BREAK_MID_LETTER
        || a == WORD_BREAK_MID_NUMBER_LETTER
        || a == WORD_BREAK_SINGLE_QUOTE;
    right = b == WORD_BREAK_A_LETTER
        || b == WORD_BREAK_HEBREW_LETTER;
    if(center && right)
    {
        WordBreak c;
        int c_index = get_break_at(context, a_index - 1, a_break_index - 1, &c);
        if(c_index != invalid_index)
        {
            c = resolve_ignore_sequence_before(context, c, c_index, a_break_index - 1, NULL, NULL);
            bool left = c == WORD_BREAK_A_LETTER
                || c == WORD_BREAK_HEBREW_LETTER;
            if(left)
            {
                return false;
            }
        }
    }

    if(a == WORD_BREAK_HEBREW_LETTER)
    {
        if(b == WORD_BREAK_SINGLE_QUOTE)
        {
            return false;
        }

        if(b == WORD_BREAK_DOUBLE_QUOTE)
        {
            int c_index = utf8_skip_to_next_codepoint(context->text, context->text_size, b_index + 1);
            if(c_index != invalid_index)
            {
                WordBreak c;
                c_index = get_break_at(context, c_index, break_index + 1, &c);
                if(c_index != invalid_index)
                {
                    c = resolve_ignore_sequence_after(context, c, c_index, break_index + 1);
                    if(c == WORD_BREAK_HEBREW_LETTER)
                    {
                        return false;
                    }
                }
            }
        }
    }

    if(a == WORD_BREAK_DOUBLE_QUOTE && b == WORD_BREAK_HEBREW_LETTER)
    {
        WordBreak c;
        int c_index = get_break_at(context, a_index - 1, a_break_index - 1, &c);
        if(c_index != invalid_index)
        {
            c = resolve_ignore_sequence_before(context, c, c_index, a_break_index - 1, &c_index, NULL);
            if(c == WORD_BREAK_HEBREW_LETTER)
            {
                return false;
            }
        }
    }

    // Do not break within sequences of digits, or digits adjacent to letters.
    if(a == WORD_BREAK_NUMERIC && b == WORD_BREAK_NUMERIC)
    {
        return false;
    }
    left = a == WORD_BREAK_NUMERIC;
    right = b == WORD_BREAK_A_LETTER || b == WORD_BREAK_HEBREW_LETTER;
    if(left && right)
    {
        return false;
    }
    left = a == WORD_BREAK_A_LETTER || a == WORD_BREAK_HEBREW_LETTER;
    right = b == WORD_BREAK_NUMERIC;
    if(left && right)
    {
        return false;
    }

    // Do not break within number sequences that contain punctuation such as
    // decimals and thousands separators.
    center = a == WORD_BREAK_MID_NUMBER
        || a == WORD_BREAK_MID_NUMBER_LETTER
        || a == WORD_BREAK_SINGLE_QUOTE;
    right = b == WORD_BREAK_NUMERIC;
    if(center && right)
    {
        WordBreak c;
        int c_index = get_break_at(context, a_index - 1, a_break_index - 1, &c);
        if(c_index != invalid_index)
        {
            c = resolve_ignore_sequence_before(context, c, c_index, a_break_index - 1, &c_index, NULL);
            if(c == WORD_BREAK_NUMERIC)
            {
                return false;
            }
        }
    }

    left = a == WORD_BREAK_NUMERIC;
    center = b == WORD_BREAK_MID_NUMBER
        || b == WORD_BREAK_MID_NUMBER_LETTER
        || b == WORD_BREAK_SINGLE_QUOTE;
    if(left && center)
    {
        int c_index = utf8_skip_to_next_codepoint(context->text, context->text_size, b_index + 1);
        if(c_index != invalid_index)
        {
            WordBreak c;
            c_index = get_break_at(context, c_index, break_index + 1, &c);
            if(c_index != invalid_index)
            {
                c = resolve_ignore_sequence_after(context, c, c_index, break_index + 1);
                if(c == WORD_BREAK_NUMERIC)
                {
                    return false;
                }
            }
        }
    }

    // Do not break between katakana.
    if(a == WORD_BREAK_KATAKANA && b == WORD_BREAK_KATAKANA)
    {
        return false;
    }

    // Do not break from extenders.
    left = a == WORD_BREAK_A_LETTER
        || a == WORD_BREAK_HEBREW_LETTER
        || a == WORD_BREAK_NUMERIC
        || a == WORD_BREAK_KATAKANA
        || a == WORD_BREAK_EXTEND_NUMBER_LETTER;
    right = b == WORD_BREAK_EXTEND_NUMBER_LETTER;
    if(left && right)
    {
        return false;
    }

    left = a == WORD_BREAK_EXTEND_NUMBER_LETTER;
    right = b == WORD_BREAK_A_LETTER
        || b == WORD_BREAK_HEBREW_LETTER
        || b == WORD_BREAK_NUMERIC
        || b == WORD_BREAK_KATAKANA
        || b == WORD_BREAK_EXTEND_NUMBER_LETTER;
    if(left && right)
    {
        return false;
    }

    // Do not break within emoji modifier sequences.
    left = a == WORD_BREAK_EMOJI_BASE
        || a == WORD_BREAK_EMOJI_BASE_GAZ;
    right = b == WORD_BREAK_EMOJI_MODIFIER;
    if(left && right)
    {
        return false;
    }

    // Do not break between regional indicator (RI) symbols if there is an odd
    // number of RI characters before the break point.
    left = a == WORD_BREAK_REGIONAL_INDICATOR;
    right = b == WORD_BREAK_REGIONAL_INDICATOR;
    if(left && right)
    {
        int count = 1;
        for(int i = a_index - 1, j = a_break_index - 1; i >= 0; j -= 1)
        {
            WordBreak value;
            int index = get_break_at(context, i, j, &value);
            if(index == invalid_index)
            {
                break;
            }
            value = resolve_ignore_sequence_before(context, value, index, j, &index, &j);
            if(value != WORD_BREAK_REGIONAL_INDICATOR)
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

// This is an arbitrary choice of word break types that are used to determine
// whether a codepoint is part of a word or spacing between words.
static bool is_considered_spacing(WordBreak word_break)
{
    return word_break == WORD_BREAK_OTHER
        || word_break == WORD_BREAK_CARRIAGE_RETURN
        || word_break == WORD_BREAK_LINE_FEED
        || word_break == WORD_BREAK_NEWLINE;
}

int find_prior_beginning_of_word(const char* text, int start_index, Stack* stack)
{
    const int breaks_cap = 64;

    WordBreakContext context = {0};
    context.text = text;
    context.breaks = STACK_ALLOCATE(stack, WordBreak, breaks_cap);
    context.lowest_in_text = start_index;
    context.highest_in_text = start_index;
    context.text_size = string_size(text);
    context.breaks_cap = breaks_cap;
    context.head = 0;
    context.tail = 0;

    int adjusted_start = utf8_skip_to_prior_codepoint(context.text, start_index - 1);

    int found = invalid_index;
    for(int i = adjusted_start, j = 0; i != invalid_index; j -= 1)
    {
        bool allowed = allow_word_break(&context, i, j);
        if(allowed)
        {
            WordBreak left;
            WordBreak right;
            int left_index = get_break_at(&context, i - 1, j - 1, &left);
            int right_index = get_break_at(&context, i, j, &right);
            bool check = left_index != invalid_index
                && is_considered_spacing(left)
                && right_index != invalid_index
                && !is_considered_spacing(right);
            if(check)
            {
                found = i;
                break;
            }
        }

        i = utf8_skip_to_prior_codepoint(context.text, i - 1);
    }

    STACK_DEALLOCATE(stack, context.breaks);

    if(found == invalid_index)
    {
        return 0;
    }

    return found;
}

int find_next_end_of_word(const char* text, int start_index, Stack* stack)
{
    const int breaks_cap = 64;

    WordBreakContext context = {0};
    context.text = text;
    context.breaks = STACK_ALLOCATE(stack, WordBreak, breaks_cap);
    context.lowest_in_text = start_index;
    context.highest_in_text = start_index;
    context.text_size = string_size(text);
    context.breaks_cap = breaks_cap;
    context.head = 0;
    context.tail = 0;

    int adjusted_start = utf8_skip_to_next_codepoint(context.text, context.text_size, start_index + 1);

    int found = invalid_index;
    for(int i = adjusted_start, j = 0; i != invalid_index; j += 1)
    {
        bool allowed = allow_word_break(&context, i, j);
        if(allowed)
        {
            WordBreak left;
            WordBreak right;
            int left_index = get_break_at(&context, i - 1, j - 1, &left);
            int right_index = get_break_at(&context, i, j, &right);
            bool check = left_index != invalid_index
                && !is_considered_spacing(left)
                && right_index != invalid_index
                && is_considered_spacing(right);
            if(check)
            {
                found = i;
                break;
            }
        }

        i = utf8_skip_to_next_codepoint(context.text, context.text_size, i + 1);
    }

    STACK_DEALLOCATE(stack, context.breaks);

    if(found == invalid_index)
    {
        return context.text_size;
    }

    return found;
}

bool test_word_break(const char* text, int text_index, Stack* stack)
{
    const int breaks_cap = 64;

    WordBreakContext context = {0};
    context.text = text;
    context.breaks = STACK_ALLOCATE(stack, WordBreak, breaks_cap);
    context.lowest_in_text = text_index;
    context.highest_in_text = text_index;
    context.text_size = string_size(text);
    context.breaks_cap = breaks_cap;
    context.head = 0;
    context.tail = 0;

    bool allowed = allow_word_break(&context, text_index, 0);

    STACK_DEALLOCATE(stack, context.breaks);

    return allowed;
}
