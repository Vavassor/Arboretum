#include "ui.h"

#include "array2.h"
#include "assert.h"
#include "colours.h"
#include "float_utilities.h"
#include "immediate.h"
#include "input.h"
#include "int_utilities.h"
#include "logging.h"
#include "memory.h"
#include "math_basics.h"
#include "platform.h"
#include "string_build.h"
#include "string_utilities.h"
#include "unicode.h"
#include "unicode_grapheme_cluster_break.h"
#include "unicode_line_break.h"
#include "unicode_word_break.h"
#include "vector_math.h"

const UiId ui_invalid_id = 0;

static void create_event_queue(UiEventQueue* queue, Heap* heap)
{
    queue->cap = 32;
    queue->events = HEAP_ALLOCATE(heap, UiEvent, queue->cap);
}

static void destroy_event_queue(UiEventQueue* queue, Heap* heap)
{
    SAFE_HEAP_DEALLOCATE(heap, queue->events);
}

static void enqueue(UiEventQueue* queue, UiEvent event)
{
    queue->events[queue->tail] = event;
    queue->tail = (queue->tail + 1) % queue->cap;
}

bool ui_dequeue(UiEventQueue* queue, UiEvent* event)
{
    if(queue->head == queue->tail)
    {
        return false;
    }
    else
    {
        *event = queue->events[queue->head];
        queue->head = (queue->head + 1) % queue->cap;
        return true;
    }
}

void ui_create_context(UiContext* context, Heap* heap, Stack* scratch)
{
    create_event_queue(&context->queue, heap);
    context->seed = 1;
    context->heap = heap;
    context->scratch = scratch;
}

static void destroy_text_block(UiTextBlock* text_block, Heap* heap)
{
    SAFE_HEAP_DEALLOCATE(heap, text_block->text);
    SAFE_HEAP_DEALLOCATE(heap, text_block->glyphs);

    map_destroy(&text_block->glyph_map, heap);
}

static void destroy_list(UiList* list, Heap* heap)
{
    for(int i = 0; i < list->items_count; i += 1)
    {
        destroy_text_block(&list->items[i], heap);
    }
    list->items_count = 0;

    SAFE_HEAP_DEALLOCATE(heap, list->items);
    SAFE_HEAP_DEALLOCATE(heap, list->items_bounds);
}

static void destroy_container(UiContainer* container, Heap* heap)
{
    if(container)
    {
        for(int i = 0; i < container->items_count; i += 1)
        {
            UiItem* item = &container->items[i];
            switch(item->type)
            {
                case UI_ITEM_TYPE_BUTTON:
                {
                    destroy_text_block(&item->button.text_block, heap);
                    break;
                }
                case UI_ITEM_TYPE_CONTAINER:
                {
                    destroy_container(&item->container, heap);
                    break;
                }
                case UI_ITEM_TYPE_LIST:
                {
                    destroy_list(&item->list, heap);
                    break;
                }
                case UI_ITEM_TYPE_TEXT_BLOCK:
                {
                    destroy_text_block(&item->text_block, heap);
                    break;
                }
                case UI_ITEM_TYPE_TEXT_INPUT:
                {
                    destroy_text_block(&item->text_input.text_block, heap);
                    destroy_text_block(&item->text_input.label, heap);
                    break;
                }
            }
        }
        container->items_count = 0;

        SAFE_HEAP_DEALLOCATE(heap, container->items);
    }
}

void ui_destroy_context(UiContext* context, Heap* heap)
{
    ASSERT(heap == context->heap);

    destroy_event_queue(&context->queue, heap);

    FOR_ALL(UiItem*, context->toplevel_containers)
    {
        UiItem* item = *it;
        destroy_container(&item->container, heap);
        SAFE_HEAP_DEALLOCATE(heap, item);
    }

    ARRAY_DESTROY(context->toplevel_containers, heap);
    ARRAY_DESTROY(context->tab_navigation_list, heap);
}

static UiId generate_id(UiContext* context)
{
    UiId id = context->seed;
    context->seed += 1;
    return id;
}

static bool in_focus_scope(UiItem* outer, UiItem* item)
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
            bool in_scope = in_focus_scope(inside, item);
            if(in_scope)
            {
                return true;
            }
        }
    }

    return false;
}

static UiId get_focus_scope(UiContext* context, UiItem* item)
{
    if(!item)
    {
        return ui_invalid_id;
    }

    FOR_ALL(UiItem*, context->toplevel_containers)
    {
        UiItem* toplevel = *it;
        bool within = in_focus_scope(toplevel, item);
        if(within)
        {
            return toplevel->id;
        }
    }

    return ui_invalid_id;
}

bool ui_focused_on(UiContext* context, UiItem* item)
{
    return item == context->focused_item;
}

void ui_focus_on_item(UiContext* context, UiItem* item)
{
    if(item != context->focused_item)
    {
        UiEvent event = {0};
        event.type = UI_EVENT_TYPE_FOCUS_CHANGE;
        if(item)
        {
            event.focus_change.now_focused = item->id;
        }
        if(context->focused_item)
        {
            event.focus_change.now_unfocused = context->focused_item->id;
        }
        event.focus_change.current_scope = get_focus_scope(context, item);
        enqueue(&context->queue, event);
    }

    context->focused_item = item;
}

bool ui_is_captor(UiContext* context, UiItem* item)
{
    return item == context->captor_item;
}

void ui_capture(UiContext* context, UiItem* item)
{
    context->captor_item = item;
}

static bool is_capture_within(UiContext* context, UiItem* item)
{
    ASSERT(item->type == UI_ITEM_TYPE_CONTAINER);

    if(!context->captor_item)
    {
        return false;
    }

    UiContainer* container = &item->container;
    for(int i = 0; i < container->items_count; i += 1)
    {
        UiItem* inside = &container->items[i];
        if(inside->id == context->captor_item->id)
        {
            return true;
        }
        if(inside->type == UI_ITEM_TYPE_CONTAINER)
        {
            bool within = is_capture_within(context, inside);
            if(within)
            {
                return true;
            }
        }
    }

    return false;
}

UiItem* ui_create_toplevel_container(UiContext* context, Heap* heap)
{
    UiItem* item = HEAP_ALLOCATE(heap, UiItem, 1);
    item->id = generate_id(context);
    ARRAY_ADD(context->toplevel_containers, item, heap);
    return item;
}

void ui_destroy_toplevel_container(UiContext* context, UiItem* item, Heap* heap)
{
    // If the container is currently focused, make sure it becomes unfocused.
    if(context->focused_item && in_focus_scope(item, context->focused_item))
    {
        ui_focus_on_item(context, NULL);
    }
    // Release capture if it's currently holding something.
    if(is_capture_within(context, item))
    {
        ui_capture(context, NULL);
    }

    destroy_container(&item->container, heap);

    // Find the container and overwrite it with the container on the end.
    int count = array_count(context->toplevel_containers);
    for(int i = 0; i < count; i += 1)
    {
        UiItem* next = context->toplevel_containers[i];
        if(next == item)
        {
            UiItem* last = context->toplevel_containers[count - 1];
            context->toplevel_containers[i] = last;
            break;
        }
    }

    // Only deallocate after the item is removed, because it sets the pointer to
    // null and would cause it not to be found.
    SAFE_HEAP_DEALLOCATE(heap, item);

    if(count)
    {
        UiItem** last = &ARRAY_LAST(context->toplevel_containers);
        ARRAY_REMOVE(context->toplevel_containers, last);
    }
}

void ui_empty_item(UiContext* context, UiItem* item)
{
    switch(item->type)
    {
        case UI_ITEM_TYPE_BUTTON:
        {
            break;
        }
        case UI_ITEM_TYPE_CONTAINER:
        {
            // Release capture if it's currently holding something.
            if(is_capture_within(context, item))
            {
                ui_capture(context, NULL);
            }

            destroy_container(&item->container, context->heap);
            break;
        }
        case UI_ITEM_TYPE_LIST:
        {
            destroy_list(&item->list, context->heap);
            break;
        }
        case UI_ITEM_TYPE_TEXT_BLOCK:
        {
            break;
        }
        case UI_ITEM_TYPE_TEXT_INPUT:
        {
            break;
        }
    }
}

void ui_add_row(UiContainer* container, int count, UiContext* context, Heap* heap)
{
    container->axis = UI_AXIS_HORIZONTAL;
    container->items = HEAP_ALLOCATE(heap, UiItem, count);
    container->items_count = count;
    for(int i = 0; i < count; i += 1)
    {
        container->items[i].id = generate_id(context);
    }
}

void ui_add_column(UiContainer* container, int count, UiContext* context, Heap* heap)
{
    container->axis = UI_AXIS_VERTICAL;
    container->items = HEAP_ALLOCATE(heap, UiItem, count);
    container->items_count = count;
    for(int i = 0; i < count; i += 1)
    {
        container->items[i].id = generate_id(context);
    }
}

void ui_set_text(UiTextBlock* text_block, const char* text, Heap* heap)
{
    // Clean up any prior text.
    map_destroy(&text_block->glyph_map, heap);

    replace_string(&text_block->text, text, heap);

    // @Incomplete: There isn't a one-to-one mapping between chars and glyphs.
    // UiGlyph count should be based on the mapping of the given text in a
    // particular font instead of the size in bytes.
    int size = MAX(string_size(text), 1);
    text_block->glyphs = HEAP_REALLOCATE(heap, text_block->glyphs, UiGlyph, size);
    text_block->glyphs_cap = size;
    map_create(&text_block->glyph_map, size, heap);
}

// The "main" axis of a container is the one items are placed along and the
// "cross" axis is the one perpendicular to it. The main axis could also be
// called longitudinal and the cross axis could be called transverse.

static int get_main_axis_index(UiAxis axis)
{
    return (int) axis;
}

static int get_cross_axis_index(UiAxis axis)
{
    return ((int) axis) ^ 1;
}

static float padding_along_axis(UiPadding padding, int axis)
{
    return padding.e[axis] + padding.e[axis + 2];
}

