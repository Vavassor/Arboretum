#include "unicode_load_tables.h"

#include "asset_paths.h"
#include "filesystem.h"
#include "unicode_grapheme_cluster_break.h"
#include "unicode_line_break.h"
#include "unicode_word_break.h"

static void* load_table(const char* asset_name, Heap* heap, Stack* stack)
{
    char* path = get_unicode_data_path_by_name(asset_name, stack);

    void* contents;
    uint64_t bytes;
    bool loaded = load_whole_file(path, &contents, &bytes, stack);

    STACK_DEALLOCATE(stack, path);

    if(!loaded)
    {
        return NULL;
    }
    else
    {
        void* table = heap_allocate(heap, (uint32_t) bytes);
        copy_memory(table, contents, bytes);
        return table;
    }
}

void unicode_load_tables(Heap* heap, Stack* stack)
{
    uint8_t* stage1 = (uint8_t*) load_table("grapheme_cluster_break_stage1", heap, stack);
    uint8_t* stage2 = (uint8_t*) load_table("grapheme_cluster_break_stage2", heap, stack);
    set_grapheme_cluster_break_tables(stage1, stage2);

    stage1 = (uint8_t*) load_table("line_break_stage1", heap, stack);
    stage2 = (uint8_t*) load_table("line_break_stage2", heap, stack);
    set_line_break_tables(stage1, stage2);

    stage1 = (uint8_t*) load_table("word_break_stage1", heap, stack);
    stage2 = (uint8_t*) load_table("word_break_stage2", heap, stack);
    set_word_break_tables(stage1, stage2);
}

void unicode_unload_tables(Heap* heap)
{
    destroy_grapheme_cluster_break_tables(heap);
    destroy_line_break_tables(heap);
    destroy_word_break_tables(heap);
}
