#include "ui.h"

#include "array2.h"
#include "assert.h"
#include "colours.h"
#include "float_utilities.h"
#include "immediate.h"
#include "input.h"
#include "int_utilities.h"
#include "logging.h"
#include "loop_macros.h"
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

namespace ui {

extern const Id invalid_id = 0;

static void create_event_queue(EventQueue* queue, Heap* heap)
{
    queue->cap = 32;
    queue->events = HEAP_ALLOCATE(heap, Event, queue->cap);
}

static void destroy_event_queue(EventQueue* queue, Heap* heap)
{
    SAFE_HEAP_DEALLOCATE(heap, queue->events);
}

static void enqueue(EventQueue* queue, Event event)
{
    queue->events[queue->tail] = event;
    queue->tail = (queue->tail + 1) % queue->cap;
}

bool dequeue(EventQueue* queue, Event* event)
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

void create_context(Context* context, Heap* heap, Stack* scratch)
{
    create_event_queue(&context->queue, heap);
    context->seed = 1;
    context->heap = heap;
    context->scratch = scratch;
}

static void destroy_text_block(TextBlock* text_block, Heap* heap)
{
    SAFE_HEAP_DEALLOCATE(heap, text_block->text);
    SAFE_HEAP_DEALLOCATE(heap, text_block->glyphs);

    destroy(&text_block->glyph_map, heap);
}

static void destroy_list(List* list, Heap* heap)
{
    FOR_N(i, list->items_count)
    {
        destroy_text_block(&list->items[i], heap);
    }
    list->items_count = 0;

    SAFE_HEAP_DEALLOCATE(heap, list->items);
    SAFE_HEAP_DEALLOCATE(heap, list->items_bounds);
}

static void destroy_container(Container* container, Heap* heap)
{
    if(container)
    {
        FOR_N(i, container->items_count)
        {
            Item* item = &container->items[i];
            switch(item->type)
            {
                case ItemType::Button:
                {
                    destroy_text_block(&item->button.text_block, heap);
                    break;
                }
                case ItemType::Container:
                {
                    destroy_container(&item->container, heap);
                    break;
                }
                case ItemType::List:
                {
                    destroy_list(&item->list, heap);
                    break;
                }
                case ItemType::Text_Block:
                {
                    destroy_text_block(&item->text_block, heap);
                    break;
                }
                case ItemType::Text_Input:
                {
                    destroy_text_block(&item->text_input.text_block, heap);
                    break;
                }
            }
        }
        container->items_count = 0;

        SAFE_HEAP_DEALLOCATE(heap, container->items);
    }
}

void destroy_context(Context* context, Heap* heap)
{
    ASSERT(heap == context->heap);

    destroy_event_queue(&context->queue, heap);

    FOR_ALL(context->toplevel_containers)
    {
        Item* item = *it;
        destroy_container(&item->container, heap);
        SAFE_HEAP_DEALLOCATE(heap, item);
    }

    ARRAY_DESTROY(context->toplevel_containers, heap);
    ARRAY_DESTROY(context->tab_navigation_list, heap);
}

static Id generate_id(Context* context)
{
    Id id = context->seed;
    context->seed += 1;
    return id;
}

static bool in_focus_scope(Item* outer, Item* item)
{
    if(outer->id == item->id)
    {
        return true;
    }

    Container* container = &outer->container;

    FOR_N(i, container->items_count)
    {
        Item* inside = &container->items[i];
        if(inside->id == item->id)
        {
            return true;
        }
        if(inside->type == ItemType::Container)
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

static Id get_focus_scope(Context* context, Item* item)
{
    if(!item)
    {
        return invalid_id;
    }

    FOR_ALL(context->toplevel_containers)
    {
        Item* toplevel = *it;
        bool within = in_focus_scope(toplevel, item);
        if(within)
        {
            return toplevel->id;
        }
    }

    return invalid_id;
}

bool focused_on(Context* context, Item* item)
{
    return item == context->focused_item;
}

void focus_on_item(Context* context, Item* item)
{
    if(item != context->focused_item)
    {
        Event event = {};
        event.type = EventType::Focus_Change;
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

bool is_captor(Context* context, Item* item)
{
    return item == context->captor_item;
}

void capture(Context* context, Item* item)
{
    context->captor_item = item;
}

static bool is_capture_within(Context* context, Item* item)
{
    if(!context->captor_item)
    {
        return false;
    }

    Container* container = &item->container;
    FOR_N(i, container->items_count)
    {
        Item* inside = &container->items[i];
        if(inside->id == context->captor_item->id)
        {
            return true;
        }
        if(inside->type == ItemType::Container)
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

Item* create_toplevel_container(Context* context, Heap* heap)
{
    Item* item = HEAP_ALLOCATE(heap, Item, 1);
    item->id = generate_id(context);
    ARRAY_ADD(context->toplevel_containers, item, heap);
    return item;
}

void destroy_toplevel_container(Context* context, Item* item, Heap* heap)
{
    // If the container is currently focused, make sure it becomes unfocused.
    if(context->focused_item && in_focus_scope(item, context->focused_item))
    {
        focus_on_item(context, nullptr);
    }
    // Release capture if it's currently holding something.
    if(is_capture_within(context, item))
    {
        capture(context, nullptr);
    }

    destroy_container(&item->container, heap);

    // Find the container and overwrite it with the container on the end.
    int count = ARRAY_COUNT(context->toplevel_containers);
    FOR_N(i, count)
    {
        Item* next = context->toplevel_containers[i];
        if(next == item)
        {
            Item* last = context->toplevel_containers[count - 1];
            context->toplevel_containers[i] = last;
            break;
        }
    }

    // Only deallocate after the item is removed, because it sets the pointer to
    // null and would cause it not to be found.
    SAFE_HEAP_DEALLOCATE(heap, item);

    if(count)
    {
        Item** last = &ARRAY_LAST(context->toplevel_containers);
        ARRAY_REMOVE(context->toplevel_containers, last);
    }
}

void empty_item(Context* context, Item* item)
{
    // Release capture if it's currently holding something.
    if(is_capture_within(context, item))
    {
        capture(context, nullptr);
    }

    switch(item->type)
    {
        case ItemType::Button:
        {
            break;
        }
        case ItemType::Container:
        {
            destroy_container(&item->container, context->heap);
            break;
        }
        case ItemType::List:
        {
            destroy_list(&item->list, context->heap);
            break;
        }
        case ItemType::Text_Block:
        {
            break;
        }
        case ItemType::Text_Input:
        {
            break;
        }
    }
}

void add_row(Container* container, int count, Context* context, Heap* heap)
{
    container->axis = Axis::Horizontal;
    container->items = HEAP_ALLOCATE(heap, Item, count);
    container->items_count = count;
    FOR_N(i, count)
    {
        container->items[i].id = generate_id(context);
    }
}

void add_column(Container* container, int count, Context* context, Heap* heap)
{
    container->axis = Axis::Vertical;
    container->items = HEAP_ALLOCATE(heap, Item, count);
    container->items_count = count;
    FOR_N(i, count)
    {
        container->items[i].id = generate_id(context);
    }
}

void set_text(TextBlock* text_block, const char* text, Heap* heap)
{
    // Clean up any prior text.
    destroy(&text_block->glyph_map, heap);

    replace_string(&text_block->text, text, heap);

    // @Incomplete: There isn't a one-to-one mapping between chars and glyphs.
    // Glyph count should be based on the mapping of the given text in a
    // particular font instead of the size in bytes.
    int size = string_size(text);
    text_block->glyphs = HEAP_REALLOCATE(heap, text_block->glyphs, size);
    text_block->glyphs_cap = size;
    create(&text_block->glyph_map, size, heap);
}

// The "main" axis of a container is the one items are placed along and the
// "cross" axis is the one perpendicular to it. The main axis could also be
// called longitudinal and the cross axis could be called transverse.

static int get_main_axis_index(Axis axis)
{
    return static_cast<int>(axis);
}

static int get_cross_axis_index(Axis axis)
{
    return static_cast<int>(axis) ^ 1;
}

static float padding_along_axis(Padding padding, int axis)
{
    return padding[axis] + padding[axis + 2];
}

static float get_list_item_height(List* list, TextBlock* text_block)
{
    float spacing = list->item_spacing;
    Padding padding = text_block->padding;
    return padding.top + text_block->font->line_height + padding.bottom + spacing;
}

static float get_scroll_bottom(Item* item)
{
    float inner_height = item->bounds.dimensions.y;
    List* list = &item->list;
    float content_height = 0.0f;
    if(list->items_count > 0)
    {
        float lines = list->items_count;
        content_height = lines * get_list_item_height(list, &list->items[0]);
    }
    return fmax(content_height - inner_height, 0.0f);
}

static void set_scroll(Item* item, float scroll)
{
    float scroll_bottom = get_scroll_bottom(item);
    item->list.scroll_top = clamp(scroll, 0.0f, scroll_bottom);
}

static void scroll(Item* item, float scroll_velocity_y)
{
    const float speed = 120.0f;
    float scroll_top = item->list.scroll_top - (speed * scroll_velocity_y);
    set_scroll(item, scroll_top);
}

void create_items(Item* item, int lines_count, Heap* heap)
{
    List* list = &item->list;
    list->items_count = lines_count;
    if(lines_count)
    {
        list->items = HEAP_ALLOCATE(heap, TextBlock, lines_count);
        list->items_bounds = HEAP_ALLOCATE(heap, Rect, lines_count);
    }
    else
    {
        list->items = nullptr;
        list->items_bounds = nullptr;
    }
    list->hovered_item_index = invalid_index;
    list->selected_item_index = invalid_index;
}

static Vector2 measure_ideal_dimensions(TextBlock* text_block, Stack* stack)
{
    Vector2 bounds = vector2_zero;

    bmfont::Font* font = text_block->font;
    Padding padding = text_block->padding;

    Vector2 texture_dimensions;
    texture_dimensions.x = font->image_width;
    texture_dimensions.y = font->image_height;

    text_block->glyphs_count = 0;
    reset_all(&text_block->glyph_map);

    char32_t prior_char = '\0';
    int size = string_size(text_block->text);
    int next_break = find_next_mandatory_line_break(text_block->text, 0, stack);
    Vector2 pen = {padding.start, -padding.top - font->line_height};
    FOR_N(i, size)
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

            bounds.x = fmax(bounds.x, pen.x);
            pen.x = padding.start;
            pen.y -= font->line_height;
        }

        bool newline = is_newline(current);

        if(!is_default_ignorable(current) && !newline)
        {
            bmfont::Glyph* glyph = bmfont::find_glyph(font, current);
            float kerning = bmfont::lookup_kerning(font, prior_char, current);

            Vector2 top_left = pen;
            top_left.x += glyph->offset.x;
            top_left.y -= glyph->offset.y;

            Rect viewport_rect;
            viewport_rect.dimensions = glyph->rect.dimensions;
            viewport_rect.bottom_left.x = top_left.x;
            viewport_rect.bottom_left.y = top_left.y + font->line_height - viewport_rect.dimensions.y;

            Rect texture_rect = glyph->rect;
            texture_rect.bottom_left = pointwise_divide(texture_rect.bottom_left, texture_dimensions);
            texture_rect.dimensions = pointwise_divide(texture_rect.dimensions, texture_dimensions);

            int glyph_index = text_block->glyphs_count;
            Glyph* typeset_glyph = &text_block->glyphs[glyph_index];
            typeset_glyph->rect = viewport_rect;
            typeset_glyph->texture_rect = texture_rect;
            typeset_glyph->baseline_start = pen;
            typeset_glyph->x_advance = glyph->x_advance;
            typeset_glyph->text_index = text_index;
            text_block->glyphs_count += 1;

            insert(&text_block->glyph_map, text_index, glyph_index);

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
            Glyph* typeset_glyph = &text_block->glyphs[glyph_index];
            typeset_glyph->rect = viewport_rect;
            typeset_glyph->texture_rect = {{0.0f, 0.0f}, {0.0f, 0.0f}};
            typeset_glyph->baseline_start = pen;
            typeset_glyph->x_advance = 0.0f;
            typeset_glyph->text_index = text_index;
            text_block->glyphs_count += 1;

            insert(&text_block->glyph_map, text_index, glyph_index);
        }

        prior_char = current;
        i = text_index;
    }

    pen.y = abs(pen.y);
    bounds = max2(bounds, pen);
    bounds.x += padding.bottom;
    bounds.y += padding.end;

    return bounds;
}

static Vector2 measure_ideal_dimensions(Container* container, Stack* stack)
{
    Vector2 bounds = vector2_zero;
    int main_axis = get_main_axis_index(container->axis);
    int cross_axis = get_cross_axis_index(container->axis);

    FOR_N(i, container->items_count)
    {
        Item* item = &container->items[i];
        Vector2 item_ideal;
        switch(item->type)
        {
            case ItemType::Button:
            {
                item_ideal = measure_ideal_dimensions(&item->button.text_block, stack);
                break;
            }
            case ItemType::Container:
            {
                item_ideal = measure_ideal_dimensions(&item->container, stack);
                break;
            }
            case ItemType::List:
            {
                item_ideal = vector2_max;
                break;
            }
            case ItemType::Text_Block:
            {
                item_ideal = measure_ideal_dimensions(&item->text_block, stack);
                break;
            }
            case ItemType::Text_Input:
            {
                item_ideal = measure_ideal_dimensions(&item->text_input.text_block, stack);
                break;
            }
        }
        item->ideal_dimensions = max2(item_ideal, item->min_dimensions);

        bounds[main_axis] += item->ideal_dimensions[main_axis];
        bounds[cross_axis] = fmax(bounds[cross_axis], item->ideal_dimensions[cross_axis]);
    }

    Padding padding = container->padding;
    bounds.x += padding.start + padding.end;
    bounds.y += padding.top + padding.bottom;

    return bounds;
}

static float compute_run_length(const char* text, int start, int end, bmfont::Font* font)
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

        bmfont::Glyph* glyph = bmfont::find_glyph(font, current);
        length += glyph->x_advance;
        i = text_index;
    }
    return length;
}

static void place_glyph(TextBlock* text_block, bmfont::Glyph* glyph, bmfont::Font* font, int text_index, Vector2 pen, Vector2 texture_dimensions)
{
    Vector2 top_left = pen;
    top_left.x += glyph->offset.x;
    top_left.y -= glyph->offset.y;

    Rect viewport_rect;
    viewport_rect.dimensions = glyph->rect.dimensions;
    viewport_rect.bottom_left.x = top_left.x;
    viewport_rect.bottom_left.y = top_left.y + font->line_height - viewport_rect.dimensions.y;

    Rect texture_rect = glyph->rect;
    texture_rect.bottom_left = pointwise_divide(texture_rect.bottom_left, texture_dimensions);
    texture_rect.dimensions = pointwise_divide(texture_rect.dimensions, texture_dimensions);

    int glyph_index = text_block->glyphs_count;
    Glyph* typeset_glyph = &text_block->glyphs[glyph_index];
    typeset_glyph->rect = viewport_rect;
    typeset_glyph->texture_rect = texture_rect;
    typeset_glyph->baseline_start = pen;
    typeset_glyph->x_advance = glyph->x_advance;
    typeset_glyph->text_index = text_index;
    text_block->glyphs_count += 1;

    insert(&text_block->glyph_map, text_index, glyph_index);
}

static Vector2 measure_bound_dimensions(TextBlock* text_block, Vector2 dimensions, Stack* stack)
{
    Vector2 bounds = vector2_zero;

    bmfont::Font* font = text_block->font;

    Padding padding = text_block->padding;
    float right = dimensions.x - padding.end;
    float whole_width = dimensions.x - padding.start - padding.end;

    text_block->glyphs_count = 0;
    reset_all(&text_block->glyph_map);

    Vector2 texture_dimensions;
    texture_dimensions.x = font->image_width;
    texture_dimensions.y = font->image_height;

    const char* ellipsize_text = u8"…";
    float extra_length = 0.0f;
    if(text_block->text_overflow == TextOverflow::Ellipsize_End)
    {
        int size = string_size(ellipsize_text);
        extra_length = compute_run_length(ellipsize_text, 0, size, font);
    }
    bool add_ellipsis_pattern = false;

    bool mandatory_break;
    int next_break = find_next_line_break(text_block->text, 0, &mandatory_break, stack);
    float run_length = compute_run_length(text_block->text, 0, next_break, font);

    char32_t prior_char = '\0';
    Vector2 pen = {padding.start, -padding.top - font->line_height};
    int text_index = 0;
    int size = string_size(text_block->text);

    FOR_N(i, size)
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
                if(text_block->text_overflow == TextOverflow::Wrap)
                {
                    bounds.x = fmax(bounds.x, pen.x);
                    pen.x = padding.start;
                    pen.y -= font->line_height;
                }
                else if(text_block->text_overflow == TextOverflow::Ellipsize_End)
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

            bmfont::Glyph* glyph = bmfont::find_glyph(font, current);

            // If the right size is close, but a line break wasn't expected.
            if(pen.x + glyph->x_advance + extra_length > right)
            {
                if(text_block->text_overflow == TextOverflow::Wrap && run_length > whole_width)
                {
                    // If there is a long section of text with no break opportunities
                    // make an emergency break so it doesn't overflow the text box.
                    bounds.x = fmax(bounds.x, pen.x);
                    pen.x = padding.start;
                    pen.y -= font->line_height;
                }
                else if(text_block->text_overflow == TextOverflow::Ellipsize_End)
                {
                    // This is the normal end of an ellipsized text block.
                    add_ellipsis_pattern = true;
                    break;
                }
            }

            float kerning = bmfont::lookup_kerning(font, prior_char, current);
            pen.x += kerning;

            place_glyph(text_block, glyph, font, text_index, pen, texture_dimensions);

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
            Glyph* typeset_glyph = &text_block->glyphs[glyph_index];
            typeset_glyph->rect = viewport_rect;
            typeset_glyph->texture_rect = {{0.0f, 0.0f}, {0.0f, 0.0f}};
            typeset_glyph->baseline_start = pen;
            typeset_glyph->x_advance = 0.0f;
            typeset_glyph->text_index = text_index;
            text_block->glyphs_count += 1;

            insert(&text_block->glyph_map, text_index, glyph_index);
        }

        prior_char = current;
        i = text_index;
    }

    if(text_block->text_overflow == TextOverflow::Ellipsize_End && add_ellipsis_pattern)
    {
        int size = string_size(ellipsize_text);
        FOR_N(i, size)
        {
            char32_t current;
            int index = utf8_get_next_codepoint(ellipsize_text, size, i, &current);
            if(index == invalid_index)
            {
                break;
            }

            bmfont::Glyph* glyph = bmfont::find_glyph(font, current);
            float kerning = bmfont::lookup_kerning(font, prior_char, current);
            pen.x += kerning;

            place_glyph(text_block, glyph, font, text_index, pen, texture_dimensions);

            pen.x += glyph->x_advance;

            prior_char = current;
            i = index;
        }
    }

    pen.y = abs(pen.y);
    bounds = max2(bounds, pen);
    bounds.x += padding.bottom;
    bounds.y += padding.end;

    return bounds;
}

static Vector2 measure_bound_dimensions(Container* container, Vector2 container_space, float shrink, Axis shrink_along, Stack* stack)
{
    Vector2 result = vector2_zero;
    int main_axis = get_main_axis_index(container->axis);
    int cross_axis = get_cross_axis_index(container->axis);
    int shrink_axis = get_main_axis_index(shrink_along);
    int nonshrink_axis = get_cross_axis_index(shrink_along);

    FOR_N(i, container->items_count)
    {
        Item* item = &container->items[i];

        Vector2 space;
        space[shrink_axis] = shrink * item->ideal_dimensions[shrink_axis];
        space[nonshrink_axis] = container_space[nonshrink_axis];
        space = max2(space, item->min_dimensions);

        Vector2 dimensions;
        switch(item->type)
        {
            case ItemType::Button:
            {
                dimensions = measure_bound_dimensions(&item->button.text_block, space, stack);
                break;
            }
            case ItemType::Container:
            {
                dimensions = measure_bound_dimensions(&item->container, space, shrink, shrink_along, stack);
                break;
            }
            case ItemType::List:
            {
                dimensions = vector2_zero;
                break;
            }
            case ItemType::Text_Block:
            {
                dimensions = measure_bound_dimensions(&item->text_block, space, stack);
                break;
            }
            case ItemType::Text_Input:
            {
                dimensions = measure_bound_dimensions(&item->text_input.text_block, space, stack);
                break;
            }
        }
        item->bounds.dimensions = max2(dimensions, item->min_dimensions);

        result[main_axis] += item->bounds.dimensions[main_axis];
        result[cross_axis] = fmax(result[cross_axis], item->bounds.dimensions[cross_axis]);
    }

    Padding padding = container->padding;
    result.x += padding.start + padding.end;
    result.y += padding.top + padding.bottom;

    // Distribute any excess among the growable items, if allowed.
    if(result[main_axis] < container_space[main_axis])
    {
        int growable_items = 0;
        FOR_N(i, container->items_count)
        {
            Item item = container->items[i];
            if(item.growable)
            {
                growable_items += 1;
            }
        }
        if(growable_items > 0)
        {
            float grow = (container_space[main_axis] - result[main_axis]) / growable_items;
            FOR_N(i, container->items_count)
            {
                Item* item = &container->items[i];
                if(item->growable)
                {
                    item->bounds.dimensions[main_axis] += grow;
                }
            }
        }
    }
    if(container->alignment == Alignment::Stretch && result[cross_axis] < container_space[cross_axis])
    {
        result[cross_axis] = container_space[cross_axis];
        FOR_N(i, container->items_count)
        {
            Item* item = &container->items[i];
            item->bounds.dimensions[cross_axis] = container_space[cross_axis];
        }
    }

    return result;
}

static float compute_shrink_factor(Container* container, Rect space, float ideal_length)
{
    int main_axis = get_main_axis_index(container->axis);
    float available = space.dimensions[main_axis] - padding_along_axis(container->padding, main_axis);

    float shrink = available / ideal_length;
    int items_overshrunk;

    for(;;)
    {
        items_overshrunk = 0;
        float min_filled = 0.0f;
        float fit = 0.0f;
        FOR_N(i, container->items_count)
        {
            Item* item = &container->items[i];

            float length = shrink * item->ideal_dimensions[main_axis];
            if(length < item->min_dimensions[main_axis])
            {
                length = item->min_dimensions[main_axis];
                min_filled += item->min_dimensions[main_axis];
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

static void grow_or_commit_ideal_dimensions(Item* item, Vector2 container_space)
{
    Container* container = &item->container;
    int main_axis = get_main_axis_index(container->axis);
    int cross_axis = get_cross_axis_index(container->axis);

    item->bounds.dimensions[main_axis] = container_space[main_axis];
    item->bounds.dimensions[cross_axis] = item->ideal_dimensions[cross_axis];

    // Record how much space is taken up by items that won't grow and measure
    // the ideal length at the same time.
    float ungrowable_length = 0.0f;
    float ideal_length = 0.0f;
    FOR_N(i, container->items_count)
    {
        Item* next = &container->items[i];
        if(!next->growable)
        {
            ungrowable_length += next->bounds.dimensions[main_axis];
        }
        ideal_length += next->ideal_dimensions[main_axis];
    }

    // Compute by how much to grow the growable items.
    float available = container_space[main_axis] - padding_along_axis(container->padding, main_axis);
    float grow = (available - ungrowable_length) / (ideal_length - ungrowable_length);

    // Grow them.
    FOR_N(i, container->items_count)
    {
        Item* next = &container->items[i];

        Vector2 space;
        if(next->growable)
        {
            space[main_axis] = grow * next->ideal_dimensions[main_axis];
            space[cross_axis] = next->ideal_dimensions[cross_axis];
        }
        else
        {
            space = next->ideal_dimensions;
        }
        if(container->alignment == Alignment::Stretch)
        {
            space[cross_axis] = item->ideal_dimensions[cross_axis];
        }

        switch(next->type)
        {
            case ItemType::Container:
            {
                grow_or_commit_ideal_dimensions(next, space);
                break;
            }
            case ItemType::Button:
            case ItemType::List:
            case ItemType::Text_Block:
            case ItemType::Text_Input:
            {
                next->bounds.dimensions = space;
                break;
            }
        }
    }
}

static float measure_length(Container* container)
{
    int axis = get_main_axis_index(container->axis);
    float length = 0.0f;
    FOR_N(i, container->items_count)
    {
        Item item = container->items[i];
        length += item.bounds.dimensions[axis];
    }
    return length;
}

static void place_items_along_main_axis(Item* item, Rect space)
{
    Container* container = &item->container;
    Padding padding = container->padding;
    int axis = get_main_axis_index(container->axis);

    // Place the container within the space.
    switch(container->axis)
    {
        case Axis::Horizontal:
        {
            item->bounds.bottom_left.x = space.bottom_left.x;
            break;
        }
        case Axis::Vertical:
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
        case Axis::Horizontal:
        {
            float left = item->bounds.bottom_left.x;
            float right = rect_right(item->bounds);
            switch(container->direction)
            {
                case Direction::Left_To_Right:
                {
                    first = left + padding.start;
                    last = right - padding.end;
                    break;
                }
                case Direction::Right_To_Left:
                {
                    first = right - padding.start;
                    last = left + padding.end;
                    break;
                }
            }
            break;
        }
        case Axis::Vertical:
        {
            float top = rect_top(item->bounds);
            float bottom = item->bounds.bottom_left.y;
            first = top - padding.top;
            last = bottom + padding.bottom;
            break;
        }
    }

    bool fill_backward = (container->axis == Axis::Horizontal && container->direction == Direction::Right_To_Left) ||
        container->axis == Axis::Vertical;

    float cursor;
    float apart = 0.0f;
    switch(container->justification)
    {
        case Justification::Start:
        {
            cursor = first;
            break;
        }
        case Justification::End:
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
        case Justification::Center:
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
        case Justification::Space_Around:
        {
            float length = measure_length(container);
            float container_length = abs(last - first);
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
        case Justification::Space_Between:
        {
            float length = measure_length(container);
            float container_length = abs(last - first);
            apart = (container_length - length) / (container->items_count - 1);
            cursor = first;
            break;
        }
    }

    if(fill_backward)
    {
        FOR_N(i, container->items_count)
        {
            Item* next = &container->items[i];

            cursor -= next->bounds.dimensions[axis];
            next->bounds.bottom_left[axis] = cursor;
            cursor -= apart;

            if(next->type == ItemType::Container && next->container.items_count > 0)
            {
                place_items_along_main_axis(next, next->bounds);
            }
        }
    }
    else
    {
        FOR_N(i, container->items_count)
        {
            Item* next = &container->items[i];

            next->bounds.bottom_left[axis] = cursor;
            cursor += next->bounds.dimensions[axis] + apart;

            if(next->type == ItemType::Container && next->container.items_count > 0)
            {
                place_items_along_main_axis(next, next->bounds);
            }
        }
    }
}

static void place_items_along_cross_axis(Item* item, Rect space)
{
    Container* container = &item->container;
    Padding padding = container->padding;
    int cross_axis = get_cross_axis_index(container->axis);

    float space_top = rect_top(space);
    item->bounds.bottom_left.y = space_top - item->bounds.dimensions.y;
    float centering = 0.0f;

    float position;
    switch(container->alignment)
    {
        case Alignment::Start:
        case Alignment::Stretch:
        {
            switch(container->axis)
            {
                case Axis::Horizontal:
                {
                    float top = rect_top(item->bounds);
                    position = top - padding.top;
                    centering = 1.0f;
                    break;
                }
                case Axis::Vertical:
                {
                    switch(container->direction)
                    {
                        case Direction::Left_To_Right:
                        {
                            float left = item->bounds.bottom_left.x;
                            position = left + padding.start;
                            centering = 0.0f;
                            break;
                        }
                        case Direction::Right_To_Left:
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
        case Alignment::End:
        {
            switch(container->axis)
            {
                case Axis::Horizontal:
                {
                    position = item->bounds.bottom_left.y + padding.bottom;
                    centering = 0.0f;
                    break;
                }
                case Axis::Vertical:
                {
                    switch(container->direction)
                    {
                        case Direction::Left_To_Right:
                        {
                            float right = rect_right(item->bounds);
                            position = right - padding.end;
                            centering = 1.0f;
                            break;
                        }
                        case Direction::Right_To_Left:
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
        case Alignment::Center:
        {
            position = item->bounds.bottom_left[cross_axis] + (item->bounds.dimensions[cross_axis] / 2.0f);
            centering = 0.5f;
            break;
        }
    }

    FOR_N(i, container->items_count)
    {
        Item* next = &container->items[i];
        next->bounds.bottom_left[cross_axis] = position - (centering * next->bounds.dimensions[cross_axis]);
        if(next->type == ItemType::Container && next->container.items_count > 0)
        {
            place_items_along_cross_axis(next, next->bounds);
        }
    }
}

void lay_out(Item* item, Rect space, Stack* stack)
{
    Container* container = &item->container;
    int main_axis = get_main_axis_index(container->axis);

    // Compute the ideal dimensions of the container and its contents.
    Vector2 ideal = measure_ideal_dimensions(container, stack);
    item->ideal_dimensions = max2(item->min_dimensions, ideal);
    float ideal_length = 0.0f;
    FOR_N(i, container->items_count)
    {
        Item next = container->items[i];
        ideal_length += next.ideal_dimensions[main_axis];
    }

    // Determine whether there's enough space to fit all items at their ideal
    // dimensions.
    float available = space.dimensions[main_axis] - padding_along_axis(container->padding, main_axis);
    if(ideal_length <= available)
    {
        grow_or_commit_ideal_dimensions(item, space.dimensions);
    }
    else
    {
        // Determine how much to reduce each item lengthwise so everything can
        // fit in the container.
        float shrink = compute_shrink_factor(container, space, ideal_length);

        Vector2 bound = measure_bound_dimensions(container, space.dimensions, shrink, container->axis, stack);
        if(item->growable)
        {
            bound[main_axis] = space.dimensions[main_axis];
        }
        item->bounds.dimensions = max2(bound, item->min_dimensions);
    }

    // Now that all the items have been sized, just place them where they need to be.
    place_items_along_main_axis(item, space);
    place_items_along_cross_axis(item, space);
}

static void draw_text_block(TextBlock* text_block, Rect bounds)
{
    Vector2 top_left = rect_top_left(bounds);

    FOR_N(i, text_block->glyphs_count)
    {
        Glyph glyph = text_block->glyphs[i];
        Rect rect = glyph.rect;
        rect.bottom_left += top_left;
        Quad quad = rect_to_quad(rect);
        immediate::add_quad_textured(&quad, glyph.texture_rect);
    }
    immediate::set_blend_mode(immediate::BlendMode::Transparent);
    immediate::draw();
}

static void draw_button(Item* item)
{
    Button* button = &item->button;

    Vector4 non_hovered_colour;
    Vector4 hovered_colour;
    Vector3 text_colour;
    if(button->enabled)
    {
        non_hovered_colour = {0.145f, 0.145f, 0.145f, 1.0f};
        hovered_colour = {0.247f, 0.251f, 0.271f, 1.0f};
        text_colour = vector3_white;
    }
    else
    {
        non_hovered_colour = {0.318f, 0.318f, 0.318f, 1.0f};
        hovered_colour = {0.382f, 0.386f, 0.418f, 1.0f};
        text_colour = {0.782f, 0.786f, 0.818f};
    }

    Vector4 colour;
    if(button->hovered)
    {
        colour = hovered_colour;
    }
    else
    {
        colour = non_hovered_colour;
    }
    immediate::draw_opaque_rect(item->bounds, colour);

    immediate::set_text_colour(text_colour);
    draw_text_block(&item->button.text_block, item->bounds);
}

static void draw_container(Item* item, Context* context)
{
    ASSERT(item->type == ItemType::Container);

    immediate::draw_opaque_rect(item->bounds, item->container.background_colour);

    FOR_N(i, item->container.items_count)
    {
        draw(&item->container.items[i], context);
    }
}

static void place_glyphs(TextBlock* text_block, Vector2 bounds, Stack* stack)
{
    measure_bound_dimensions(text_block, bounds, stack);
}

static void place_list_items(Item* item, Stack* stack)
{
    List* list = &item->list;

    if(list->items_count > 0)
    {
        float item_height = get_list_item_height(list, &list->items[0]);
        Vector2 top_left = rect_top_left(item->bounds);

        Rect place;
        place.bottom_left.x = list->side_margin + top_left.x;
        place.bottom_left.y = -item_height + top_left.y;
        place.dimensions.x = item->bounds.dimensions.x - (2.0f * list->side_margin);
        place.dimensions.y = item_height - list->item_spacing;

        FOR_N(i, list->items_count)
        {
            list->items_bounds[i] = place;
            place.bottom_left.y -= place.dimensions.y + list->item_spacing;
            place_glyphs(&list->items[i], list->items_bounds[i].dimensions, stack);
        }
    }
}

static void draw_list(Item* item, Context* context)
{
    ASSERT(item->type == ItemType::List);

    place_list_items(item, context->scratch);

    immediate::set_clip_area(item->bounds, context->viewport.x, context->viewport.y);

    List* list = &item->list;
    const Vector4 hover_colour = {1.0f, 1.0f, 1.0f, 0.3f};
    const Vector4 selection_colour = {1.0f, 1.0f, 1.0f, 0.5f};

    if(list->items)
    {
        // Draw the hover highlight for the items.
        if(is_valid_index(list->hovered_item_index) && list->hovered_item_index != list->selected_item_index)
        {
            Rect rect = list->items_bounds[list->hovered_item_index];
            rect.bottom_left.y += list->scroll_top;
            immediate::draw_transparent_rect(rect, hover_colour);
        }

        // Draw the selection highlight for the records.
        if(is_valid_index(list->selected_item_index))
        {
            Rect rect = list->items_bounds[list->selected_item_index];
            rect.bottom_left.y += list->scroll_top;
            immediate::draw_transparent_rect(rect, selection_colour);
        }

        // Draw each item's contents.
        float item_height = get_list_item_height(list, &list->items[0]);
        float scroll = list->scroll_top;
        int start_index = scroll / item_height;
        start_index = MAX(start_index, 0);
        int end_index = ceil((scroll + item->bounds.dimensions.y) / item_height);
        end_index = MIN(end_index, list->items_count);

        for(int i = start_index; i < end_index; i += 1)
        {
            TextBlock* next = &list->items[i];
            Rect bounds = list->items_bounds[i];
            bounds.bottom_left.y += list->scroll_top;
            draw_text_block(next, bounds);
        }
    }

    immediate::stop_clip_area();
}

static Vector2 compute_cursor_position(TextBlock* text_block, Vector2 dimensions, int index)
{
    bmfont::Font* font = text_block->font;
    Padding padding = text_block->padding;

    // @Incomplete: There isn't a one-to-one mapping between chars and glyphs
    // Add a lookup to find the glyph given and index in the string.
    Vector2 position = {padding.start, -padding.top - font->line_height};
    int size = string_size(text_block->text);
    if(index >= size && size > 0)
    {
        Glyph glyph = text_block->glyphs[text_block->glyphs_count - 1];
        Vector2 bottom_right;
        bottom_right.x = glyph.baseline_start.x + glyph.x_advance;
        bottom_right.y = glyph.baseline_start.y;
        position = bottom_right;
    }
    else if(index >= 0)
    {
        u32 glyph_index;
        bool found = look_up(&text_block->glyph_map, index, &glyph_index);
        if(found)
        {
            Glyph glyph = text_block->glyphs[glyph_index];
            position = glyph.baseline_start;
        }
    }

    return position;
}

static int find_index_at_position(TextBlock* text_block, Vector2 dimensions, Vector2 position)
{
    int index = invalid_index;

    bmfont::Font* font = text_block->font;

    float closest = infinity;
    int count = text_block->glyphs_count;
    FOR_N(i, count)
    {
        Glyph glyph = text_block->glyphs[i];
        Vector2 point = glyph.baseline_start;

        if(position.y >= point.y + font->line_height)
        {
            break;
        }
        else if(position.y >= point.y)
        {
            float distance = abs(point.x - position.x);
            if(distance < closest)
            {
                closest = distance;
                index = glyph.text_index;
            }
        }
    }

    Glyph last = text_block->glyphs[count - 1];
    Vector2 point;
    point.x = last.baseline_start.x + last.x_advance;
    point.y = last.baseline_start.y;
    if(position.y < point.y + font->line_height && position.y > point.y)
    {
        float distance = abs(point.x - position.x);
        if(distance < closest)
        {
            closest = distance;
            index = string_size(text_block->text);
        }
    }

    return index;
}

void draw_text_input(Item* item, Context* context)
{
    ASSERT(item->type == ItemType::Text_Input);

    const Vector4 selection_colour = {1.0f, 1.0f, 1.0f, 0.4f};

    draw_text_block(&item->text_input.text_block, item->bounds);

    // Draw the cursor.
    TextInput* text_input = &item->text_input;
    TextBlock* text_block = &text_input->text_block;
    bmfont::Font* font = text_block->font;

    if(focused_on(context, item))
    {
        Vector2 cursor = compute_cursor_position(text_block, item->bounds.dimensions, text_input->cursor_position);
        Vector2 top_left = rect_top_left(item->bounds);
        cursor += top_left;

        Rect rect;
        rect.dimensions = {1.7f, font->line_height};
        rect.bottom_left = cursor;

        text_input->cursor_blink_frame = (text_input->cursor_blink_frame + 1) & 0x3f;
        if((text_input->cursor_blink_frame / 32) & 1)
        {
            immediate::draw_opaque_rect(rect, vector4_white);
        }

        // Draw the selection highlight.
        if(text_input->cursor_position != text_input->selection_start)
        {
            Vector2 start = compute_cursor_position(text_block, item->bounds.dimensions, text_input->selection_start);
            start += top_left;

            Vector2 first, second;
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
                rect.dimensions.y = font->line_height;
                immediate::draw_transparent_rect(rect, selection_colour);
            }
            else
            {
                // The selection endpoints are on different lines.
                Padding padding = text_block->padding;
                float left = item->bounds.bottom_left.x + padding.start;
                float right = left + item->bounds.dimensions.x - padding.end;

                rect.bottom_left = first;
                rect.dimensions.x = right - first.x;
                rect.dimensions.y = font->line_height;
                immediate::add_rect(rect, selection_colour);

                int between_lines = (first.y - second.y) / font->line_height - 1;
                FOR_N(i, between_lines)
                {
                    rect.dimensions.x = right - left;
                    rect.bottom_left.x = left;
                    rect.bottom_left.y = first.y - (font->line_height * (i + 1));
                    immediate::add_rect(rect, selection_colour);
                }

                rect.dimensions.x = second.x - left;
                rect.bottom_left.x = left;
                rect.bottom_left.y = second.y;
                immediate::add_rect(rect, selection_colour);

                immediate::set_blend_mode(immediate::BlendMode::Transparent);
                immediate::draw();
            }
        }
    }
}

void draw(Item* item, Context* context)
{
    switch(item->type)
    {
        case ItemType::Button:
        {
            draw_button(item);
            break;
        }
        case ItemType::Container:
        {
            draw_container(item, context);
            break;
        }
        case ItemType::List:
        {
            draw_list(item, context);
            break;
        }
        case ItemType::Text_Block:
        {
            draw_text_block(&item->text_block, item->bounds);
            break;
        }
        case ItemType::Text_Input:
        {
            draw_text_input(item, context);
            break;
        }
    }
}

void draw_focus_indicator(Item* item, Context* context)
{
    if(context->focused_item && in_focus_scope(item, context->focused_item))
    {
        Item* focused = context->focused_item;

        const Vector4 colour = {1.0f, 1.0f, 0.0f, 0.6f};
        const float line_width = 2.0f;

        Rect bounds = focused->bounds;

        float width = bounds.dimensions.x;
        float height = bounds.dimensions.y;

        Vector2 top_left = rect_top_left(bounds);
        Vector2 bottom_left = bounds.bottom_left;
        Vector2 bottom_right = rect_bottom_right(bounds);

        Rect top;
        top.bottom_left = top_left;
        top.dimensions = {width, line_width};

        Rect bottom;
        bottom.bottom_left = {bottom_left.x, bottom_left.y - line_width};
        bottom.dimensions = {width, line_width};

        Rect left;
        left.bottom_left = {bottom_left.x - line_width, bottom_left.y - line_width};
        left.dimensions = {line_width, height + (2 * line_width)};

        Rect right;
        right.bottom_left = {bottom_right.x, bottom_right.y - line_width};
        right.dimensions = {line_width, height + (2 * line_width)};

        immediate::draw_transparent_rect(top, colour);
        immediate::draw_transparent_rect(bottom, colour);
        immediate::draw_transparent_rect(left, colour);
        immediate::draw_transparent_rect(right, colour);
    }
}

static bool detect_hover(Item* item, Vector2 pointer_position, Platform* platform)
{
    bool detected = false;

    switch(item->type)
    {
        case ItemType::Button:
        {
            Button* button = &item->button;
            bool hovered = point_in_rect(item->bounds, pointer_position);
            button->hovered = hovered;
            if(hovered)
            {
                if(button->enabled)
                {
                    change_cursor(platform, CursorType::Arrow);
                }
                else
                {
                    change_cursor(platform, CursorType::Prohibition_Sign);
                }
            }
            detected = hovered;
            break;
        }
        case ItemType::Container:
        {
            Container* container = &item->container;
            FOR_N(i, container->items_count)
            {
                Item* inside = &container->items[i];
                detected |= detect_hover(inside, pointer_position, platform);
            }
            break;
        }
        case ItemType::List:
        {
            List* list = &item->list;
            list->hovered_item_index = invalid_index;
            FOR_N(i, list->items_count)
            {
                Rect bounds = list->items_bounds[i];
                bounds.bottom_left.y += list->scroll_top;
                clip_rects(bounds, item->bounds, &bounds);
                bool hovered = point_in_rect(bounds, pointer_position);
                if(hovered)
                {
                    list->hovered_item_index = i;
                    change_cursor(platform, CursorType::Arrow);
                    detected = true;
                }
            }
            break;
        }
        case ItemType::Text_Block:
        {
            break;
        }
        case ItemType::Text_Input:
        {
            bool hovered = point_in_rect(item->bounds, pointer_position);
            if(hovered)
            {
                change_cursor(platform, CursorType::I_Beam);
            }
            detected = hovered;
            break;
        }
    }

    return detected;
}

static bool detect_focus_changes(Context* context, Item* item, Vector2 mouse_position)
{
    bool focus_taken;

    switch(item->type)
    {
        case ItemType::Button:
        {
            focus_on_item(context, item);
            focus_taken = true;
            break;
        }
        case ItemType::Container:
        {
            Container* container = &item->container;

            focus_taken = false;
            FOR_N(i, container->items_count)
            {
                Item* inside = &container->items[i];

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
        case ItemType::List:
        {
            focus_on_item(context, item);
            focus_taken = true;
            break;
        }
        case ItemType::Text_Block:
        {
            focus_taken = false;
            break;
        }
        case ItemType::Text_Input:
        {
            focus_on_item(context, item);
            focus_taken = true;
            break;
        }
    }

    return focus_taken;
}

static void detect_focus_changes_for_toplevel_containers(Context* context)
{
    bool clicked = input::get_mouse_clicked(input::MouseButton::Left)
        || input::get_mouse_clicked(input::MouseButton::Middle)
        || input::get_mouse_clicked(input::MouseButton::Right);

    Vector2 mouse_position;
    int position_x;
    int position_y;
    input::get_mouse_position(&position_x, &position_y);
    mouse_position.x = position_x - context->viewport.x / 2.0f;
    mouse_position.y = -(position_y - context->viewport.y / 2.0f);

    Item* highest = nullptr;
    FOR_ALL(context->toplevel_containers)
    {
        Item* item = *it;
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
            focus_on_item(context, nullptr);
        }
    }
}

static bool detect_capture_changes(Context* context, Item* item, Vector2 mouse_position)
{
    bool captured;

    switch(item->type)
    {
        case ItemType::Button:
        {
            capture(context, item);
            captured = true;
            break;
        }
        case ItemType::Container:
        {
            Container* container = &item->container;
            FOR_N(i, container->items_count)
            {
                Item* inside = &container->items[i];

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
                capture(context, item);
                captured = true;
            }
            break;
        }
        case ItemType::List:
        {
            capture(context, item);
            captured = true;
            break;
        }
        case ItemType::Text_Block:
        {
            capture(context, item);
            captured = true;
            break;
        }
        case ItemType::Text_Input:
        {
            capture(context, item);
            captured = true;
            break;
        }
    }

    return captured;
}

static void detect_capture_changes_for_toplevel_containers(Context* context, Platform* platform)
{
    bool clicked = input::get_mouse_clicked(input::MouseButton::Left)
        || input::get_mouse_clicked(input::MouseButton::Middle)
        || input::get_mouse_clicked(input::MouseButton::Right);

    Vector2 mouse_position;
    int position_x;
    int position_y;
    input::get_mouse_position(&position_x, &position_y);
    mouse_position.x = position_x - context->viewport.x / 2.0f;
    mouse_position.y = -(position_y - context->viewport.y / 2.0f);

    Item* highest = nullptr;
    FOR_ALL(context->toplevel_containers)
    {
        Item* item = *it;
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
            capture(context, nullptr);
        }
        context->anything_hovered = false;
    }
}

static void update_removed_glyphs(TextBlock* text_block, Vector2 dimensions, Stack* stack)
{
    place_glyphs(text_block, dimensions, stack);
}

static void update_added_glyphs(TextBlock* text_block, Vector2 dimensions, Heap* heap, Stack* stack)
{
    // @Incomplete: There isn't a one-to-one mapping between chars and glyphs.
    // Glyph count should be based on the mapping of the given text in a
    // particular font instead of the size in bytes.
    int glyphs_cap = string_size(text_block->text);
    text_block->glyphs = HEAP_REALLOCATE(heap, text_block->glyphs, glyphs_cap);
    text_block->glyphs_cap = glyphs_cap;
    destroy(&text_block->glyph_map, heap);
    create(&text_block->glyph_map, glyphs_cap, heap);

    place_glyphs(text_block, dimensions, stack);
}

static void remove_selected_text(TextInput* text_input, Vector2 dimensions, Stack* stack)
{
    if(text_input->cursor_position != text_input->selection_start)
    {
        remove_substring(text_input->text_block.text, text_input->selection_start, text_input->cursor_position);
        update_removed_glyphs(&text_input->text_block, dimensions, stack);
        int collapsed = MIN(text_input->selection_start, text_input->cursor_position);
        text_input->cursor_position = collapsed;
        text_input->selection_start = collapsed;
    }
}

void insert_text(Item* item, const char* text_to_add, Heap* heap, Stack* stack)
{
    TextInput* text_input = &item->text_input;
    TextBlock* text_block = &text_input->text_block;
    Vector2 dimensions = item->bounds.dimensions;

    int text_to_add_size = string_size(text_to_add);
    if(text_to_add_size > 0)
    {
        remove_selected_text(text_input, dimensions, stack);

        int insert_index = text_input->cursor_position;
        char* text = insert_string(text_block->text, text_to_add, insert_index, heap);
        HEAP_DEALLOCATE(heap, text_block->text);
        text_block->text = text;

        update_added_glyphs(text_block, dimensions, heap, stack);

        // Advance the cursor past the inserted text.
        int text_end = string_size(text_block->text);
        text_input->cursor_position = MIN(text_input->cursor_position + text_to_add_size, text_end);
        text_input->selection_start = text_input->cursor_position;
    }
}

static bool copy_selected_text(TextInput* text_input, Platform* platform, Heap* heap)
{
    TextBlock* text_block = &text_input->text_block;

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

static int find_beginning_of_line(TextBlock* text_block, int start_index)
{
    start_index = MIN(start_index, string_size(text_block->text) - 1);

    u32 glyph_index;
    bool found = look_up(&text_block->glyph_map, start_index, &glyph_index);
    if(found)
    {
        Glyph first = text_block->glyphs[glyph_index];
        float first_y = first.baseline_start.y;
        for(int i = glyph_index - 1; i >= 0; i -= 1)
        {
            Glyph glyph = text_block->glyphs[i];
            if(glyph.baseline_start.y > first_y)
            {
                return text_block->glyphs[i + 1].text_index;
            }
        }
    }

    return 0;
}

static int find_end_of_line(TextBlock* text_block, int start_index)
{
    int end_of_text = string_size(text_block->text);
    start_index = MIN(start_index, end_of_text - 1);

    u32 glyph_index;
    bool found = look_up(&text_block->glyph_map, start_index, &glyph_index);
    if(found)
    {
        Glyph first = text_block->glyphs[glyph_index];
        float first_y = first.baseline_start.y;
        for(int i = glyph_index + 1; i < text_block->glyphs_count; i += 1)
        {
            Glyph glyph = text_block->glyphs[i];
            if(glyph.baseline_start.y < first_y)
            {
                return text_block->glyphs[i - 1].text_index;
            }
        }
    }

    return end_of_text;
}

static void update_keyboard_input(Item* item, Context* context, Platform* platform)
{
    switch(item->type)
    {
        case ItemType::Button:
        {
            Button* button = &item->button;

            bool activated = input::get_key_tapped(input::Key::Space)
                || input::get_key_tapped(input::Key::Enter);

            if(activated && button->enabled)
            {
                Event event;
                event.type = EventType::Button;
                event.button.id = item->id;
                enqueue(&context->queue, event);
            }
            break;
        }
        case ItemType::Container:
        {
            Container* container = &item->container;
            FOR_N(i, container->items_count)
            {
                update_keyboard_input(&container->items[i], context, platform);
            }
            break;
        }
        case ItemType::List:
        {
            List* list = &item->list;
            int count = list->items_count;

            bool selection_changed = false;
            bool expand = false;

            if(count > 0)
            {
                if(input::get_key_auto_repeated(input::Key::Down_Arrow))
                {
                    list->selected_item_index = (list->selected_item_index + 1) % count;
                    selection_changed = true;
                }

                if(input::get_key_auto_repeated(input::Key::Up_Arrow))
                {
                    list->selected_item_index = mod(list->selected_item_index - 1, count);
                    selection_changed = true;
                }

                if(input::get_key_tapped(input::Key::Home))
                {
                    list->selected_item_index = 0;
                    selection_changed = true;
                }

                if(input::get_key_tapped(input::Key::End))
                {
                    list->selected_item_index = count - 1;
                    selection_changed = true;
                }

                float item_height = list->items_bounds[0].dimensions.y + list->item_spacing;
                int items_per_page = round(item->bounds.dimensions.y / item_height);

                if(input::get_key_tapped(input::Key::Page_Up))
                {
                    list->selected_item_index = MAX(list->selected_item_index - items_per_page, 0);
                    selection_changed = true;
                }

                if(input::get_key_tapped(input::Key::Page_Down))
                {
                    list->selected_item_index = MIN(list->selected_item_index + items_per_page, count - 1);
                    selection_changed = true;
                }
            }

            if((input::get_key_tapped(input::Key::Space) || input::get_key_tapped(input::Key::Enter)) && is_valid_index(list->selected_item_index))
            {
                selection_changed = true;
                expand = true;
            }

            if(selection_changed)
            {
                Event event;
                event.type = EventType::List_Selection;
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

                const float velocity = 0.17f;

                if(item_top < scroll_top + from_page_edge)
                {
                    scroll(item, velocity);
                }
                else if(item_top > scroll_top + page_height - from_page_edge)
                {
                    scroll(item, -velocity);
                }
            }
            break;
        }
        case ItemType::Text_Block:
        {
            break;
        }
        case ItemType::Text_Input:
        {
            TextInput* text_input = &item->text_input;
            TextBlock* text_block = &text_input->text_block;

            // Type out any new text.
            char* text_to_add = input::get_composed_text();
            insert_text(item, text_to_add, context->heap, context->scratch);

            if(input::get_key_auto_repeated(input::Key::Left_Arrow))
            {
                if(input::get_key_modified_by_control(input::Key::Left_Arrow))
                {
                    text_input->cursor_position = find_prior_beginning_of_word(text_block->text, text_input->cursor_position, context->scratch);
                }
                else
                {
                    text_input->cursor_position = find_prior_beginning_of_grapheme_cluster(text_block->text, text_input->cursor_position, context->scratch);
                }

                if(!input::get_key_modified_by_shift(input::Key::Left_Arrow))
                {
                    text_input->selection_start = text_input->cursor_position;
                }
            }

            if(input::get_key_auto_repeated(input::Key::Right_Arrow))
            {
                if(input::get_key_modified_by_control(input::Key::Right_Arrow))
                {
                    text_input->cursor_position = find_next_end_of_word(text_block->text, text_input->cursor_position, context->scratch);
                }
                else
                {
                    text_input->cursor_position = find_next_end_of_grapheme_cluster(text_block->text, text_input->cursor_position, context->scratch);
                }

                if(!input::get_key_modified_by_shift(input::Key::Right_Arrow))
                {
                    text_input->selection_start = text_input->cursor_position;
                }
            }

            if(input::get_key_auto_repeated(input::Key::Up_Arrow))
            {
                Vector2 position = compute_cursor_position(text_block, item->bounds.dimensions, text_input->cursor_position);
                position.y += text_block->font->line_height;
                int index = find_index_at_position(text_block, item->bounds.dimensions, position);

                if(index != invalid_index)
                {
                    text_input->cursor_position = index;

                    if(!input::get_key_modified_by_shift(input::Key::Up_Arrow))
                    {
                        text_input->selection_start = text_input->cursor_position;
                    }
                }
            }

            if(input::get_key_auto_repeated(input::Key::Down_Arrow))
            {
                Vector2 position = compute_cursor_position(text_block, item->bounds.dimensions, text_input->cursor_position);
                position.y -= text_block->font->line_height;
                int index = find_index_at_position(text_block, item->bounds.dimensions, position);

                if(index != invalid_index)
                {
                    text_input->cursor_position = index;

                    if(!input::get_key_modified_by_shift(input::Key::Down_Arrow))
                    {
                        text_input->selection_start = text_input->cursor_position;
                    }
                }
            }

            if(input::get_key_tapped(input::Key::Home))
            {
                if(input::get_key_modified_by_control(input::Key::Home))
                {
                    text_input->cursor_position = 0;
                }
                else
                {
                    text_input->cursor_position = find_beginning_of_line(text_block, text_input->cursor_position);
                }
                if(!input::get_key_modified_by_shift(input::Key::Home))
                {
                    text_input->selection_start = text_input->cursor_position;
                }
            }

            if(input::get_key_tapped(input::Key::End))
            {
                if(input::get_key_modified_by_control(input::Key::End))
                {
                    text_input->cursor_position = string_size(text_block->text);
                }
                else
                {
                    text_input->cursor_position = find_end_of_line(text_block, text_input->cursor_position);
                }
                if(!input::get_key_modified_by_shift(input::Key::End))
                {
                    text_input->selection_start = text_input->cursor_position;
                }
            }

            if(input::get_key_auto_repeated(input::Key::Delete))
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
                update_removed_glyphs(text_block, item->bounds.dimensions, context->scratch);

                // Set the cursor to the beginning of the selection.
                int new_position;
                if(start < end)
                {
                    new_position = start;
                }
                else
                {
                    new_position = end;
                }
                text_input->cursor_position = new_position;
                text_input->selection_start = new_position;
            }

            if(input::get_key_auto_repeated(input::Key::Backspace))
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
                update_removed_glyphs(text_block, item->bounds.dimensions, context->scratch);

                // Retreat the cursor or set it to the beginning of the selection.
                int new_position;
                if(start < end)
                {
                    new_position = start;
                }
                else
                {
                    new_position = end;
                }
                text_input->cursor_position = new_position;
                text_input->selection_start = new_position;
            }

            if(input::get_hotkey_tapped(input::Function::Select_All))
            {
                text_input->cursor_position = 0;
                text_input->selection_start = string_size(text_block->text);
            }

            if(input::get_hotkey_tapped(input::Function::Copy))
            {
                copy_selected_text(text_input, platform, context->heap);
            }

            if(input::get_hotkey_tapped(input::Function::Cut))
            {
                bool copied = copy_selected_text(text_input, platform, context->heap);
                if(copied)
                {
                    remove_selected_text(text_input, item->bounds.dimensions, context->scratch);
                }
            }

            if(input::get_hotkey_tapped(input::Function::Paste))
            {
                request_paste_from_clipboard(platform);
            }

            break;
        }
    }
}

static void build_tab_navigation_list(Context* context, Item* at)
{
    Container* container = &at->container;

    FOR_N(j, container->items_count)
    {
        Item* item = &container->items[j];

        switch(item->type)
        {
            case ItemType::Button:
            {
                ARRAY_ADD(context->tab_navigation_list, item, context->heap);
                break;
            }
            case ItemType::Container:
            {
                build_tab_navigation_list(context, item);
                break;
            }
            case ItemType::List:
            {
                ARRAY_ADD(context->tab_navigation_list, item, context->heap);
                break;
            }
            case ItemType::Text_Block:
            {
                break;
            }
            case ItemType::Text_Input:
            {
                ARRAY_ADD(context->tab_navigation_list, item, context->heap);
                break;
            }
        }
    }
}

static void build_tab_navigation_list(Context* context)
{
    ARRAY_DESTROY(context->tab_navigation_list, context->heap);

    FOR_ALL(context->toplevel_containers)
    {
        Item* toplevel = *it;
        build_tab_navigation_list(context, toplevel);
    }
}

static void update_non_item_specific_keyboard_input(Context* context)
{
    if(input::get_key_auto_repeated(input::Key::Tab))
    {
        bool backward = input::get_key_modified_by_shift(input::Key::Tab);

        ASSERT(context->tab_navigation_list);
        ASSERT(ARRAY_COUNT(context->tab_navigation_list) > 0);

        if(!context->focused_item)
        {
            if(backward)
            {
                focus_on_item(context, ARRAY_LAST(context->tab_navigation_list));
            }
            else
            {
                focus_on_item(context, context->tab_navigation_list[0]);
            }
        }
        else
        {
            int found_index = invalid_index;
            int count = ARRAY_COUNT(context->tab_navigation_list);

            FOR_N(i, count)
            {
                Item* item = context->tab_navigation_list[i];
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
                focus_on_item(context, context->tab_navigation_list[index]);
            }
        }
    }
}

static void update_pointer_input(Item* item, Context* context, Platform* platform)
{
    switch(item->type)
    {
        case ItemType::Button:
        {
            Button* button = &item->button;
            if(button->hovered && input::get_mouse_clicked(input::MouseButton::Left) && button->enabled)
            {
                Event event;
                event.type = EventType::Button;
                event.button.id = item->id;
                enqueue(&context->queue, event);
            }
            break;
        }
        case ItemType::Container:
        {
            Container* container = &item->container;
            FOR_N(i, container->items_count)
            {
                update_pointer_input(&container->items[i], context, platform);
            }
            break;
        }
        case ItemType::List:
        {
            List* list = &item->list;

            int velocity_x;
            int velocity_y;
            input::get_mouse_scroll_velocity(&velocity_x, &velocity_y);
            const float speed = 0.17f;
            scroll(item, speed * velocity_y);

            if(is_valid_index(list->hovered_item_index) && input::get_mouse_clicked(input::MouseButton::Left))
            {
                list->selected_item_index = list->hovered_item_index;

                Event event;
                event.type = EventType::List_Selection;
                event.list_selection.index = list->selected_item_index;
                event.list_selection.expand = true;
                enqueue(&context->queue, event);
            }
            break;
        }
        case ItemType::Text_Block:
        {
            break;
        }
        case ItemType::Text_Input:
        {
            TextInput* text_input = &item->text_input;
            TextBlock* text_block = &text_input->text_block;

            bool clicked = input::get_mouse_clicked(input::MouseButton::Left);
            bool dragged = !clicked && input::get_mouse_pressed(input::MouseButton::Left);
            if(clicked || dragged)
            {
                int x, y;
                input::get_mouse_position(&x, &y);
                Vector2 position;
                position.x = x - context->viewport.x / 2.0f;
                position.y = -(y - context->viewport.y / 2.0f);

                Vector2 top_left = rect_top_left(item->bounds);
                position -= top_left;

                int index = find_index_at_position(text_block, item->bounds.dimensions, position);
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

void update(Context* context, Platform* platform)
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

} // namespace ui