static float get_list_item_height(UiList* list, UiTextBlock* text_block, float line_height)
{
    float spacing = list->item_spacing;
    UiPadding padding = text_block->padding;
    return padding.top + line_height + padding.bottom + spacing;
}

static float get_scroll_bottom(UiItem* item, float line_height)
{
    float inner_height = item->bounds.dimensions.y;
    UiList* list = &item->list;
    float content_height = 0.0f;
    if(list->items_count > 0)
    {
        float lines = (float) list->items_count;
        content_height = lines * get_list_item_height(list, &list->items[0], line_height);
    }
    return fmaxf(content_height - inner_height, 0.0f);
}

static void set_scroll(UiItem* item, float scroll, float line_height)
{
    float scroll_bottom = get_scroll_bottom(item, line_height);
    item->list.scroll_top = clamp(scroll, 0.0f, scroll_bottom);
}

static void scroll(UiItem* item, float scroll_velocity_y, float line_height)
{
    const float speed = 120.0f;
    float scroll_top = item->list.scroll_top - (speed * scroll_velocity_y);
    set_scroll(item, scroll_top, line_height);
}

void ui_create_items(UiItem* item, int lines_count, Heap* heap)
{
    UiList* list = &item->list;

    list->items_count = lines_count;
    if(lines_count)
    {
        list->items = HEAP_ALLOCATE(heap, UiTextBlock, lines_count);
        list->items_bounds = HEAP_ALLOCATE(heap, Rect, lines_count);
    }
    else
    {
        list->items = NULL;
        list->items_bounds = NULL;
    }

    list->hovered_item_index = invalid_index;
    list->selected_item_index = invalid_index;
}

static Float2 measure_ideal_dimensions_text_block(UiTextBlock* text_block, UiContext* context)
{
    Float2 bounds = float2_zero;

    BmfFont* font = context->theme.font;
    Stack* stack = context->scratch;
    UiPadding padding = text_block->padding;

    Float2 texture_dimensions;
    texture_dimensions.x = (float) font->image_width;
    texture_dimensions.y = (float)font->image_height;

    text_block->glyphs_count = 0;
    map_clear(&text_block->glyph_map);

    char32_t prior_char = '\0';
    int size = string_size(text_block->text);
    int next_break = find_next_mandatory_line_break(text_block->text, 0, stack);
    Float2 pen = {{padding.start, -padding.top - font->line_height}};
    for(int i = 0; i < size; i += 1)
    {
        // @Incomplete: There isn't a one-to-one mapping between chars and
        // glyphs. Add a lookup to glyph given the next sequence of text.
        char32_t current;
        int text_index = utf8_get_next_codepoint(text_block->text, size, i, &current);
        if(text_index == invalid_index)
        {
            break;
        }

        if(text_index == next_break)
        {
            next_break = find_next_mandatory_line_break(text_block->text, next_break, stack);

            bounds.x = fmaxf(bounds.x, pen.x);
            pen.x = padding.start;
            pen.y -= font->line_height;
        }

        bool newline = is_newline(current);

        if(!is_default_ignorable(current) && !newline)
        {
            BmfGlyph* glyph = bmf_find_glyph(font, current);
            float kerning = bmf_lookup_kerning(font, prior_char, current);

            Float2 top_left = pen;
            top_left.x += glyph->offset.x;
            top_left.y -= glyph->offset.y;

            Rect viewport_rect;
            viewport_rect.dimensions = glyph->rect.dimensions;
            viewport_rect.bottom_left.x = top_left.x;
            viewport_rect.bottom_left.y = top_left.y + font->line_height - viewport_rect.dimensions.y;

            Rect texture_rect = glyph->rect;
            texture_rect.bottom_left = float2_pointwise_divide(texture_rect.bottom_left, texture_dimensions);
            texture_rect.dimensions = float2_pointwise_divide(texture_rect.dimensions, texture_dimensions);

            int glyph_index = text_block->glyphs_count;
            UiGlyph* typeset_glyph = &text_block->glyphs[glyph_index];
            typeset_glyph->rect = viewport_rect;
            typeset_glyph->texture_rect = texture_rect;
            typeset_glyph->baseline_start = pen;
            typeset_glyph->x_advance = glyph->x_advance;
            typeset_glyph->text_index = text_index;
            text_block->glyphs_count += 1;

            MAP_ADD(&text_block->glyph_map, text_index, glyph_index, context->heap);

            pen.x += glyph->x_advance + kerning;
        }
        else if(newline)
        {
            // Insert an invisible glyph so cursor movement and selection can
            // use that to find the newline.

            Rect viewport_rect;
            viewport_rect.dimensions.x = 0.0f;
            viewport_rect.dimensions.y = font->line_height;
            viewport_rect.bottom_left = pen;

            int glyph_index = text_block->glyphs_count;
            UiGlyph* typeset_glyph = &text_block->glyphs[glyph_index];
            typeset_glyph->rect = viewport_rect;
            typeset_glyph->texture_rect = (Rect){{{0.0f, 0.0f}}, {{0.0f, 0.0f}}};
            typeset_glyph->baseline_start = pen;
            typeset_glyph->x_advance = 0.0f;
            typeset_glyph->text_index = text_index;
            text_block->glyphs_count += 1;

            MAP_ADD(&text_block->glyph_map, text_index, glyph_index, context->heap);
        }

        prior_char = current;
        i = text_index;
    }

    pen.y = fabsf(pen.y);
    bounds = float2_max(bounds, pen);
    bounds.x += padding.bottom;
    bounds.y += padding.end;

    return bounds;
}

static Float2 measure_ideal_dimensions(UiContainer* container, UiContext* context)
{
    Float2 bounds = float2_zero;
    int main_axis = get_main_axis_index(container->axis);
    int cross_axis = get_cross_axis_index(container->axis);

    for(int i = 0; i < container->items_count; i += 1)
    {
        UiItem* item = &container->items[i];
        Float2 item_ideal;
        switch(item->type)
        {
            case UI_ITEM_TYPE_BUTTON:
            {
                item_ideal = measure_ideal_dimensions_text_block(&item->button.text_block, context);
                break;
            }
            case UI_ITEM_TYPE_CONTAINER:
            {
                item_ideal = measure_ideal_dimensions(&item->container, context);
                break;
            }
            case UI_ITEM_TYPE_LIST:
            {
                item_ideal = float2_plus_infinity;
                break;
            }
            case UI_ITEM_TYPE_TEXT_BLOCK:
            {
                item_ideal = measure_ideal_dimensions_text_block(&item->text_block, context);
                break;
            }
            case UI_ITEM_TYPE_TEXT_INPUT:
            {
                Float2 input = measure_ideal_dimensions_text_block(&item->text_input.text_block, context);
                Float2 hint = measure_ideal_dimensions_text_block(&item->text_input.label, context);
                item_ideal = float2_max(input, hint);
                break;
            }
        }
        item->ideal_dimensions = float2_max(item_ideal, item->min_dimensions);

        bounds.e[main_axis] += item->ideal_dimensions.e[main_axis];
        bounds.e[cross_axis] = fmaxf(bounds.e[cross_axis], item->ideal_dimensions.e[cross_axis]);
    }

    UiPadding padding = container->padding;
    bounds.x += padding.start + padding.end;
    bounds.y += padding.top + padding.bottom;

    return bounds;
}

static float compute_run_length(const char* text, int start, int end, BmfFont* font)
{
    float length = 0.0f;
    for(int i = start; i < end; i += 1)
    {
        // @Incomplete: There isn't a one-to-one mapping between chars and
        // glyphs. Add a lookup to glyph given the next sequence of text.
        char32_t current;
        int text_index = utf8_get_next_codepoint(text, end, i, &current);
        if(text_index == invalid_index)
        {
            break;
        }

        BmfGlyph* glyph = bmf_find_glyph(font, current);
        length += glyph->x_advance;
        i = text_index;
    }
    return length;
}

static void place_glyph(UiTextBlock* text_block, BmfGlyph* glyph, BmfFont* font, int text_index, Float2 pen, Float2 texture_dimensions, UiContext* context)
{
    Float2 top_left = pen;
    top_left.x += glyph->offset.x;
    top_left.y -= glyph->offset.y;

    Rect viewport_rect;
    viewport_rect.dimensions = glyph->rect.dimensions;
    viewport_rect.bottom_left.x = top_left.x;
    viewport_rect.bottom_left.y = top_left.y + font->line_height - viewport_rect.dimensions.y;

    Rect texture_rect = glyph->rect;
    texture_rect.bottom_left = float2_pointwise_divide(texture_rect.bottom_left, texture_dimensions);
    texture_rect.dimensions = float2_pointwise_divide(texture_rect.dimensions, texture_dimensions);

    int glyph_index = text_block->glyphs_count;
    UiGlyph* typeset_glyph = &text_block->glyphs[glyph_index];
    typeset_glyph->rect = viewport_rect;
    typeset_glyph->texture_rect = texture_rect;
    typeset_glyph->baseline_start = pen;
    typeset_glyph->x_advance = glyph->x_advance;
    typeset_glyph->text_index = text_index;
    text_block->glyphs_count += 1;

    MAP_ADD(&text_block->glyph_map, text_index, glyph_index, context->heap);
}

