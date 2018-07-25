#include "unicode_line_break.h"

#include "assert.h"
#include "memory.h"
#include "string_utilities.h"
#include "unicode.h"

// The rules and tables in this file are based on the line breaking algorithm
// described in Unicode Standard Annex #14 revision 39
// http://www.unicode.org/reports/tr14/ for Unicode version 10.0.0

// line_break table, 36352 bytes
static uint8_t* line_break_stage1;

// @Optimize: This only uses 6 bits of each value in the table. It could
// be combined with the second stage tables from grapheme cluster and word
// breaking to be more memory-efficient.
static uint8_t* line_break_stage2;

void set_line_break_tables(uint8_t* stage1, uint8_t* stage2)
{
    line_break_stage1 = stage1;
    line_break_stage2 = stage2;
}

void destroy_line_break_tables(Heap* heap)
{
    SAFE_HEAP_DEALLOCATE(heap, line_break_stage1);
    SAFE_HEAP_DEALLOCATE(heap, line_break_stage2);
}

LineBreak get_line_break(char32_t c)
{
    const uint32_t BLOCK_SIZE = 128;
    ASSERT(c < 0x110000);
    uint32_t block_offset = line_break_stage1[c / BLOCK_SIZE] * BLOCK_SIZE;
    uint8_t b = line_break_stage2[block_offset + c % BLOCK_SIZE];

    ASSERT(b >= 0 && b <= 42);
    return (LineBreak) b;
}

typedef struct LineBreakContext
{
    const char* text;
    LineBreak* breaks;

    int text_size;
    int lowest_in_text;
    int highest_in_text;

    int breaks_cap;
    int head;
    int tail;
} LineBreakContext;

static LineBreak substitute_line_break(LineBreak line_break)
{
    switch(line_break)
    {
        case LINE_BREAK_AMBIGUOUS:
        case LINE_BREAK_SURROGATE:
        case LINE_BREAK_UNKNOWN:
            return LINE_BREAK_ORDINARY_ALPHABETIC_OR_SYMBOL;

        // @Incomplete: Unicode actually recommends substituting
        // LINE_BREAK_Combining_Mark (CM) for complex context-dependent
        // codepoints that are spacing or nonspacing marks (general categories
        // Mc and Mn). But, looking up general category is too much hassle,
        // especially since this substitution process is already a fallback for
        // real "good" behaviour. So this just substitutes AL for those, also.
        case LINE_BREAK_COMPLEX_CONTEXT_DEPENDENT:
            return LINE_BREAK_ORDINARY_ALPHABETIC_OR_SYMBOL;

        case LINE_BREAK_CONDITIONAL_JAPANESE_STARTER:
            return LINE_BREAK_NONSTARTERS;

        default:
            return line_break;
    }
}

static bool is_empty(LineBreakContext* context)
{
    return context->head == context->tail;
}

static int get_break_at(LineBreakContext* context, int start_index, int break_index, LineBreak* result)
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
        LineBreak line_break = context->breaks[index];
        *result = substitute_line_break(line_break);

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
    LineBreak line_break = get_line_break(codepoint);

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
        context->breaks[context->tail] = line_break;
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

        context->breaks[context->head] = line_break;
        context->head = next;
    }

    *result = substitute_line_break(line_break);
    return index;
}

LineBreak resolve_combining_mark(LineBreakContext* context, LineBreak line_break, int index, int break_index)
{
    bool next = line_break != LINE_BREAK_COMBINING_MARK
        && line_break != LINE_BREAK_ZERO_WIDTH_JOINER;
    if(next)
    {
        return line_break;
    }

    LineBreak unresolved = line_break;
    for(int i = index - 1, j = break_index - 1; i >= 0; j -= 1)
    {
        LineBreak c;
        int c_index = get_break_at(context, i, j, &c);
        if(c_index == invalid_index)
        {
            return LINE_BREAK_ORDINARY_ALPHABETIC_OR_SYMBOL;
        }
        i = c_index - 1;
        next = c != LINE_BREAK_COMBINING_MARK
            && c != LINE_BREAK_ZERO_WIDTH_JOINER;
        if(next)
        {
            next = c == LINE_BREAK_MANDATORY_BREAK
                || c == LINE_BREAK_CARRIAGE_RETURN
                || c == LINE_BREAK_LINE_FEED
                || c == LINE_BREAK_NEXT_LINE
                || c == LINE_BREAK_SPACE
                || c == LINE_BREAK_ZERO_WIDTH_SPACE;
            if(next)
            {
                return LINE_BREAK_ORDINARY_ALPHABETIC_OR_SYMBOL;
            }
            else
            {
                return c;
            }
        }
    }
    if(line_break == unresolved)
    {
        return LINE_BREAK_ORDINARY_ALPHABETIC_OR_SYMBOL;
    }

    return line_break;
}

