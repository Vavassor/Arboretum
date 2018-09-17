#include "unicode_trie.h"

typedef union LowCode
{
    struct
    {
        uint16_t stage1_index : 10;
        uint16_t data_offset : 6;
    };
    uint16_t codepoint;
} LowCode;

typedef union HighCode
{
    struct
    {
        uint32_t reserved : 12;
        uint32_t stage1_offset : 6;
        uint32_t stage2_offset: 5;
        uint32_t stage3_offset: 5;
        uint32_t data_offset: 4;
    };
    uint32_t codepoint;
} HighCode;

void unicode_trie_destroy(UnicodeTrie* trie, Heap* heap)
{
    if(trie)
    {
        SAFE_HEAP_DEALLOCATE(heap, trie->indices);
        SAFE_HEAP_DEALLOCATE(heap, trie->data);
    }
}

uint32_t unicode_trie_get_value(UnicodeTrie* trie, char32_t codepoint)
{
    if(codepoint <= 0x7f)
    {
        return trie->data[codepoint];
    }
    else if(codepoint <= 0xfff)
    {
        LowCode low_code = {.codepoint = (uint32_t) codepoint};
        uint16_t block_index = trie->indices[low_code.stage1_index];
        return trie->data[block_index + low_code.data_offset];
    }
    else if(codepoint <= trie->high_end)
    {
        HighCode high_code = {.codepoint = (uint32_t) codepoint};
        const uint16_t low_table_length = 0xfff >> 6;
        uint16_t stage1_index = low_table_length + high_code.stage1_offset;
        uint16_t stage2_block = trie->indices[stage1_index];
        uint16_t stage2_index = stage2_block + high_code.stage2_offset;
        uint16_t stage3_block = trie->indices[stage2_index];
        int32_t data_block;
        if((stage3_block & 0x8000) == 0)
        {
            data_block = stage3_block + high_code.stage3_offset;
        }
        else
        {
            // 18-bit indices are stored in groups of 9 entries per 8 indices.
            uint16_t i3 = high_code.stage3_offset;
            stage3_block = (stage3_block & 0x7fff) + (i3 & ~7) + (i3 >> 3);
            i3 &= 7;
            data_block = ((int32_t) trie->indices[stage3_block]
                    << (2 + (2 * i3))) & 0x30000;
            stage3_block += 1;
            data_block |= trie->indices[stage3_block + i3];
        }
        return trie->data[data_block + high_code.data_offset];
    }
    else
    {
        return trie->default_value;
    }
}