static Float2 measure_bound_dimensions_text_block(UiTextBlock* text_block, Float2 dimensions, UiContext* context)
{
    Float2 bounds = float2_zero;

    BmfFont* font = context->theme.font;
    Stack* stack = context->scratch;

    UiPadding padding = text_block->padding;
    float right = dimensions.x - padding.end;
    float whole_width = dimensions.x - padding.start - padding.end;

    text_block->glyphs_count = 0;
    map_clear(&text_block->glyph_map);

    Float2 texture_dimensions;
    texture_dimensions.x = (float) font->image_width;
    texture_dimensions.y = (float) font->image_height;

    const char* ellipsize_text = u8"…";
    float extra_length = 0.0f;
    if(text_block->text_overflow == UI_TEXT_OVERFLOW_ELLIPSIZE_END)
    {
        int size = string_size(ellipsize_text);
        extra_length = compute_run_length(ellipsize_text, 0, size, font);
    }
    bool add_ellipsis_pattern = false;

    bool mandatory_break;
    int next_break = find_next_line_break(text_block->text, 0, &mandatory_break, stack);
    float run_length = compute_run_length(text_block->text, 0, next_break, font);

    char32_t prior_char = '\0';
    Float2 pen = {{padding.start, -padding.top - font->line_height}};
    int text_index = 0;
    int size = string_size(text_block->text);

    for(int i = 0; i < size; i += 1)
    {
        // @Incomplete: There isn't a one-to-one mapping between chars and
        // glyphs. Add a lookup to glyph given the next sequence of text.
        char32_t current;
        text_index = utf8_get_next_codepoint(text_block->text, size, i, &current);
        if(text_index == invalid_index)
        {
            break;
        }

        // Handle the next line break opportunity found in the text.
        if(text_index == next_break)
        {
            bool is_mandatory = mandatory_break;
            int current_break = next_break;
            next_break = find_next_line_break(text_block->text, current_break, &mandatory_break, stack);
            run_length = compute_run_length(text_block->text, current_break, next_break, font);

            if(pen.x + run_length > right || is_mandatory)
            {
                if(text_block->text_overflow == UI_TEXT_OVERFLOW_WRAP)
                {
                    bounds.x = fmaxf(bounds.x, pen.x);
                    pen.x = padding.start;
                    pen.y -= font->line_height;
                }
                else if(text_block->text_overflow == UI_TEXT_OVERFLOW_ELLIPSIZE_END)
                {
                    if(is_mandatory)
                    {
                        // A mandatory break in an ellipsized text block should
                        // ellipsized at the break point.
                        add_ellipsis_pattern = true;
                        break;
                    }
                    else
                    {
                        // ignore the break
                    }
                }
            }
        }

        bool newline = is_newline(current);

        if(!is_default_ignorable(current) && !newline)
        {
            // Place the glyph as normal.

            BmfGlyph* glyph = bmf_find_glyph(font, current);

            // If the right size is close, but a line break wasn't expected.
            if(pen.x + glyph->x_advance + extra_length > right)
            {
                if(text_block->text_overflow == UI_TEXT_OVERFLOW_WRAP && run_length > whole_width)
                {
                    // If there is a long section of text with no break opportunities
                    // make an emergency break so it doesn't overflow the text box.
                    bounds.x = fmaxf(bounds.x, pen.x);
                    pen.x = padding.start;
                    pen.y -= font->line_height;
                }
                else if(text_block->text_overflow == UI_TEXT_OVERFLOW_ELLIPSIZE_END)
                {
                    // This is the normal end of an ellipsized text block.
                    add_ellipsis_pattern = true;
                    break;
                }
            }

            float kerning = bmf_lookup_kerning(font, prior_char, current);
            pen.x += kerning;

            place_glyph(text_block, glyph, font, text_index, pen, texture_dimensions, context);

            pen.x += glyph->x_advance;
        }
        else if(newline)
        {
            // Insert an invisible glyph so cursor movement and selection can
            // use that to find the newline.

            Rect viewport_rect;
            viewport_rect.dimensions.x = 0.0f;
            viewport_rect.dimensions.y = font->line_height;
            viewport_rect.bottom_left = pen;

            int glyph_index = text_block->glyphs_count;
            UiGlyph* typeset_glyph = &text_block->glyphs[glyph_index];
            typeset_glyph->rect = viewport_rect;
            typeset_glyph->texture_rect = (Rect){{{0.0f, 0.0f}}, {{0.0f, 0.0f}}};
            typeset_glyph->baseline_start = pen;
            typeset_glyph->x_advance = 0.0f;
            typeset_glyph->text_index = text_index;
            text_block->glyphs_count += 1;

            MAP_ADD(&text_block->glyph_map, text_index, glyph_index, context->heap);
        }

        prior_char = current;
        i = text_index;
    }

    if(text_block->text_overflow == UI_TEXT_OVERFLOW_ELLIPSIZE_END && add_ellipsis_pattern)
    {
        int size = string_size(ellipsize_text);
        for(int i = 0; i < size; i += 1)
        {
            char32_t current;
            int index = utf8_get_next_codepoint(ellipsize_text, size, i, &current);
            if(index == invalid_index)
            {
                break;
            }

            BmfGlyph* glyph = bmf_find_glyph(font, current);
            float kerning = bmf_lookup_kerning(font, prior_char, current);
            pen.x += kerning;

            place_glyph(text_block, glyph, font, text_index, pen, texture_dimensions, context);

            pen.x += glyph->x_advance;

            prior_char = current;
            i = index;
        }
    }

    pen.y = fabsf(pen.y);
    bounds = float2_max(bounds, pen);
    bounds.x += padding.bottom;
    bounds.y += padding.end;

    return bounds;
}

static Float2 measure_bound_dimensions(UiContainer* container, Float2 container_space, float shrink, UiAxis shrink_along, UiContext* context)
{
    Float2 result = float2_zero;
    int main_axis = get_main_axis_index(container->axis);
    int cross_axis = get_cross_axis_index(container->axis);
    int shrink_axis = get_main_axis_index(shrink_along);
    int nonshrink_axis = get_cross_axis_index(shrink_along);

    for(int i = 0; i < container->items_count; i += 1)
    {
        UiItem* item = &container->items[i];

        Float2 space;
        space.e[shrink_axis] = shrink * item->ideal_dimensions.e[shrink_axis];
        space.e[nonshrink_axis] = container_space.e[nonshrink_axis];
        space = float2_max(space, item->min_dimensions);

        Float2 dimensions;
        switch(item->type)
        {
            case UI_ITEM_TYPE_BUTTON:
            {
                dimensions = measure_bound_dimensions_text_block(&item->button.text_block, space, context);
                break;
            }
            case UI_ITEM_TYPE_CONTAINER:
            {
                dimensions = measure_bound_dimensions(&item->container, space, shrink, shrink_along, context);
                break;
            }
            case UI_ITEM_TYPE_LIST:
            {
                dimensions = float2_zero;
                break;
            }
            case UI_ITEM_TYPE_TEXT_BLOCK:
            {
                dimensions = measure_bound_dimensions_text_block(&item->text_block, space, context);
                break;
            }
            case UI_ITEM_TYPE_TEXT_INPUT:
            {
                Float2 input = measure_bound_dimensions_text_block(&item->text_input.text_block, space, context);
                Float2 hint = measure_bound_dimensions_text_block(&item->text_input.label, space, context);
                dimensions = float2_max(input, hint);
                break;
            }
        }
        item->bounds.dimensions = float2_max(dimensions, item->min_dimensions);

        result.e[main_axis] += item->bounds.dimensions.e[main_axis];
        result.e[cross_axis] = fmaxf(result.e[cross_axis], item->bounds.dimensions.e[cross_axis]);
    }

    UiPadding padding = container->padding;
    result.x += padding.start + padding.end;
    result.y += padding.top + padding.bottom;

    // Distribute any excess among the growable items, if allowed.
    if(result.e[main_axis] < container_space.e[main_axis])
    {
        int growable_items = 0;
        for(int i = 0; i < container->items_count; i += 1)
        {
            UiItem item = container->items[i];
            if(item.growable)
            {
                growable_items += 1;
            }
        }
        if(growable_items > 0)
        {
            float grow = (container_space.e[main_axis] - result.e[main_axis]) / growable_items;
            for(int i = 0; i < container->items_count; i += 1)
            {
                UiItem* item = &container->items[i];
                if(item->growable)
                {
                    item->bounds.dimensions.e[main_axis] += grow;
                }
            }
        }
    }
    if(container->alignment == UI_ALIGNMENT_STRETCH && result.e[cross_axis] < container_space.e[cross_axis])
    {
        result.e[cross_axis] = container_space.e[cross_axis];
        for(int i = 0; i < container->items_count; i += 1)
        {
            UiItem* item = &container->items[i];
            item->bounds.dimensions.e[cross_axis] = container_space.e[cross_axis];
        }
    }

    return result;
}

static float compute_shrink_factor(UiContainer* container, Rect space, float ideal_length)
{
    int main_axis = get_main_axis_index(container->axis);
    float available = space.dimensions.e[main_axis] - padding_along_axis(container->padding, main_axis);

    float shrink = available / ideal_length;
    int items_overshrunk;

    for(;;)
    {
        items_overshrunk = 0;
        float min_filled = 0.0f;
        float fit = 0.0f;
        for(int i = 0; i < container->items_count; i += 1)
        {
            UiItem* item = &container->items[i];

            float length = shrink * item->ideal_dimensions.e[main_axis];
            if(length < item->min_dimensions.e[main_axis])
            {
                length = item->min_dimensions.e[main_axis];
                min_filled += item->min_dimensions.e[main_axis];
                items_overshrunk += 1;
            }
            fit += length;
        }
        // fudge note:
        // This accounts for the case where there's enough space but floating-
        // point error causes the sum to overshoot the "available" mark
        // repeatedly.
        const float fudge = 1e-3f;
        if(fit - fudge <= available || items_overshrunk == container->items_count)
        {
            break;
        }
        else
        {
            shrink = (available - min_filled) / (ideal_length - min_filled);
        }
    }

    return shrink;
}

