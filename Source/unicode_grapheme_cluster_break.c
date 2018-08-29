#include "unicode_grapheme_cluster_break.h"

#include "assert.h"
#include "memory.h"
#include "string_utilities.h"
#include "unicode.h"

// The rules and tables in this file are based on the extended grapheme cluster
// boundary rules described in Unicode Standard Annex #29 revision 31
// http://www.unicode.org/reports/tr29/tr29-31.html#Grapheme_Cluster_Boundaries
// for Unicode version 10.0.0

// grapheme_cluster_break table, 25088 bytes
static uint8_t* grapheme_cluster_break_stage1;

// @Optimize: This only uses 5 bits of each value in the table. It could
// be combined with the second-stage tables from line and word breaking to be
// more memory-efficient.
static uint8_t* grapheme_cluster_break_stage2;

void set_grapheme_cluster_break_tables(uint8_t* stage1, uint8_t* stage2)
{
    ASSERT(stage1);
    ASSERT(stage2);
    grapheme_cluster_break_stage1 = stage1;
    grapheme_cluster_break_stage2 = stage2;
}

void destroy_grapheme_cluster_break_tables(Heap* heap)
{
    SAFE_HEAP_DEALLOCATE(heap, grapheme_cluster_break_stage1);
    SAFE_HEAP_DEALLOCATE(heap, grapheme_cluster_break_stage2);
}

GraphemeClusterBreak get_grapheme_cluster_break(char32_t c)
{
    const uint32_t BLOCK_SIZE = 256;
    ASSERT(c < 0x110000);
    uint32_t block_offset = grapheme_cluster_break_stage1[c / BLOCK_SIZE] * BLOCK_SIZE;
    uint8_t value = grapheme_cluster_break_stage2[block_offset + c % BLOCK_SIZE];

    ASSERT(value >= 0 && value <= 17);
    return (GraphemeClusterBreak) value;
}

typedef struct GraphemeClusterBreakContext
{
    const char* text;
    GraphemeClusterBreak* breaks;

    int text_size;
    int lowest_in_text;
    int highest_in_text;

    int breaks_cap;
    int head;
    int tail;
} GraphemeClusterBreakContext;

static bool is_empty(GraphemeClusterBreakContext* context)
{
    return context->head == context->tail;
}

static int get_break_at(GraphemeClusterBreakContext* context, int text_index, int break_index, GraphemeClusterBreak* result)
{
    if(text_index < 0)
    {
        return invalid_index;
    }

    bool first_fetch = is_empty(context);

    // Retrieve the break if it's been seen already.
    int wrap_mask = context->breaks_cap - 1;
    if(!first_fetch && text_index >= context->lowest_in_text && text_index <= context->highest_in_text)
    {
        int index = (break_index) & wrap_mask;
        GraphemeClusterBreak grapheme_break = context->breaks[index];
        *result = grapheme_break;

        int back_down = utf8_skip_to_prior_codepoint(context->text, text_index);
        ASSERT(back_down != invalid_index);
        return back_down;
    }

    // Obtain the next break.
    char32_t codepoint;
    int index = utf8_get_prior_codepoint(context->text, text_index, &codepoint);
    if(index == invalid_index)
    {
        return invalid_index;
    }
    GraphemeClusterBreak grapheme_break = get_grapheme_cluster_break(codepoint);

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
        context->breaks[context->tail] = grapheme_break;
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

        context->breaks[context->head] = grapheme_break;
        context->head = next;
    }

    *result = grapheme_break;
    return index;
}