// @Optimize: Checking each rule one after another is very unnecessary as many
// of them aren't dependent on prior rules having been checked.
//
// Pair cases could use a 2D table that cross-compares all possible break types.
// Each cell would contain either the break category or an "indeterminate"
// value. If the lookup returns a determined value, return it. Otherwise, move
// on to the cases involving runs of codepoints, like for emoji
// sequences and regional indicators.
//
// Note that there's complicating rules such as combining mark resolutions and
// break subsitutions that need to fit into the changes.
static LineBreakCategory categorise_line_break(LineBreakContext* context, int index, int break_index)
{
    //--- Non-Tailorable Rules ---

    // Never break at the start of text.
    if(index == 0)
    {
        return LINE_BREAK_CATEGORY_PROHIBITED;
    }

    // Always break at the end of text.
    if(index >= context->text_size)
    {
        return LINE_BREAK_CATEGORY_MANDATORY;
    }

    LineBreak a;
    LineBreak b;
    int a_index = get_break_at(context, index - 1, break_index - 1, &a);
    int b_index = get_break_at(context, index, break_index, &b);
    if(a_index == invalid_index)
    {
        return LINE_BREAK_CATEGORY_PROHIBITED;
    }
    if(b_index == invalid_index)
    {
        return LINE_BREAK_CATEGORY_MANDATORY;
    }

    // Never break between carriage return followed by line feed, but always
    // break after a lone carriage return.
    if(a == LINE_BREAK_CARRIAGE_RETURN)
    {
        if(b == LINE_BREAK_LINE_FEED)
        {
            return LINE_BREAK_CATEGORY_PROHIBITED;
        }
        else
        {
            return LINE_BREAK_CATEGORY_MANDATORY;
        }
    }

    // Treat line feed, next line, and mandatory break properties as hard line
    // breaks.
    bool left = a == LINE_BREAK_LINE_FEED
        || a == LINE_BREAK_NEXT_LINE
        || a == LINE_BREAK_MANDATORY_BREAK;
    if(left)
    {
        return LINE_BREAK_CATEGORY_MANDATORY;
    }

    // Never break before hard line breaks.
    bool right = b == LINE_BREAK_MANDATORY_BREAK
        || b == LINE_BREAK_CARRIAGE_RETURN
        || b == LINE_BREAK_LINE_FEED
        || b == LINE_BREAK_NEXT_LINE;
    if(right)
    {
        return LINE_BREAK_CATEGORY_PROHIBITED;
    }

    // Never break before a space or zero-width space.
    if(b == LINE_BREAK_SPACE || b == LINE_BREAK_ZERO_WIDTH_SPACE)
    {
        return LINE_BREAK_CATEGORY_PROHIBITED;
    }

    // Break before any character following a zero-width space, even if one or
    // more spaces intervene.
    if(a == LINE_BREAK_ZERO_WIDTH_SPACE)
    {
        return LINE_BREAK_CATEGORY_OPTIONAL;
    }
    if(a == LINE_BREAK_SPACE)
    {
        for(int i = a_index - 1, j = break_index - 2; i >= 0; j -= 1)
        {
            LineBreak c;
            int c_index = get_break_at(context, i, j, &c);
            if(c_index == invalid_index)
            {
                break;
            }
            i = c_index - 1;
            if(c == LINE_BREAK_ZERO_WIDTH_SPACE)
            {
                return LINE_BREAK_CATEGORY_OPTIONAL;
            }
            if(c != LINE_BREAK_SPACE)
            {
                break;
            }
        }
    }

    // Do not break between a zero width joiner and an ideograph, emoji base,
    // or emoji modifier.
    left = a == LINE_BREAK_ZERO_WIDTH_JOINER;
    right = b == LINE_BREAK_IDEOGRAPHIC
        || b == LINE_BREAK_EMOJI_BASE
        || b == LINE_BREAK_EMOJI_MODIFIER;
    if(left && right)
    {
        return LINE_BREAK_CATEGORY_PROHIBITED;
    }

    // Do not break a combining character sequence.
    if(a == LINE_BREAK_COMBINING_MARK || a == LINE_BREAK_ZERO_WIDTH_JOINER)
    {
        LineBreak a_unresolved = a;
        for(int i = a_index - 1, j = break_index - 2; i >= 0; j -= 1)
        {
            LineBreak c;
            int c_index = get_break_at(context, i, j, &c);
            if(c_index == invalid_index)
            {
                break;
            }
            i = c_index - 1;
            bool next = c != LINE_BREAK_COMBINING_MARK
                && c != LINE_BREAK_ZERO_WIDTH_JOINER;
            if(next)
            {
                next = c == LINE_BREAK_MANDATORY_BREAK
                    || c == LINE_BREAK_CARRIAGE_RETURN
                    || c == LINE_BREAK_LINE_FEED
                    || c == LINE_BREAK_NEXT_LINE
                    || c == LINE_BREAK_SPACE
                    || c == LINE_BREAK_ZERO_WIDTH_SPACE;
                if(next)
                {
                    a = LINE_BREAK_ORDINARY_ALPHABETIC_OR_SYMBOL;
                }
                else
                {
                    a = c;
                }
                break;
            }
        }
        if(a == a_unresolved)
        {
            a = LINE_BREAK_ORDINARY_ALPHABETIC_OR_SYMBOL;
        }
    }
    if(b == LINE_BREAK_COMBINING_MARK || b == LINE_BREAK_ZERO_WIDTH_JOINER)
    {
        left = a == LINE_BREAK_MANDATORY_BREAK
            || a == LINE_BREAK_CARRIAGE_RETURN
            || a == LINE_BREAK_LINE_FEED
            || a == LINE_BREAK_NEXT_LINE
            || a == LINE_BREAK_SPACE
            || a == LINE_BREAK_ZERO_WIDTH_SPACE;
        if(left)
        {
            b = LINE_BREAK_ORDINARY_ALPHABETIC_OR_SYMBOL;
        }
        else
        {
            return LINE_BREAK_CATEGORY_PROHIBITED;
        }
    }

    // Do not break before or after word joiner and related characters.
    if(a == LINE_BREAK_WORD_JOINER || b == LINE_BREAK_WORD_JOINER)
    {
        return LINE_BREAK_CATEGORY_PROHIBITED;
    }

    // Do not break after Non-breaking space and related characters.
    if(a == LINE_BREAK_NON_BREAKING)
    {
        return LINE_BREAK_CATEGORY_PROHIBITED;
    }

    //--- Tailorable Rules ---

    // Do not break before Non-breaking space and related characters, except
    // after spaces and hyphens.
    left = a != LINE_BREAK_SPACE
        && a != LINE_BREAK_BREAK_AFTER
        && a != LINE_BREAK_HYPHEN;
    right = b == LINE_BREAK_NON_BREAKING;
    if(left && right)
    {
        return LINE_BREAK_CATEGORY_PROHIBITED;
    }

    // Do not break before ']' or '!'or ';' or '/', even after spaces.
    right = b == LINE_BREAK_CLOSE_PUNCTUATION
        || b == LINE_BREAK_CLOSING_PARENTHESIS
        || b == LINE_BREAK_EXCLAMATION_INTERROGATION
        || b == LINE_BREAK_INFIX_NUMERIC_SEPARATOR
        || b == LINE_BREAK_SYMBOLS;
    if(right)
    {
        return LINE_BREAK_CATEGORY_PROHIBITED;
    }

    // Do not break after open punctuation, even after spaces.
    if(a == LINE_BREAK_OPEN_PUNCTUATION)
    {
        return LINE_BREAK_CATEGORY_PROHIBITED;
    }
    else if(a == LINE_BREAK_SPACE)
    {
        for(int i = a_index - 1, j = break_index - 2; i >= 0; j -= 1)
        {
            LineBreak c;
            int c_index = get_break_at(context, i, j, &c);
            if(c_index == invalid_index)
            {
                break;
            }
            c = resolve_combining_mark(context, c, c_index, j);
            if(c == LINE_BREAK_OPEN_PUNCTUATION)
            {
                return LINE_BREAK_CATEGORY_PROHIBITED;
            }
            if(c != LINE_BREAK_SPACE)
            {
                break;
            }
            i = c_index - 1;
        }
    }

    // Do not break between a quotation mark and open punctuation, even with
    // intervening spaces.
    if(b == LINE_BREAK_OPEN_PUNCTUATION)
    {
        for(int i = a_index, j = break_index - 1; i >= 0; j -= 1)
        {
            LineBreak c;
            int c_index = get_break_at(context, i, j, &c);
            if(c_index == invalid_index)
            {
                break;
            }
            c = resolve_combining_mark(context, c, c_index, j);
            if(c == LINE_BREAK_QUOTATION)
            {
                return LINE_BREAK_CATEGORY_PROHIBITED;
            }
            if(c != LINE_BREAK_SPACE)
            {
                break;
            }
            i = c_index - 1;
        }
    }

    // Do not break between closing punctuation and a nonstarter, even with
    // intervening spaces.
    if(b == LINE_BREAK_NONSTARTERS)
    {
        for(int i = a_index, j = break_index - 1; i >= 0; j -= 1)
        {
            LineBreak c;
            int c_index = get_break_at(context, i, j, &c);
            if(c_index == invalid_index)
            {
                break;
            }
            c = resolve_combining_mark(context, c, c_index, j);
            bool next = c == LINE_BREAK_CLOSE_PUNCTUATION
                || c == LINE_BREAK_CLOSING_PARENTHESIS;
            if(next)
            {
                return LINE_BREAK_CATEGORY_PROHIBITED;
            }
            if(c != LINE_BREAK_SPACE)
            {
                break;
            }
            i = c_index - 1;
        }
    }

    // Do not break within B2, even with intervening spaces.
    if(b == LINE_BREAK_BREAK_OPPORTUNITY_BEFORE_AND_AFTER)
    {
        if(a == LINE_BREAK_BREAK_OPPORTUNITY_BEFORE_AND_AFTER)
        {
            return LINE_BREAK_CATEGORY_PROHIBITED;
        }
        for(int i = a_index - 1, j = break_index - 2; i >= 0; j -= 1)
        {
            LineBreak c;
            int c_index = get_break_at(context, i, j, &c);
            if(c_index == invalid_index)
            {
                break;
            }
            c = resolve_combining_mark(context, c, c_index, j);
            if(c == LINE_BREAK_BREAK_OPPORTUNITY_BEFORE_AND_AFTER)
            {
                return LINE_BREAK_CATEGORY_PROHIBITED;
            }
            if(c != LINE_BREAK_SPACE)
            {
                break;
            }
            i = c_index - 1;
        }
    }

    // Break after spaces.
    if(a == LINE_BREAK_SPACE)
    {
        return LINE_BREAK_CATEGORY_OPTIONAL;
    }

    // Do not break before or after quotation marks.
    if(a == LINE_BREAK_QUOTATION || b == LINE_BREAK_QUOTATION)
    {
        return LINE_BREAK_CATEGORY_PROHIBITED;
    }

    // Break before and after unresolved contingent breaks.
    left = a == LINE_BREAK_CONTINGENT_BREAK_OPPORTUNITY;
    right = b == LINE_BREAK_CONTINGENT_BREAK_OPPORTUNITY;
    if(left || right)
    {
        return LINE_BREAK_CATEGORY_OPTIONAL;
    }

    // Do not break before hyphen-minus, other hyphens, fixed-width spaces,
    // small kana, and other non-starters, or after acute accents.
    left = a == LINE_BREAK_BREAK_BEFORE;
    right = b == LINE_BREAK_BREAK_AFTER
        || b == LINE_BREAK_HYPHEN
        || b == LINE_BREAK_NONSTARTERS;
    if(left || right)
    {
        return LINE_BREAK_CATEGORY_PROHIBITED;
    }

    // Don't break after Hebrew followed by a hyphen.
    LineBreak c;
    int c_index = get_break_at(context, a_index - 1, break_index - 2, &c);
    if(c_index != invalid_index)
    {
        c = resolve_combining_mark(context, c, c_index, break_index - 2);
        left = a == LINE_BREAK_HYPHEN || a == LINE_BREAK_BREAK_AFTER;
        bool next = c == LINE_BREAK_HEBREW_LETTER;
        if(left && next)
        {
            return LINE_BREAK_CATEGORY_PROHIBITED;
        }
    }

    // Don’t break between Solidus and Hebrew letters.
    if(a == LINE_BREAK_SYMBOLS && b == LINE_BREAK_HEBREW_LETTER)
    {
        return LINE_BREAK_CATEGORY_PROHIBITED;
    }

    // Do not break between two ellipses, or between letters, numbers or
    // exclamations and ellipsis.
    left = a == LINE_BREAK_ORDINARY_ALPHABETIC_OR_SYMBOL
        || a == LINE_BREAK_EMOJI_BASE
        || a == LINE_BREAK_EMOJI_MODIFIER
        || a == LINE_BREAK_EXCLAMATION_INTERROGATION
        || a == LINE_BREAK_HEBREW_LETTER
        || a == LINE_BREAK_IDEOGRAPHIC
        || a == LINE_BREAK_INSEPARABLE_CHARACTERS
        || a == LINE_BREAK_NUMERIC;
    right = b == LINE_BREAK_INSEPARABLE_CHARACTERS;
    if(left && right)
    {
        return LINE_BREAK_CATEGORY_PROHIBITED;
    }

    // Do not break between digits and letters.
    left = a == LINE_BREAK_ORDINARY_ALPHABETIC_OR_SYMBOL
        || a == LINE_BREAK_HEBREW_LETTER;
    right = b == LINE_BREAK_NUMERIC;
    if(left && right)
    {
        return LINE_BREAK_CATEGORY_PROHIBITED;
    }

    left = a == LINE_BREAK_NUMERIC;
    right = b == LINE_BREAK_ORDINARY_ALPHABETIC_OR_SYMBOL
        || b == LINE_BREAK_HEBREW_LETTER;
    if(left && right)
    {
        return LINE_BREAK_CATEGORY_PROHIBITED;
    }

    // Do not break between numeric prefixes and ideographs, or between
    // ideographs and numeric postfixes.
    left = a == LINE_BREAK_PREFIX_NUMERIC;
    right = b == LINE_BREAK_IDEOGRAPHIC
        || b == LINE_BREAK_EMOJI_BASE
        || b == LINE_BREAK_EMOJI_MODIFIER;
    if(left && right)
    {
        return LINE_BREAK_CATEGORY_PROHIBITED;
    }

    left = a == LINE_BREAK_IDEOGRAPHIC
        || a == LINE_BREAK_EMOJI_BASE
        || a == LINE_BREAK_EMOJI_MODIFIER;
    right = b == LINE_BREAK_POSTFIX_NUMERIC;
    if(left && right)
    {
        return LINE_BREAK_CATEGORY_PROHIBITED;
    }

    // Do not break between numeric prefix/postfix and letters, or between
    // letters and prefix/postfix.
    left = a == LINE_BREAK_PREFIX_NUMERIC
        || a == LINE_BREAK_POSTFIX_NUMERIC;
    right = b == LINE_BREAK_ORDINARY_ALPHABETIC_OR_SYMBOL
        || b == LINE_BREAK_HEBREW_LETTER;
    if(left && right)
    {
        return LINE_BREAK_CATEGORY_PROHIBITED;
    }

    left = a == LINE_BREAK_ORDINARY_ALPHABETIC_OR_SYMBOL
        || a == LINE_BREAK_HEBREW_LETTER;
    right = b == LINE_BREAK_PREFIX_NUMERIC
        || b == LINE_BREAK_POSTFIX_NUMERIC;
    if(left && right)
    {
        return LINE_BREAK_CATEGORY_PROHIBITED;
    }

    // Do not break between the following pairs of classes relevant to numbers.
    bool uhh = (a == LINE_BREAK_CLOSE_PUNCTUATION && b == LINE_BREAK_POSTFIX_NUMERIC)
        || (a == LINE_BREAK_CLOSING_PARENTHESIS && b == LINE_BREAK_POSTFIX_NUMERIC)
        || (a == LINE_BREAK_CLOSE_PUNCTUATION && b == LINE_BREAK_PREFIX_NUMERIC)
        || (a == LINE_BREAK_CLOSING_PARENTHESIS && b == LINE_BREAK_PREFIX_NUMERIC)
        || (a == LINE_BREAK_NUMERIC && b == LINE_BREAK_POSTFIX_NUMERIC)
        || (a == LINE_BREAK_NUMERIC && b == LINE_BREAK_PREFIX_NUMERIC)
        || (a == LINE_BREAK_POSTFIX_NUMERIC && b == LINE_BREAK_OPEN_PUNCTUATION)
        || (a == LINE_BREAK_POSTFIX_NUMERIC && b == LINE_BREAK_NUMERIC)
        || (a == LINE_BREAK_PREFIX_NUMERIC && b == LINE_BREAK_OPEN_PUNCTUATION)
        || (a == LINE_BREAK_PREFIX_NUMERIC && b == LINE_BREAK_NUMERIC)
        || (a == LINE_BREAK_HYPHEN && b == LINE_BREAK_NUMERIC)
        || (a == LINE_BREAK_INFIX_NUMERIC_SEPARATOR && b == LINE_BREAK_NUMERIC)
        || (a == LINE_BREAK_NUMERIC && b == LINE_BREAK_NUMERIC)
        || (a == LINE_BREAK_SYMBOLS && b == LINE_BREAK_NUMERIC);
    if(uhh)
    {
        return LINE_BREAK_CATEGORY_PROHIBITED;
    }

    // Do not break a Korean syllable.
    left = a == LINE_BREAK_HANGUL_L_JAMO;
    right = b == LINE_BREAK_HANGUL_L_JAMO
        || b == LINE_BREAK_HANGUL_V_JAMO
        || b == LINE_BREAK_HANGUL_LV_SYLLABLE
        || b == LINE_BREAK_HANGUL_LVT_SYLLABLE;
    if(left && right)
    {
        return LINE_BREAK_CATEGORY_PROHIBITED;
    }

    left = a == LINE_BREAK_HANGUL_V_JAMO
        || a == LINE_BREAK_HANGUL_LV_SYLLABLE;
    right = b == LINE_BREAK_HANGUL_V_JAMO
        || b == LINE_BREAK_HANGUL_T_JAMO;
    if(left && right)
    {
        return LINE_BREAK_CATEGORY_PROHIBITED;
    }

    left = a == LINE_BREAK_HANGUL_T_JAMO
        || a == LINE_BREAK_HANGUL_LVT_SYLLABLE;
    right = b == LINE_BREAK_HANGUL_T_JAMO;
    if(left && right)
    {
        return LINE_BREAK_CATEGORY_PROHIBITED;
    }

    // Treat a Korean syllable block the same as an ideographic codepoint.
    left = a == LINE_BREAK_HANGUL_L_JAMO
        || a == LINE_BREAK_HANGUL_T_JAMO
        || a == LINE_BREAK_HANGUL_V_JAMO
        || a == LINE_BREAK_HANGUL_LV_SYLLABLE
        || a == LINE_BREAK_HANGUL_LVT_SYLLABLE;
    right = b == LINE_BREAK_INSEPARABLE_CHARACTERS
        || b == LINE_BREAK_POSTFIX_NUMERIC;
    if(left && right)
    {
        return LINE_BREAK_CATEGORY_PROHIBITED;
    }

    left = a == LINE_BREAK_PREFIX_NUMERIC;
    right = b == LINE_BREAK_HANGUL_L_JAMO
        || b == LINE_BREAK_HANGUL_T_JAMO
        || b == LINE_BREAK_HANGUL_V_JAMO
        || b == LINE_BREAK_HANGUL_LV_SYLLABLE
        || b == LINE_BREAK_HANGUL_LVT_SYLLABLE;
    if(left && right)
    {
        return LINE_BREAK_CATEGORY_PROHIBITED;
    }

    // Do not break between alphabetics.
    left = a == LINE_BREAK_ORDINARY_ALPHABETIC_OR_SYMBOL
        || a == LINE_BREAK_HEBREW_LETTER;
    right = b == LINE_BREAK_ORDINARY_ALPHABETIC_OR_SYMBOL
        || b == LINE_BREAK_HEBREW_LETTER;
    if(left && right)
    {
        return LINE_BREAK_CATEGORY_PROHIBITED;
    }

    // Do not break between numeric punctuation and alphabetics.
    left = a == LINE_BREAK_INFIX_NUMERIC_SEPARATOR;
    right = b == LINE_BREAK_ORDINARY_ALPHABETIC_OR_SYMBOL
        || b == LINE_BREAK_HEBREW_LETTER;
    if(left && right)
    {
        return LINE_BREAK_CATEGORY_PROHIBITED;
    }

    // Do not break between letters, numbers, or ordinary symbols and opening
    // or closing parentheses.
    left = a == LINE_BREAK_ORDINARY_ALPHABETIC_OR_SYMBOL
        || a == LINE_BREAK_HEBREW_LETTER
        || a == LINE_BREAK_NUMERIC;
    right = b == LINE_BREAK_OPEN_PUNCTUATION;
    if(left && right)
    {
        return LINE_BREAK_CATEGORY_PROHIBITED;
    }

    left = a == LINE_BREAK_CLOSING_PARENTHESIS;
    right = b == LINE_BREAK_ORDINARY_ALPHABETIC_OR_SYMBOL
        || b == LINE_BREAK_HEBREW_LETTER
        || b == LINE_BREAK_NUMERIC;
    if(left && right)
    {
        return LINE_BREAK_CATEGORY_PROHIBITED;
    }

    // Break between two regional indicator symbols if and only if there are an
    // even number of regional indicators preceding the position of the break.
    if(a == LINE_BREAK_REGIONAL_INDICATOR && b == LINE_BREAK_REGIONAL_INDICATOR)
    {
        int count = 0;
        for(int i = a_index, j = break_index - 1; i >= 0; j -= 1)
        {
            c_index = get_break_at(context, i, j, &c);
            if(c_index == invalid_index)
            {
                break;
            }
            else
            {
                LineBreak resolved = resolve_combining_mark(context, c, c_index, j);
                if(resolved != LINE_BREAK_REGIONAL_INDICATOR)
                {
                    break;
                }
                else if(c == LINE_BREAK_COMBINING_MARK || c == LINE_BREAK_ZERO_WIDTH_JOINER)
                {
                    // Skip counting combining marks.
                    i = c_index - 1;
                    continue;
                }
                i = c_index - 1;
            }
            count += 1;
        }
        if(count & 1)
        {
            return LINE_BREAK_CATEGORY_PROHIBITED;
        }
    }

    // Do not break between an emoji base and an emoji modifier.
    if(a == LINE_BREAK_EMOJI_BASE && b == LINE_BREAK_EMOJI_MODIFIER)
    {
        return LINE_BREAK_CATEGORY_PROHIBITED;
    }

    return LINE_BREAK_CATEGORY_OPTIONAL;
}

int find_next_line_break(const char* text, int start_index, bool* mandatory, Stack* stack)
{
    const int breaks_cap = 64;

    LineBreakContext context = {0};
    context.text = text;
    context.breaks = STACK_ALLOCATE(stack, LineBreak, breaks_cap);
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
        LineBreakCategory category = categorise_line_break(&context, i, j);
        if(category != LINE_BREAK_CATEGORY_PROHIBITED)
        {
            found = i;
            *mandatory = category == LINE_BREAK_CATEGORY_MANDATORY;
            break;
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

int find_next_mandatory_line_break(const char* text, int start_index, Stack* stack)
{
    int end = string_size(text);
    for(int i = start_index; i < end; i += 1)
    {
        bool mandatory;
        i = find_next_line_break(text, i, &mandatory, stack);
        if(mandatory)
        {
            return i;
        }
    }
    return end;
}