static void grow_or_commit_ideal_dimensions(UiItem* item, Float2 container_space)
{
    UiContainer* container = &item->container;
    int main_axis = get_main_axis_index(container->axis);
    int cross_axis = get_cross_axis_index(container->axis);

    item->bounds.dimensions.e[main_axis] = container_space.e[main_axis];
    item->bounds.dimensions.e[cross_axis] = item->ideal_dimensions.e[cross_axis];

    // Record how much space is taken up by items that won't grow and measure
    // the ideal length at the same time.
    float ungrowable_length = 0.0f;
    float ideal_length = 0.0f;
    for(int i = 0; i < container->items_count; i += 1)
    {
        UiItem* next = &container->items[i];
        if(!next->growable)
        {
            ungrowable_length += next->bounds.dimensions.e[main_axis];
        }
        ideal_length += next->ideal_dimensions.e[main_axis];
    }

    // Compute by how much to grow the growable items.
    float available = container_space.e[main_axis] - padding_along_axis(container->padding, main_axis);
    float grow = (available - ungrowable_length) / (ideal_length - ungrowable_length);

    // Grow them.
    for(int i = 0; i < container->items_count; i += 1)
    {
        UiItem* next = &container->items[i];

        Float2 space;
        if(next->growable)
        {
            space.e[main_axis] = grow * next->ideal_dimensions.e[main_axis];
            space.e[cross_axis] = next->ideal_dimensions.e[cross_axis];
        }
        else
        {
            space = next->ideal_dimensions;
        }
        if(container->alignment == UI_ALIGNMENT_STRETCH)
        {
            space.e[cross_axis] = item->ideal_dimensions.e[cross_axis];
        }

        switch(next->type)
        {
            case UI_ITEM_TYPE_CONTAINER:
            {
                grow_or_commit_ideal_dimensions(next, space);
                break;
            }
            case UI_ITEM_TYPE_BUTTON:
            case UI_ITEM_TYPE_LIST:
            case UI_ITEM_TYPE_TEXT_BLOCK:
            case UI_ITEM_TYPE_TEXT_INPUT:
            {
                next->bounds.dimensions = space;
                break;
            }
        }
    }
}

static float measure_length(UiContainer* container)
{
    int axis = get_main_axis_index(container->axis);
    float length = 0.0f;
    for(int i = 0; i < container->items_count; i += 1)
    {
        UiItem item = container->items[i];
        length += item.bounds.dimensions.e[axis];
    }
    return length;
}

static void place_items_along_main_axis(UiItem* item, Rect space)
{
    UiContainer* container = &item->container;
    UiPadding padding = container->padding;
    int axis = get_main_axis_index(container->axis);

    // Place the container within the space.
    switch(container->axis)
    {
        case UI_AXIS_HORIZONTAL:
        {
            item->bounds.bottom_left.x = space.bottom_left.x;
            break;
        }
        case UI_AXIS_VERTICAL:
        {
            float top = rect_top(space);
            item->bounds.bottom_left.y = top - item->bounds.dimensions.y;
            break;
        }
    }

    float first;
    float last;
    switch(container->axis)
    {
        case UI_AXIS_HORIZONTAL:
        {
            float left = item->bounds.bottom_left.x;
            float right = rect_right(item->bounds);
            switch(container->direction)
            {
                case UI_DIRECTION_LEFT_TO_RIGHT:
                {
                    first = left + padding.start;
                    last = right - padding.end;
                    break;
                }
                case UI_DIRECTION_RIGHT_TO_LEFT:
                {
                    first = right - padding.start;
                    last = left + padding.end;
                    break;
                }
            }
            break;
        }
        case UI_AXIS_VERTICAL:
        {
            float top = rect_top(item->bounds);
            float bottom = item->bounds.bottom_left.y;
            first = top - padding.top;
            last = bottom + padding.bottom;
            break;
        }
    }

    bool fill_backward = (container->axis == UI_AXIS_HORIZONTAL && container->direction == UI_DIRECTION_RIGHT_TO_LEFT) ||
        container->axis == UI_AXIS_VERTICAL;

    float cursor;
    float apart = 0.0f;
    switch(container->justification)
    {
        case UI_JUSTIFICATION_START:
        {
            cursor = first;
            break;
        }
        case UI_JUSTIFICATION_END:
        {
            float length = measure_length(container);
            if(fill_backward)
            {
                cursor = last + length;
            }
            else
            {
                cursor = last - length;
            }
            break;
        }
        case UI_JUSTIFICATION_CENTER:
        {
            float length = measure_length(container);
            float center = (last + first) / 2.0f;
            if(fill_backward)
            {
                cursor = center + (length / 2.0f);
            }
            else
            {
                cursor = center - (length / 2.0f);
            }
            break;
        }
        case UI_JUSTIFICATION_SPACE_AROUND:
        {
            float length = measure_length(container);
            float container_length = fabsf(last - first);
            apart = (container_length - length) / (container->items_count + 1);
            if(fill_backward)
            {
                cursor = first - apart;
            }
            else
            {
                cursor = first + apart;
            }
            break;
        }
        case UI_JUSTIFICATION_SPACE_BETWEEN:
        {
            float length = measure_length(container);
            float container_length = fabsf(last - first);
            apart = (container_length - length) / (container->items_count - 1);
            cursor = first;
            break;
        }
    }

    if(fill_backward)
    {
        for(int i = 0; i < container->items_count; i += 1)
        {
            UiItem* next = &container->items[i];

            cursor -= next->bounds.dimensions.e[axis];
            next->bounds.bottom_left.e[axis] = cursor;
            cursor -= apart;

            if(next->type == UI_ITEM_TYPE_CONTAINER && next->container.items_count > 0)
            {
                place_items_along_main_axis(next, next->bounds);
            }
        }
    }
    else
    {
        for(int i = 0; i < container->items_count; i += 1)
        {
            UiItem* next = &container->items[i];

            next->bounds.bottom_left.e[axis] = cursor;
            cursor += next->bounds.dimensions.e[axis] + apart;

            if(next->type == UI_ITEM_TYPE_CONTAINER && next->container.items_count > 0)
            {
                place_items_along_main_axis(next, next->bounds);
            }
        }
    }
}

static void place_items_along_cross_axis(UiItem* item, Rect space)
{
    UiContainer* container = &item->container;
    UiPadding padding = container->padding;
    int cross_axis = get_cross_axis_index(container->axis);

    float space_top = rect_top(space);
    item->bounds.bottom_left.y = space_top - item->bounds.dimensions.y;
    float centering = 0.0f;

    float position;
    switch(container->alignment)
    {
        case UI_ALIGNMENT_START:
        case UI_ALIGNMENT_STRETCH:
        {
            switch(container->axis)
            {
                case UI_AXIS_HORIZONTAL:
                {
                    float top = rect_top(item->bounds);
                    position = top - padding.top;
                    centering = 1.0f;
                    break;
                }
                case UI_AXIS_VERTICAL:
                {
                    switch(container->direction)
                    {
                        case UI_DIRECTION_LEFT_TO_RIGHT:
                        {
                            float left = item->bounds.bottom_left.x;
                            position = left + padding.start;
                            centering = 0.0f;
                            break;
                        }
                        case UI_DIRECTION_RIGHT_TO_LEFT:
                        {
                            float right = rect_right(item->bounds);
                            position = right - padding.start;
                            centering = 1.0f;
                            break;
                        }
                    }
                    break;
                }
            }
            break;
        }
        case UI_ALIGNMENT_END:
        {
            switch(container->axis)
            {
                case UI_AXIS_HORIZONTAL:
                {
                    position = item->bounds.bottom_left.y + padding.bottom;
                    centering = 0.0f;
                    break;
                }
                case UI_AXIS_VERTICAL:
                {
                    switch(container->direction)
                    {
                        case UI_DIRECTION_LEFT_TO_RIGHT:
                        {
                            float right = rect_right(item->bounds);
                            position = right - padding.end;
                            centering = 1.0f;
                            break;
                        }
                        case UI_DIRECTION_RIGHT_TO_LEFT:
                        {
                            float left = item->bounds.bottom_left.x;
                            position = left + padding.end;
                            centering = 0.0f;
                            break;
                        }
                    }
                    break;
                }
            }
            break;
        }
        case UI_ALIGNMENT_CENTER:
        {
            position = item->bounds.bottom_left.e[cross_axis] + (item->bounds.dimensions.e[cross_axis] / 2.0f);
            centering = 0.5f;
            break;
        }
    }

    for(int i = 0; i < container->items_count; i += 1)
    {
        UiItem* next = &container->items[i];
        next->bounds.bottom_left.e[cross_axis] = position - (centering * next->bounds.dimensions.e[cross_axis]);
        if(next->type == UI_ITEM_TYPE_CONTAINER && next->container.items_count > 0)
        {
            place_items_along_cross_axis(next, next->bounds);
        }
    }
}

void ui_lay_out(UiItem* item, Rect space, UiContext* context)
{
    UiContainer* container = &item->container;
    int main_axis = get_main_axis_index(container->axis);

    // Compute the ideal dimensions of the container and its contents.
    Float2 ideal = measure_ideal_dimensions(container, context);
    item->ideal_dimensions = float2_max(item->min_dimensions, ideal);
    float ideal_length = 0.0f;
    for(int i = 0; i < container->items_count; i += 1)
    {
        UiItem next = container->items[i];
        ideal_length += next.ideal_dimensions.e[main_axis];
    }

    // Determine whether there's enough space to fit all items at their ideal
    // dimensions.
    float available = space.dimensions.e[main_axis] - padding_along_axis(container->padding, main_axis);
    if(ideal_length <= available)
    {
        grow_or_commit_ideal_dimensions(item, space.dimensions);
    }
    else
    {
        // Determine how much to reduce each item lengthwise so everything can
        // fit in the container.
        float shrink = compute_shrink_factor(container, space, ideal_length);

        Float2 bound = measure_bound_dimensions(container, space.dimensions, shrink, container->axis, context);
        if(item->growable)
        {
            bound.e[main_axis] = space.dimensions.e[main_axis];
        }
        item->bounds.dimensions = float2_max(bound, item->min_dimensions);
    }

    // Now that all the items have been sized, just place them where they need to be.
    place_items_along_main_axis(item, space);
    place_items_along_cross_axis(item, space);
}

