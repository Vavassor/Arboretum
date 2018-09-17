#ifndef UNICODE_BREAK_ITERATOR_H_
#define UNICODE_BREAK_ITERATOR_H_

#include "memory.h"
#include "unicode_trie.h"

typedef union Break
{
    struct
    {
        uint32_t grapheme_cluster : 5;
        uint32_t line : 6;
        uint32_t word : 5;
        uint32_t extended_pictographic : 1;
    };
    uint32_t all;
} Break;

typedef struct BreakIterator
{
    UnicodeTrie* trie;
    const char* text;
    Break* breaks;
    int lowest_in_text;
    int highest_in_text;
    int text_size;
    int breaks_cap;
    int head;
    int tail;
} BreakIterator;

bool test_grapheme_cluster_break(const char* text, int text_index,
        Stack* stack);

#endif // UNICODE_BREAK_ITERATOR_H_