// @Optimize: Checking each rule one after another is very unnecessary as many
// of them aren't dependent on prior rules having been checked.
//
// Pair cases could use a 2D table that cross-compares all possible break types.
// Each cell would then have a possible value of "allowed", "disallowed", or
// "indeterminate". If the lookup returns a determined value, return it.
// Otherwise, move on to the cases involving runs of codepoints, like for emoji
// sequences and regional indicators.
bool allow_grapheme_cluster_break(GraphemeClusterBreakContext* context, int text_index, int break_index)
{
    // Always break at the beginning and end of text.
    if(text_index == 0 || text_index >= context->text_size)
    {
        return true;
    }

    GraphemeClusterBreak a;
    GraphemeClusterBreak b;
    int a_index = get_break_at(context, text_index - 1, break_index - 1, &a);
    int b_index = get_break_at(context, text_index, break_index, &b);
    if(a_index == invalid_index || b_index == invalid_index)
    {
        return true;
    }

    // Do not break between a carriage return and line feed.
    bool left = a == GRAPHEME_CLUSTER_BREAK_CARRIAGE_RETURN;
    bool right = b == GRAPHEME_CLUSTER_BREAK_LINE_FEED;
    if(left && right)
    {
        return false;
    }

    // Break between control characters, line feeds, and carriage returns that
    // don't violate the prior carriage return/line feed rule.
    left = a == GRAPHEME_CLUSTER_BREAK_CARRIAGE_RETURN
            || a == GRAPHEME_CLUSTER_BREAK_LINE_FEED
            || a == GRAPHEME_CLUSTER_BREAK_CONTROL;
    right = b == GRAPHEME_CLUSTER_BREAK_CARRIAGE_RETURN
        || b == GRAPHEME_CLUSTER_BREAK_LINE_FEED
        || b == GRAPHEME_CLUSTER_BREAK_CONTROL;
    if(left || right)
    {
        return true;
    }

    // Do not break Hangul syllable sequences.
    left = a == GRAPHEME_CLUSTER_BREAK_HANGUL_SYLLABLE_L;
    right = b == GRAPHEME_CLUSTER_BREAK_HANGUL_SYLLABLE_L
        || b == GRAPHEME_CLUSTER_BREAK_HANGUL_SYLLABLE_V
        || b == GRAPHEME_CLUSTER_BREAK_HANGUL_SYLLABLE_LV
        || b == GRAPHEME_CLUSTER_BREAK_HANGUL_SYLLABLE_LVT;
    if(left && right)
    {
        return false;
    }

    left = a == GRAPHEME_CLUSTER_BREAK_HANGUL_SYLLABLE_V
        || a == GRAPHEME_CLUSTER_BREAK_HANGUL_SYLLABLE_LV;
    right = b == GRAPHEME_CLUSTER_BREAK_HANGUL_SYLLABLE_V
        || b == GRAPHEME_CLUSTER_BREAK_HANGUL_SYLLABLE_T;
    if(left && right)
    {
        return false;
    }

    left = a == GRAPHEME_CLUSTER_BREAK_HANGUL_SYLLABLE_T
        || a == GRAPHEME_CLUSTER_BREAK_HANGUL_SYLLABLE_LVT;
    right = b == GRAPHEME_CLUSTER_BREAK_HANGUL_SYLLABLE_T;
    if(left && right)
    {
        return false;
    }

    // Do not break before extending characters, zero-width joiner, or spacing
    // marks.
    right = b == GRAPHEME_CLUSTER_BREAK_EXTEND
        || b == GRAPHEME_CLUSTER_BREAK_ZERO_WIDTH_JOINER
        || b == GRAPHEME_CLUSTER_BREAK_SPACING_MARK;
    if(right)
    {
        return false;
    }

    // Do not break after Prepend characters.
    left = a == GRAPHEME_CLUSTER_BREAK_PREPEND;
    if(left)
    {
        return false;
    }

    // Do not break within emoji modifier sequences.
    if(b == GRAPHEME_CLUSTER_BREAK_EMOJI_MODIFIER)
    {
        for(int i = a_index, j = break_index - 1; i >= 0; j -= 1)
        {
            GraphemeClusterBreak value;
            int index = get_break_at(context, i, j, &value);
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
    left = a == GRAPHEME_CLUSTER_BREAK_ZERO_WIDTH_JOINER;
    right = b == GRAPHEME_CLUSTER_BREAK_GLUE_AFTER_ZWJ
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
            int index = get_break_at(context, i, j, &value);
            if(index == invalid_index || value != GRAPHEME_CLUSTER_BREAK_REGIONAL_INDICATOR)
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

int find_prior_beginning_of_grapheme_cluster(const char* text, int start_index, Stack* stack)
{
    const int breaks_cap = 64;

    GraphemeClusterBreakContext context = {0};
    context.text = text;
    context.breaks = STACK_ALLOCATE(stack, GraphemeClusterBreak, breaks_cap);
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
        bool allowed = allow_grapheme_cluster_break(&context, i, j);
        if(allowed)
        {
            found = i;
            break;
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

int find_next_end_of_grapheme_cluster(const char* text, int start_index, Stack* stack)
{
    const int breaks_cap = 64;

    GraphemeClusterBreakContext context = {0};
    context.text = text;
    context.breaks = STACK_ALLOCATE(stack, GraphemeClusterBreak, breaks_cap);
    context.lowest_in_text = start_index;
    context.highest_in_text = start_index;
    context.text_size = string_size(text);
    context.breaks_cap = breaks_cap;
    context.head = 0;
    context.tail = 0;

    char32_t dummy;
    int adjusted_start = utf8_get_next_codepoint(context.text, context.text_size, start_index + 1, &dummy);

    int found = invalid_index;
    for(int i = adjusted_start, j = 0; i != invalid_index; j += 1)
    {
        bool allowed = allow_grapheme_cluster_break(&context, i, j);
        if(allowed)
        {
            found = i;
            break;
        }

        i = utf8_get_next_codepoint(context.text, context.text_size, i + 1, &dummy);
    }

    STACK_DEALLOCATE(stack, context.breaks);

    if(found == invalid_index)
    {
        return context.text_size;
    }

    return found;
}

bool test_grapheme_cluster_break(const char* text, int text_index, Stack* stack)
{
    const int breaks_cap = 64;

    GraphemeClusterBreakContext context = {0};
    context.text = text;
    context.breaks = STACK_ALLOCATE(stack, GraphemeClusterBreak, breaks_cap);
    context.lowest_in_text = text_index;
    context.highest_in_text = text_index;
    context.text_size = string_size(text);
    context.breaks_cap = breaks_cap;
    context.head = 0;
    context.tail = 0;

    bool allowed = allow_grapheme_cluster_break(&context, text_index, 0);

    STACK_DEALLOCATE(stack, context.breaks);

    return allowed;
}