static void draw_text_block(UiTextBlock* text_block, Rect bounds)
{
    Float2 top_left = rect_top_left(bounds);

    for(int i = 0; i < text_block->glyphs_count; i += 1)
    {
        UiGlyph glyph = text_block->glyphs[i];
        Rect rect = glyph.rect;
        rect.bottom_left = float2_add(rect.bottom_left, top_left);
        Quad quad = rect_to_quad(rect);
        immediate_add_quad_textured(&quad, glyph.texture_rect);
    }
    immediate_set_blend_mode(BLEND_MODE_TRANSPARENT);
    immediate_draw();
}

static void draw_button(UiItem* item, UiContext* context)
{
    ASSERT(item->type == UI_ITEM_TYPE_BUTTON);

    UiButton* button = &item->button;

    UiTheme* theme = &context->theme;

    Float4 non_hovered_colour;
    Float4 hovered_colour;
    Float3 text_colour;
    if(button->enabled)
    {
        non_hovered_colour = theme->colours.button_cap_enabled;
        hovered_colour = theme->colours.button_cap_hovered_enabled;
        text_colour = theme->colours.button_label_enabled;
    }
    else
    {
        non_hovered_colour = theme->colours.button_cap_disabled;
        hovered_colour = theme->colours.button_cap_hovered_disabled;
        text_colour = theme->colours.button_label_disabled;
    }

    Float4 colour;
    if(button->hovered)
    {
        colour = hovered_colour;
    }
    else
    {
        colour = non_hovered_colour;
    }
    immediate_draw_opaque_rect(item->bounds, colour);

    immediate_set_text_colour(text_colour);
    draw_text_block(&item->button.text_block, item->bounds);
}

static void draw_container(UiItem* item, UiContext* context)
{
    ASSERT(item->type == UI_ITEM_TYPE_CONTAINER);

    int style_index = (int) item->container.style_type;
    UiStyle style = context->theme.styles[style_index];

    immediate_draw_opaque_rect(item->bounds, style.background);

    for(int i = 0; i < item->container.items_count; i += 1)
    {
        ui_draw(&item->container.items[i], context);
    }
}

static void place_glyphs(UiTextBlock* text_block, Float2 bounds, UiContext* context)
{
    measure_bound_dimensions_text_block(text_block, bounds, context);
}

static void place_list_items(UiItem* item, UiContext* context)
{
    UiList* list = &item->list;

    if(list->items_count > 0)
    {
        float line_height = context->theme.font->line_height;

        float item_height = get_list_item_height(list, &list->items[0], line_height);
        Float2 top_left = rect_top_left(item->bounds);

        Rect place;
        place.bottom_left.x = list->side_margin + top_left.x;
        place.bottom_left.y = -item_height + top_left.y;
        place.dimensions.x = item->bounds.dimensions.x - (2.0f * list->side_margin);
        place.dimensions.y = item_height - list->item_spacing;

        for(int i = 0; i < list->items_count; i += 1)
        {
            list->items_bounds[i] = place;
            place.bottom_left.y -= place.dimensions.y + list->item_spacing;
            place_glyphs(&list->items[i], list->items_bounds[i].dimensions, context);
        }
    }
}

static void draw_list(UiItem* item, UiContext* context)
{
    ASSERT(item->type == UI_ITEM_TYPE_LIST);

    place_list_items(item, context);

    immediate_set_clip_area(item->bounds, (int) context->viewport.x, (int) context->viewport.y);

    UiList* list = &item->list;

    UiTheme* theme = &context->theme;
    float line_height = theme->font->line_height;
    Float4 hover_colour = theme->colours.list_item_background_hovered;
    Float4 selection_colour = theme->colours.list_item_background_selected;

    if(list->items_count > 0)
    {
        // Draw the hover highlight for the items.
        if(is_valid_index(list->hovered_item_index) && list->hovered_item_index != list->selected_item_index)
        {
            Rect rect = list->items_bounds[list->hovered_item_index];
            rect.bottom_left.y += list->scroll_top;
            immediate_draw_transparent_rect(rect, hover_colour);
        }

        // Draw the selection highlight for the records.
        if(is_valid_index(list->selected_item_index))
        {
            Rect rect = list->items_bounds[list->selected_item_index];
            rect.bottom_left.y += list->scroll_top;
            immediate_draw_transparent_rect(rect, selection_colour);
        }

        // Draw each item's contents.
        float item_height = get_list_item_height(list, &list->items[0], line_height);
        float scroll = list->scroll_top;
        int start_index = (int) (scroll / item_height);
        start_index = MAX(start_index, 0);
        int end_index = (int) ceilf((scroll + item->bounds.dimensions.y) / item_height);
        end_index = MIN(end_index, list->items_count);

        for(int i = start_index; i < end_index; i += 1)
        {
            UiTextBlock* next = &list->items[i];
            Rect bounds = list->items_bounds[i];
            bounds.bottom_left.y += list->scroll_top;
            draw_text_block(next, bounds);
        }
    }

    immediate_stop_clip_area();
}

static Float2 compute_cursor_position(UiTextBlock* text_block, Float2 dimensions, float line_height, int index)
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

static int find_index_at_position(UiTextBlock* text_block, Float2 dimensions, float line_height, Float2 position)
{
    int index = invalid_index;

    float closest = infinity;
    int count = text_block->glyphs_count;
    for(int i = 0; i < count; i += 1)
    {
        UiGlyph glyph = text_block->glyphs[i];
        Float2 point = glyph.baseline_start;

        if(position.y >= point.y + line_height)
        {
            break;
        }
        else if(position.y >= point.y)
        {
            float distance = fabsf(point.x - position.x);
            if(distance < closest)
            {
                closest = distance;
                index = glyph.text_index;
            }
        }
    }

    UiGlyph last = text_block->glyphs[count - 1];
    Float2 point;
    point.x = last.baseline_start.x + last.x_advance;
    point.y = last.baseline_start.y;
    if(position.y < point.y + line_height && position.y > point.y)
    {
        float distance = fabsf(point.x - position.x);
        if(distance < closest)
        {
            closest = distance;
            index = string_size(text_block->text);
        }
    }

    return index;
}

static void draw_text_input(UiItem* item, UiContext* context)
{
    ASSERT(item->type == UI_ITEM_TYPE_TEXT_INPUT);

    Float4 selection_colour = context->theme.colours.text_input_selection;
    Float4 cursor_colour = context->theme.colours.text_input_cursor;
    float line_height = context->theme.font->line_height;

    UiTextInput* text_input = &item->text_input;
    UiTextBlock* text_block = &text_input->text_block;

    bool in_focus = ui_focused_on(context, item);

    if(in_focus || text_block->glyphs_count > 0)
    {
        // Show the text instead of the label.
        draw_text_block(text_block, item->bounds);
    }
    else
    {
        // Show the label only when the field isn't focused. If it's been
        // edited keep showing the entered text even if focus leaves.
        draw_text_block(&text_input->label, item->bounds);
    }

    // Draw the cursor.
    if(in_focus)
    {
        Float2 cursor = compute_cursor_position(text_block, item->bounds.dimensions, line_height, text_input->cursor_position);
        Float2 top_left = rect_top_left(item->bounds);
        cursor = float2_add(cursor, top_left);

        Rect rect;
        rect.dimensions = (Float2){{1.7f, line_height}};
        rect.bottom_left = cursor;

        text_input->cursor_blink_frame = (text_input->cursor_blink_frame + 1) & 0x3f;
        if((text_input->cursor_blink_frame / 32) & 1)
        {
            immediate_draw_opaque_rect(rect, cursor_colour);
        }

        // Draw the selection highlight.
        if(text_input->cursor_position != text_input->selection_start)
        {
            Float2 start = compute_cursor_position(text_block, item->bounds.dimensions, line_height, text_input->selection_start);
            start = float2_add(start, top_left);

            Float2 first, second;
            if(text_input->selection_start < text_input->cursor_position)
            {
                first = start;
                second = cursor;
            }
            else
            {
                first = cursor;
                second = start;
            }

            if(almost_equals(cursor.y, start.y))
            {
                // The selection endpoints are on the same line.
                rect.bottom_left = first;
                rect.dimensions.x = second.x - first.x;
                rect.dimensions.y = line_height;
                immediate_draw_transparent_rect(rect, selection_colour);
            }
            else
            {
                // The selection endpoints are on different lines.
                UiPadding padding = text_block->padding;
                float left = item->bounds.bottom_left.x + padding.start;
                float right = left + item->bounds.dimensions.x - padding.end;

                rect.bottom_left = first;
                rect.dimensions.x = right - first.x;
                rect.dimensions.y = line_height;
                immediate_add_rect(rect, selection_colour);

                int between_lines = (int) ((first.y - second.y) / line_height) - 1;
                for(int i = 0; i < between_lines; i += 1)
                {
                    rect.dimensions.x = right - left;
                    rect.bottom_left.x = left;
                    rect.bottom_left.y = first.y - (line_height * (i + 1));
                    immediate_add_rect(rect, selection_colour);
                }

                rect.dimensions.x = second.x - left;
                rect.bottom_left.x = left;
                rect.bottom_left.y = second.y;
                immediate_add_rect(rect, selection_colour);

                immediate_set_blend_mode(BLEND_MODE_TRANSPARENT);
                immediate_draw();
            }
        }
    }
}

void ui_draw(UiItem* item, UiContext* context)
{
    switch(item->type)
    {
        case UI_ITEM_TYPE_BUTTON:
        {
            draw_button(item, context);
            break;
        }
        case UI_ITEM_TYPE_CONTAINER:
        {
            draw_container(item, context);
            break;
        }
        case UI_ITEM_TYPE_LIST:
        {
            draw_list(item, context);
            break;
        }
        case UI_ITEM_TYPE_TEXT_BLOCK:
        {
            draw_text_block(&item->text_block, item->bounds);
            break;
        }
        case UI_ITEM_TYPE_TEXT_INPUT:
        {
            draw_text_input(item, context);
            break;
        }
    }
}

