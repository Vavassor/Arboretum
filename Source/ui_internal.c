#include "ui.h"
#include "ui_internal.h"

#include "string_utilities.h"

float ui_get_list_item_height(UiList* list, UiTextBlock* text_block, float line_height)
{
    float spacing = list->item_spacing;
    UiPadding padding = text_block->padding;
    return padding.top + line_height + padding.bottom + spacing;
}

bool ui_in_focus_scope(UiItem* outer, UiItem* item)
{
    if(outer->id == item->id)
    {
        return true;
    }

    UiContainer* container = &outer->container;

    for(int i = 0; i < container->items_count; i += 1)
    {
        UiItem* inside = &container->items[i];
        if(inside->id == item->id)
        {
            return true;
        }
        if(inside->type == UI_ITEM_TYPE_CONTAINER)
        {
            bool in_scope = ui_in_focus_scope(inside, item);
            if(in_scope)
            {
                return true;
            }
        }
    }

    return false;
}

Float2 ui_compute_cursor_position(UiTextBlock* text_block, Float2 dimensions, float line_height, int index)
{
    UiPadding padding = text_block->padding;

    // @Incomplete: There isn't a one-to-one mapping between chars and glyphs
    // Add a lookup to find the glyph given and index in the string.
    Float2 position = {{padding.start, -padding.top - line_height}};
    int size = string_size(text_block->text);
    if(index >= size && size > 0)
    {
        UiGlyph glyph = text_block->glyphs[text_block->glyphs_count - 1];
        Float2 bottom_right;
        bottom_right.x = glyph.baseline_start.x + glyph.x_advance;
        bottom_right.y = glyph.baseline_start.y;
        position = bottom_right;
    }
    else if(index >= 0)
    {
        void* value;
        void* key = (void*) (uintptr_t) index;
        bool found = map_get(&text_block->glyph_map, key, &value);
        if(found)
        {
            uintptr_t glyph_index = (uintptr_t) value;
            UiGlyph glyph = text_block->glyphs[glyph_index];
            position = glyph.baseline_start;
        }
    }

    return position;
}
