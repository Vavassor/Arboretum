#ifndef ATR_H_
#define ATR_H_

#include "unicode_trie.h"

bool atr_load_file(UnicodeTrie* trie, const char* path, Heap* heap);

#endif // ATR_H_