void ui_draw_focus_indicator(UiItem* item, UiContext* context)
{
    if(context->focused_item && in_focus_scope(item, context->focused_item))
    {
        UiItem* focused = context->focused_item;

        Float4 colour = context->theme.colours.focus_indicator;
        const float line_width = 2.0f;

        Rect bounds = focused->bounds;

        float width = bounds.dimensions.x;
        float height = bounds.dimensions.y;

        Float2 top_left = rect_top_left(bounds);
        Float2 bottom_left = bounds.bottom_left;
        Float2 bottom_right = rect_bottom_right(bounds);

        Rect top;
        top.bottom_left = top_left;
        top.dimensions = (Float2){{width, line_width}};

        Rect bottom;
        bottom.bottom_left = (Float2){{bottom_left.x, bottom_left.y - line_width}};
        bottom.dimensions = (Float2){{width, line_width}};

        Rect left;
        left.bottom_left = (Float2){{bottom_left.x - line_width, bottom_left.y - line_width}};
        left.dimensions = (Float2){{line_width, height + (2 * line_width)}};

        Rect right;
        right.bottom_left = (Float2){{bottom_right.x, bottom_right.y - line_width}};
        right.dimensions = (Float2){{line_width, height + (2 * line_width)}};

        immediate_draw_transparent_rect(top, colour);
        immediate_draw_transparent_rect(bottom, colour);
        immediate_draw_transparent_rect(left, colour);
        immediate_draw_transparent_rect(right, colour);
    }
}

static bool detect_hover(UiItem* item, Float2 pointer_position, Platform* platform)
{
    bool detected = false;

    switch(item->type)
    {
        case UI_ITEM_TYPE_BUTTON:
        {
            UiButton* button = &item->button;
            bool hovered = point_in_rect(item->bounds, pointer_position);
            button->hovered = hovered;
            if(hovered)
            {
                if(button->enabled)
                {
                    change_cursor(platform, CURSOR_TYPE_ARROW);
                }
                else
                {
                    change_cursor(platform, CURSOR_TYPE_PROHIBITION_SIGN);
                }
            }
            detected = hovered;
            break;
        }
        case UI_ITEM_TYPE_CONTAINER:
        {
            UiContainer* container = &item->container;
            for(int i = 0; i < container->items_count; i += 1)
            {
                UiItem* inside = &container->items[i];
                detected |= detect_hover(inside, pointer_position, platform);
            }
            break;
        }
        case UI_ITEM_TYPE_LIST:
        {
            UiList* list = &item->list;
            list->hovered_item_index = invalid_index;
            for(int i = 0; i < list->items_count; i += 1)
            {
                Rect bounds = list->items_bounds[i];
                bounds.bottom_left.y += list->scroll_top;
                clip_rects(bounds, item->bounds, &bounds);
                bool hovered = point_in_rect(bounds, pointer_position);
                if(hovered)
                {
                    list->hovered_item_index = i;
                    change_cursor(platform, CURSOR_TYPE_ARROW);
                    detected = true;
                }
            }
            break;
        }
        case UI_ITEM_TYPE_TEXT_BLOCK:
        {
            break;
        }
        case UI_ITEM_TYPE_TEXT_INPUT:
        {
            bool hovered = point_in_rect(item->bounds, pointer_position);
            if(hovered)
            {
                change_cursor(platform, CURSOR_TYPE_I_BEAM);
            }
            detected = hovered;
            break;
        }
    }

    return detected;
}

static bool detect_focus_changes(UiContext* context, UiItem* item, Float2 mouse_position)
{
    bool focus_taken;

    switch(item->type)
    {
        case UI_ITEM_TYPE_BUTTON:
        {
            ui_focus_on_item(context, item);
            focus_taken = true;
            break;
        }
        case UI_ITEM_TYPE_CONTAINER:
        {
            UiContainer* container = &item->container;

            focus_taken = false;
            for(int i = 0; i < container->items_count; i += 1)
            {
                UiItem* inside = &container->items[i];

                if(point_in_rect(inside->bounds, mouse_position))
                {
                    focus_taken = detect_focus_changes(context, inside, mouse_position);
                    if(focus_taken)
                    {
                        break;
                    }
                }
            }
            break;
        }
        case UI_ITEM_TYPE_LIST:
        {
            ui_focus_on_item(context, item);
            focus_taken = true;
            break;
        }
        case UI_ITEM_TYPE_TEXT_BLOCK:
        {
            focus_taken = false;
            break;
        }
        case UI_ITEM_TYPE_TEXT_INPUT:
        {
            ui_focus_on_item(context, item);
            focus_taken = true;
            break;
        }
    }

    return focus_taken;
}

static void detect_focus_changes_for_toplevel_containers(UiContext* context)
{
    bool clicked = input_get_mouse_clicked(MOUSE_BUTTON_LEFT)
            || input_get_mouse_clicked(MOUSE_BUTTON_MIDDLE)
            || input_get_mouse_clicked(MOUSE_BUTTON_RIGHT);

    Float2 mouse_position;
    Int2 position = input_get_mouse_position();
    mouse_position.x = position.x - context->viewport.x / 2.0f;
    mouse_position.y = -(position.y - context->viewport.y / 2.0f);

    UiItem* highest = NULL;
    FOR_ALL(UiItem*, context->toplevel_containers)
    {
        UiItem* item = *it;
        bool above = point_in_rect(item->bounds, mouse_position);
        if(above)
        {
            highest = item;
        }
    }
    if(clicked)
    {
        if(highest)
        {
            detect_focus_changes(context, highest, mouse_position);
        }
        else
        {
            ui_focus_on_item(context, NULL);
        }
    }
}

static bool detect_capture_changes(UiContext* context, UiItem* item, Float2 mouse_position)
{
    bool captured = false;

    switch(item->type)
    {
        case UI_ITEM_TYPE_BUTTON:
        {
            ui_capture(context, item);
            captured = true;
            break;
        }
        case UI_ITEM_TYPE_CONTAINER:
        {
            UiContainer* container = &item->container;
            for(int i = 0; i < container->items_count; i += 1)
            {
                UiItem* inside = &container->items[i];

                if(point_in_rect(inside->bounds, mouse_position))
                {
                    captured = detect_capture_changes(context, inside, mouse_position);
                    if(captured)
                    {
                        break;
                    }
                }
            }

            if(!captured)
            {
                ui_capture(context, item);
                captured = true;
            }
            break;
        }
        case UI_ITEM_TYPE_LIST:
        {
            ui_capture(context, item);
            captured = true;
            break;
        }
        case UI_ITEM_TYPE_TEXT_BLOCK:
        {
            ui_capture(context, item);
            captured = true;
            break;
        }
        case UI_ITEM_TYPE_TEXT_INPUT:
        {
            ui_capture(context, item);
            captured = true;
            break;
        }
    }

    return captured;
}

static void detect_capture_changes_for_toplevel_containers(UiContext* context, Platform* platform)
{
    bool clicked = input_get_mouse_clicked(MOUSE_BUTTON_LEFT)
            || input_get_mouse_clicked(MOUSE_BUTTON_MIDDLE)
            || input_get_mouse_clicked(MOUSE_BUTTON_RIGHT);

    Float2 mouse_position;
    Int2 position = input_get_mouse_position();
    mouse_position.x = position.x - context->viewport.x / 2.0f;
    mouse_position.y = -(position.y - context->viewport.y / 2.0f);

    UiItem* highest = NULL;
    FOR_ALL(UiItem*, context->toplevel_containers)
    {
        UiItem* item = *it;
        bool above = point_in_rect(item->bounds, mouse_position);
        if(above)
        {
            highest = item;
        }
    }
    if(highest)
    {
        bool hovered = detect_hover(highest, mouse_position, platform);
        context->anything_hovered = hovered;

        if(clicked)
        {
            detect_capture_changes(context, highest, mouse_position);
        }
    }
    else
    {
        if(clicked)
        {
            ui_capture(context, NULL);
        }
        context->anything_hovered = false;
    }
}

static void signal_text_change(UiContext* context, UiId id)
{
    UiEvent event = {0};
    event.type = UI_EVENT_TYPE_TEXT_CHANGE;
    event.text_change.id = id;
    enqueue(&context->queue, event);
}

static void update_removed_glyphs(UiTextBlock* text_block, Float2 dimensions, UiId id, UiContext* context)
{
    place_glyphs(text_block, dimensions, context);
    signal_text_change(context, id);
}

static void update_added_glyphs(UiTextBlock* text_block, Float2 dimensions, UiId id, UiContext* context)
{
    // @Incomplete: There isn't a one-to-one mapping between chars and glyphs.
    // UiGlyph count should be based on the mapping of the given text in a
    // particular font instead of the size in bytes.
    Heap* heap = context->heap;
    int glyphs_cap = string_size(text_block->text);
    text_block->glyphs = HEAP_REALLOCATE(heap, text_block->glyphs, UiGlyph, glyphs_cap);
    text_block->glyphs_cap = glyphs_cap;
    map_destroy(&text_block->glyph_map, heap);
    map_create(&text_block->glyph_map, glyphs_cap, heap);

    place_glyphs(text_block, dimensions, context);
    signal_text_change(context, id);
}

static void update_cursor_position(UiTextInput* text_input, Rect bounds, UiContext* context, Platform* platform)
{
    BmfFont* font = context->theme.font;
    float line_height = font->line_height;
    Float2 viewport = context->viewport;

    int index = text_input->cursor_position;
    if(is_valid_index(index))
    {
        // Get the position within the item and determine the corresponding
        // point in the viewport.
        Float2 position = compute_cursor_position(&text_input->text_block, bounds.dimensions, line_height, index);
        position = float2_add(position, bounds.bottom_left);
        position.x += viewport.x / 2.0f;
        position.y = (viewport.y / 2.0f) - position.y;

        // Set the input method to anchor itself nearby.
        int x = (int) position.x;
        int y = (int) position.y;
        set_composed_text_position(platform, x, y);
    }
}

static void remove_selected_text(UiTextInput* text_input, Float2 dimensions, UiId id, UiContext* context, Platform* platform)
{
    if(text_input->cursor_position != text_input->selection_start)
    {
        remove_substring(text_input->text_block.text, text_input->selection_start, text_input->cursor_position);
        update_removed_glyphs(&text_input->text_block, dimensions, id, context);

        int collapsed = MIN(text_input->selection_start, text_input->cursor_position);
        text_input->cursor_position = collapsed;
        text_input->selection_start = collapsed;
    }
}

