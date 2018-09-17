#ifndef UNICODE_TRIE_H_
#define UNICODE_TRIE_H_

#include "memory.h"

#include <uchar.h>

typedef struct UnicodeTrie
{
    uint32_t* data;
    uint16_t* indices;
    char32_t high_end;
    uint32_t default_value;
} UnicodeTrie;

void unicode_trie_destroy(UnicodeTrie* trie, Heap* heap);
uint32_t unicode_trie_get_value(UnicodeTrie* trie, char32_t codepoint);

#endif // UNICODE_TRIE_H_