void ui_insert_text(UiItem* item, const char* text_to_add, UiContext* context, Platform* platform)
{
    UiTextInput* text_input = &item->text_input;
    UiTextBlock* text_block = &text_input->text_block;
    Float2 dimensions = item->bounds.dimensions;

    int text_to_add_size = string_size(text_to_add);
    if(text_to_add_size > 0)
    {
        remove_selected_text(text_input, dimensions, item->id, context, platform);

        int insert_index = text_input->cursor_position;
        char* text = insert_string(text_block->text, text_to_add, insert_index, context->heap);
        HEAP_DEALLOCATE(context->heap, text_block->text);
        text_block->text = text;

        update_added_glyphs(text_block, dimensions, item->id, context);

        // Advance the cursor past the inserted text.
        int text_end = string_size(text_block->text);
        text_input->cursor_position = MIN(text_input->cursor_position + text_to_add_size, text_end);
        text_input->selection_start = text_input->cursor_position;
        update_cursor_position(text_input, item->bounds, context, platform);
    }
}

static bool copy_selected_text(UiTextInput* text_input, Platform* platform, Heap* heap)
{
    UiTextBlock* text_block = &text_input->text_block;

    int start = MIN(text_input->cursor_position, text_input->selection_start);
    int end = MAX(text_input->cursor_position, text_input->selection_start);
    int size = end - start;

    char* clipboard = copy_chars_to_heap(&text_block->text[start], size, heap);
    bool copied = copy_to_clipboard(platform, clipboard);
    if(!copied)
    {
        HEAP_DEALLOCATE(heap, clipboard);
    }

    return copied;
}

static int find_beginning_of_line(UiTextBlock* text_block, int start_index)
{
    start_index = MIN(start_index, string_size(text_block->text) - 1);

    void* key = (void*) (uintptr_t) start_index;
    void* value;
    bool found = map_get(&text_block->glyph_map, key, &value);
    if(found)
    {
        uintptr_t glyph_index = (uintptr_t) value;
        UiGlyph first = text_block->glyphs[glyph_index];
        float first_y = first.baseline_start.y;
        for(uintptr_t i = glyph_index - 1; i >= 0; i -= 1)
        {
            UiGlyph glyph = text_block->glyphs[i];
            if(glyph.baseline_start.y > first_y)
            {
                return text_block->glyphs[i + 1].text_index;
            }
        }
    }

    return 0;
}

static int find_end_of_line(UiTextBlock* text_block, int start_index)
{
    int end_of_text = string_size(text_block->text);
    start_index = MIN(start_index, end_of_text - 1);

    void* key = (void*) (uintptr_t) start_index;
    void* value;
    bool found = map_get(&text_block->glyph_map, key, &value);
    if(found)
    {
        uintptr_t glyph_index = (uintptr_t) value;
        UiGlyph first = text_block->glyphs[glyph_index];
        float first_y = first.baseline_start.y;
        for(uintptr_t i = glyph_index + 1; i < text_block->glyphs_count; i += 1)
        {
            UiGlyph glyph = text_block->glyphs[i];
            if(glyph.baseline_start.y < first_y)
            {
                return text_block->glyphs[i - 1].text_index;
            }
        }
    }

    return end_of_text;
}

static void update_keyboard_input(UiItem* item, UiContext* context, Platform* platform)
{
    switch(item->type)
    {
        case UI_ITEM_TYPE_BUTTON:
        {
            UiButton* button = &item->button;

            bool activated = input_get_key_tapped(INPUT_KEY_SPACE)
                    || input_get_key_tapped(INPUT_KEY_ENTER);

            if(activated && button->enabled)
            {
                UiEvent event;
                event.type = UI_EVENT_TYPE_BUTTON;
                event.button.id = item->id;
                enqueue(&context->queue, event);
            }
            break;
        }
        case UI_ITEM_TYPE_CONTAINER:
        {
            UiContainer* container = &item->container;
            for(int i = 0; i < container->items_count; i += 1)
            {
                update_keyboard_input(&container->items[i], context, platform);
            }
            break;
        }
        case UI_ITEM_TYPE_LIST:
        {
            UiList* list = &item->list;
            int count = list->items_count;

            bool selection_changed = false;
            bool expand = false;

            if(count > 0)
            {
                if(input_get_key_auto_repeated(INPUT_KEY_DOWN_ARROW))
                {
                    list->selected_item_index = (list->selected_item_index + 1) % count;
                    selection_changed = true;
                }

                if(input_get_key_auto_repeated(INPUT_KEY_UP_ARROW))
                {
                    list->selected_item_index = mod(list->selected_item_index - 1, count);
                    selection_changed = true;
                }

                if(input_get_key_tapped(INPUT_KEY_HOME))
                {
                    list->selected_item_index = 0;
                    selection_changed = true;
                }

                if(input_get_key_tapped(INPUT_KEY_END))
                {
                    list->selected_item_index = count - 1;
                    selection_changed = true;
                }

                float item_height = list->items_bounds[0].dimensions.y + list->item_spacing;
                int items_per_page = (int) roundf(item->bounds.dimensions.y / item_height);

                if(input_get_key_tapped(INPUT_KEY_PAGE_UP))
                {
                    list->selected_item_index = MAX(list->selected_item_index - items_per_page, 0);
                    selection_changed = true;
                }

                if(input_get_key_tapped(INPUT_KEY_PAGE_DOWN))
                {
                    list->selected_item_index = MIN(list->selected_item_index + items_per_page, count - 1);
                    selection_changed = true;
                }
            }

            if((input_get_key_tapped(INPUT_KEY_SPACE) || input_get_key_tapped(INPUT_KEY_ENTER)) && is_valid_index(list->selected_item_index))
            {
                selection_changed = true;
                expand = true;
            }

            if(selection_changed)
            {
                UiEvent event;
                event.type = UI_EVENT_TYPE_LIST_SELECTION;
                event.list_selection.index = list->selected_item_index;
                event.list_selection.expand = expand;
                enqueue(&context->queue, event);
            }

            if(is_valid_index(list->selected_item_index))
            {
                ASSERT(list->items_count > 0);

                int index = list->selected_item_index;
                float item_height = list->items_bounds[0].dimensions.y + list->item_spacing;
                float item_top = (item_height * index) + list->item_spacing;

                float page_height = item->bounds.dimensions.y;
                float scroll_top = list->scroll_top;
                float from_page_edge = 3.0f * item_height;

                float window_top = scroll_top + from_page_edge;
                float window_bottom = scroll_top + page_height - from_page_edge;

                float above_window = item_top - window_top;
                float below_window = item_top - window_bottom;

                const float factor = 0.2f;
                const float min_speed = 4.0f;

                float line_height = context->theme.font->line_height;

                if(above_window < 0.0f)
                {
                    float velocity = (factor * above_window) - min_speed;
                    scroll_top += fmaxf(velocity, above_window);
                    set_scroll(item, scroll_top, line_height);
                }
                else if(below_window > 0.0f)
                {
                    float velocity = (factor * below_window) + min_speed;
                    scroll_top += fminf(velocity, below_window);
                    set_scroll(item, scroll_top, line_height);
                }
            }
            break;
        }
        case UI_ITEM_TYPE_TEXT_BLOCK:
        {
            break;
        }
        case UI_ITEM_TYPE_TEXT_INPUT:
        {
            UiTextInput* text_input = &item->text_input;
            UiTextBlock* text_block = &text_input->text_block;
            float line_height = context->theme.font->line_height;

            // Type out any new text.
            char* text_to_add = input_get_composed_text();
            ui_insert_text(item, text_to_add, context, platform);

            // Record the cursor position before any movement so that change can
            // be detected at a single point after any possible cursor moves.
            int prior_cursor_position = text_input->cursor_position;

            if(input_get_key_auto_repeated(INPUT_KEY_LEFT_ARROW))
            {
                if(input_get_key_modified_by_control(INPUT_KEY_LEFT_ARROW))
                {
                    text_input->cursor_position = find_prior_beginning_of_word(text_block->text, text_input->cursor_position, context->scratch);
                }
                else
                {
                    text_input->cursor_position = find_prior_beginning_of_grapheme_cluster(text_block->text, text_input->cursor_position, context->scratch);
                }

                if(!input_get_key_modified_by_shift(INPUT_KEY_LEFT_ARROW))
                {
                    text_input->selection_start = text_input->cursor_position;
                }
            }

            if(input_get_key_auto_repeated(INPUT_KEY_RIGHT_ARROW))
            {
                if(input_get_key_modified_by_control(INPUT_KEY_RIGHT_ARROW))
                {
                    text_input->cursor_position = find_next_end_of_word(text_block->text, text_input->cursor_position, context->scratch);
                }
                else
                {
                    text_input->cursor_position = find_next_end_of_grapheme_cluster(text_block->text, text_input->cursor_position, context->scratch);
                }

                if(!input_get_key_modified_by_shift(INPUT_KEY_RIGHT_ARROW))
                {
                    text_input->selection_start = text_input->cursor_position;
                }
            }

            if(input_get_key_auto_repeated(INPUT_KEY_UP_ARROW))
            {
                Float2 position = compute_cursor_position(text_block, item->bounds.dimensions, line_height, text_input->cursor_position);
                position.y += line_height;
                int index = find_index_at_position(text_block, item->bounds.dimensions, line_height, position);

                if(index != invalid_index)
                {
                    text_input->cursor_position = index;

                    if(!input_get_key_modified_by_shift(INPUT_KEY_UP_ARROW))
                    {
                        text_input->selection_start = text_input->cursor_position;
                    }
                }
            }

            if(input_get_key_auto_repeated(INPUT_KEY_DOWN_ARROW))
            {
                Float2 position = compute_cursor_position(text_block, item->bounds.dimensions, line_height, text_input->cursor_position);
                position.y -= line_height;
                int index = find_index_at_position(text_block, item->bounds.dimensions, line_height, position);

                if(index != invalid_index)
                {
                    text_input->cursor_position = index;

                    if(!input_get_key_modified_by_shift(INPUT_KEY_DOWN_ARROW))
                    {
                        text_input->selection_start = text_input->cursor_position;
                    }
                }
            }

            if(input_get_key_tapped(INPUT_KEY_HOME))
            {
                if(input_get_key_modified_by_control(INPUT_KEY_HOME))
                {
                    text_input->cursor_position = 0;
                }
                else
                {
                    text_input->cursor_position = find_beginning_of_line(text_block, text_input->cursor_position);
                }
                if(!input_get_key_modified_by_shift(INPUT_KEY_HOME))
                {
                    text_input->selection_start = text_input->cursor_position;
                }
            }

            if(input_get_key_tapped(INPUT_KEY_END))
            {
                if(input_get_key_modified_by_control(INPUT_KEY_END))
                {
                    text_input->cursor_position = string_size(text_block->text);
                }
                else
                {
                    text_input->cursor_position = find_end_of_line(text_block, text_input->cursor_position);
                }
                if(!input_get_key_modified_by_shift(INPUT_KEY_END))
                {
                    text_input->selection_start = text_input->cursor_position;
                }
            }

            if(input_get_key_auto_repeated(INPUT_KEY_DELETE))
            {
                int start;
                int end;
                if(text_input->cursor_position == text_input->selection_start)
                {
                    end = text_input->cursor_position;
                    int size = string_size(text_block->text);
                    int next = utf8_skip_to_next_codepoint(text_block->text, size, end + 1);
                    if(next == invalid_index)
                    {
                        start = size;
                    }
                    else
                    {
                        start = next;
                    }
                }
                else
                {
                    start = text_input->selection_start;
                    end = text_input->cursor_position;
                }
                remove_substring(text_block->text, start, end);
                update_removed_glyphs(text_block, item->bounds.dimensions, item->id, context);

                // Set the cursor to the beginning of the selection.
                int new_position = MIN(start, end);
                text_input->cursor_position = new_position;
                text_input->selection_start = new_position;
            }

            if(input_get_key_auto_repeated(INPUT_KEY_BACKSPACE))
            {
                int start;
                int end;
                if(text_input->cursor_position == text_input->selection_start)
                {
                    end = text_input->cursor_position;
                    int prior = utf8_skip_to_prior_codepoint(text_block->text, end - 1);
                    start = MAX(prior, 0);
                }
                else
                {
                    start = text_input->selection_start;
                    end = text_input->cursor_position;
                }
                remove_substring(text_block->text, start, end);
                update_removed_glyphs(text_block, item->bounds.dimensions, item->id, context);

                // Retreat the cursor or set it to the beginning of the selection.
                int new_position = MIN(start, end);
                text_input->cursor_position = new_position;
                text_input->selection_start = new_position;
            }

            if(input_get_hotkey_tapped(INPUT_FUNCTION_SELECT_ALL))
            {
                text_input->cursor_position = 0;
                text_input->selection_start = string_size(text_block->text);
            }

            if(input_get_hotkey_tapped(INPUT_FUNCTION_COPY))
            {
                copy_selected_text(text_input, platform, context->heap);
            }

            if(input_get_hotkey_tapped(INPUT_FUNCTION_CUT))
            {
                bool copied = copy_selected_text(text_input, platform, context->heap);
                if(copied)
                {
                    remove_selected_text(text_input, item->bounds.dimensions, item->id, context, platform);
                }
            }

            if(input_get_hotkey_tapped(INPUT_FUNCTION_PASTE))
            {
                request_paste_from_clipboard(platform);
            }

            // Update the input method with the current cursor position if it's
            // been moved.
            if(prior_cursor_position != text_input->cursor_position)
            {
                update_cursor_position(text_input, item->bounds, context, platform);
            }

            break;
        }
    }
}

static void build_tab_navigation_list_for_container(UiContext* context, UiItem* at)
{
    UiContainer* container = &at->container;

    for(int i = 0; i < container->items_count; i += 1)
    {
        UiItem* item = &container->items[i];

        switch(item->type)
        {
            case UI_ITEM_TYPE_BUTTON:
            {
                ARRAY_ADD(context->tab_navigation_list, item, context->heap);
                break;
            }
            case UI_ITEM_TYPE_CONTAINER:
            {
                build_tab_navigation_list_for_container(context, item);
                break;
            }
            case UI_ITEM_TYPE_LIST:
            {
                ARRAY_ADD(context->tab_navigation_list, item, context->heap);
                break;
            }
            case UI_ITEM_TYPE_TEXT_BLOCK:
            {
                break;
            }
            case UI_ITEM_TYPE_TEXT_INPUT:
            {
                ARRAY_ADD(context->tab_navigation_list, item, context->heap);
                break;
            }
        }
    }
}

static void build_tab_navigation_list(UiContext* context)
{
    ARRAY_DESTROY(context->tab_navigation_list, context->heap);

    FOR_ALL(UiItem*, context->toplevel_containers)
    {
        UiItem* toplevel = *it;
        build_tab_navigation_list_for_container(context, toplevel);
    }
}

static void update_non_item_specific_keyboard_input(UiContext* context)
{
    if(input_get_key_auto_repeated(INPUT_KEY_TAB))
    {
        bool backward = input_get_key_modified_by_shift(INPUT_KEY_TAB);

        ASSERT(context->tab_navigation_list);
        ASSERT(array_count(context->tab_navigation_list) > 0);

        if(!context->focused_item)
        {
            if(backward)
            {
                ui_focus_on_item(context, ARRAY_LAST(context->tab_navigation_list));
            }
            else
            {
                ui_focus_on_item(context, context->tab_navigation_list[0]);
            }
        }
        else
        {
            int found_index = invalid_index;
            int count = array_count(context->tab_navigation_list);

            for(int i = 0; i < count; i += 1)
            {
                UiItem* item = context->tab_navigation_list[i];
                if(item->id == context->focused_item->id)
                {
                    found_index = i;
                    break;
                }
            }

            if(found_index != invalid_index)
            {
                int index;
                if(backward)
                {
                    index = mod(found_index - 1, count);
                }
                else
                {
                    index = (found_index + 1) % count;
                }
                ui_focus_on_item(context, context->tab_navigation_list[index]);
            }
        }
    }
}

static void update_pointer_input(UiItem* item, UiContext* context, Platform* platform)
{
    switch(item->type)
    {
        case UI_ITEM_TYPE_BUTTON:
        {
            UiButton* button = &item->button;
            if(button->hovered && input_get_mouse_clicked(MOUSE_BUTTON_LEFT) && button->enabled)
            {
                UiEvent event;
                event.type = UI_EVENT_TYPE_BUTTON;
                event.button.id = item->id;
                enqueue(&context->queue, event);
            }
            break;
        }
        case UI_ITEM_TYPE_CONTAINER:
        {
            UiContainer* container = &item->container;
            for(int i = 0; i < container->items_count; i += 1)
            {
                update_pointer_input(&container->items[i], context, platform);
            }
            break;
        }
        case UI_ITEM_TYPE_LIST:
        {
            UiList* list = &item->list;

            float line_height = context->theme.font->line_height;

            Int2 velocity = input_get_mouse_scroll_velocity();
            const float speed = 0.17f;
            scroll(item, speed * velocity.y, line_height);

            if(is_valid_index(list->hovered_item_index) && input_get_mouse_clicked(MOUSE_BUTTON_LEFT))
            {
                list->selected_item_index = list->hovered_item_index;

                UiEvent event;
                event.type = UI_EVENT_TYPE_LIST_SELECTION;
                event.list_selection.index = list->selected_item_index;
                event.list_selection.expand = true;
                enqueue(&context->queue, event);
            }
            break;
        }
        case UI_ITEM_TYPE_TEXT_BLOCK:
        {
            break;
        }
        case UI_ITEM_TYPE_TEXT_INPUT:
        {
            UiTextInput* text_input = &item->text_input;
            UiTextBlock* text_block = &text_input->text_block;
            float line_height = context->theme.font->line_height;

            bool clicked = input_get_mouse_clicked(MOUSE_BUTTON_LEFT);
            bool dragged = !clicked && input_get_mouse_pressed(MOUSE_BUTTON_LEFT);
            if(clicked || dragged)
            {
                Float2 mouse_position;
                Int2 position = input_get_mouse_position();
                mouse_position.x = position.x - context->viewport.x / 2.0f;
                mouse_position.y = -(position.y - context->viewport.y / 2.0f);

                Float2 top_left = rect_top_left(item->bounds);
                mouse_position = float2_subtract(mouse_position, top_left);

                int index = find_index_at_position(text_block, item->bounds.dimensions, line_height, mouse_position);
                if(index != invalid_index)
                {
                    text_input->cursor_position = index;
                    if(clicked)
                    {
                        text_input->selection_start = index;
                    }
                }
            }
            break;
        }
    }
}

void ui_update(UiContext* context, Platform* platform)
{
    // @Incomplete: Tab order doesn't need to be updated every frame, only when
    // the order or number of total items changes. Possibly during layout is a
    // better time?
    build_tab_navigation_list(context);

    detect_focus_changes_for_toplevel_containers(context);
    if(context->focused_item)
    {
        update_keyboard_input(context->focused_item, context, platform);
    }

    detect_capture_changes_for_toplevel_containers(context, platform);
    if(context->captor_item)
    {
        update_pointer_input(context->captor_item, context, platform);
    }

    update_non_item_specific_keyboard_input(context);
}

void ui_accept_paste_from_clipboard(UiContext* context, const char* clipboard, Platform* platform)
{
    if(context->focused_item)
    {
        UiItem* item = context->focused_item;
        if(item->type == UI_ITEM_TYPE_TEXT_INPUT)
        {
            ui_insert_text(item, clipboard, context, platform);
        }
    }
}
