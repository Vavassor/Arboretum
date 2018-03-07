#include "ui.h"

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
#include "unicode_grapheme_cluster_break.h"
#include "unicode_word_break.h"
#include "vector_math.h"

namespace ui {

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

static void destroy_list(List* list, Heap* heap)
{
    SAFE_HEAP_DEALLOCATE(heap, list->items);
}

static void destroy_text_block(TextBlock* text_block, Heap* heap)
{
    SAFE_HEAP_DEALLOCATE(heap, text_block->text);
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
        SAFE_HEAP_DEALLOCATE(heap, container->items);
    }
}

void destroy_context(Context* context, Heap* heap)
{
    destroy_event_queue(&context->queue, heap);

    FOR_N(i, context->toplevel_containers_count)
    {
        Item* item = context->toplevel_containers[i];
        destroy_container(&item->container, heap);
        SAFE_HEAP_DEALLOCATE(heap, item);
    }
    SAFE_HEAP_DEALLOCATE(heap, context->toplevel_containers);
}

static Id generate_id(Context* context)
{
    Id id = context->seed;
    context->seed += 1;
    return id;
}

bool focused_on(Context* context, Item* item)
{
    return item == context->focused_container;
}

void focus_on_container(Context* context, Item* item)
{
    if(item != context->focused_container)
    {
        Event event = {};
        event.type = EventType::Focus_Change;
        if(item)
        {
            event.focus_change.now_focused = item->id;
        }
        if(context->focused_container)
        {
            event.focus_change.now_unfocused = context->focused_container->id;
        }
        enqueue(&context->queue, event);
    }

    context->focused_container = item;
}

Item* create_toplevel_container(Context* context, Heap* heap)
{
    int count = context->toplevel_containers_count;
    context->toplevel_containers = HEAP_REALLOCATE(heap, Item*, context->toplevel_containers, count + 1);

    Item* item = HEAP_ALLOCATE(heap, Item, 1);
    item->id = generate_id(context);
    context->toplevel_containers[count] = item;
    context->toplevel_containers_count += 1;
    return item;
}

void destroy_toplevel_container(Context* context, Item* item, Heap* heap)
{
    destroy_container(&item->container, heap);

    // Find the container and overwrite it with the container on the end.
    int count = context->toplevel_containers_count;
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
    // If the container is currently focused, make sure it becomes unfocused.
    if(focused_on(context, item))
    {
        focus_on_container(context, nullptr);
    }
    // Only deallocate after the item is removed, because it sets the pointer to
    // null and would cause it not to be found.
    SAFE_HEAP_DEALLOCATE(heap, item);

    context->toplevel_containers = HEAP_REALLOCATE(heap, Item*, context->toplevel_containers, count - 1);
    context->toplevel_containers_count = count - 1;
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

static const int invalid_index = -1;

static bool is_valid_list_index(int index)
{
    return index != invalid_index;
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

static Vector2 measure_ideal_dimensions(TextBlock* text_block)
{
    Vector2 bounds = vector2_zero;

    bmfont::Font* font = text_block->font;
    Padding padding = text_block->padding;

    char32_t prior_char = '\0';
    int size = string_size(text_block->text);
    Vector2 pen = {padding.start, padding.top + font->line_height};
    FOR_N(i, size)
    {
        char32_t current = text_block->text[i];
        if(current == '\r')
        {
            bounds.x = fmax(bounds.x, pen.x);
            pen.x = padding.start;
        }
        else if(current == '\n')
        {
            bounds.x = fmax(bounds.x, pen.x);
            pen.x = padding.start;
            pen.y += font->line_height;
        }
        else
        {
            bmfont::Glyph* glyph = bmfont::find_glyph(font, current);
            float kerning = bmfont::lookup_kerning(font, prior_char, current);
            pen.x += glyph->x_advance + kerning;
        }
        prior_char = current;
    }

    bounds = max2(bounds, pen);
    bounds.x += padding.bottom;
    bounds.y += padding.end;

    return bounds;
}

static Vector2 measure_ideal_dimensions(Container* container)
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
                item_ideal = measure_ideal_dimensions(&item->button.text_block);
                break;
            }
            case ItemType::Container:
            {
                item_ideal = measure_ideal_dimensions(&item->container);
                break;
            }
            case ItemType::List:
            {
                item_ideal = vector2_max;
                break;
            }
            case ItemType::Text_Block:
            {
                item_ideal = measure_ideal_dimensions(&item->text_block);
                break;
            }
            case ItemType::Text_Input:
            {
                item_ideal = measure_ideal_dimensions(&item->text_input.text_block);
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

static Vector2 measure_bound_dimensions(TextBlock* text_block, Vector2 dimensions)
{
    Vector2 bounds = vector2_zero;

    bmfont::Font* font = text_block->font;
    Padding padding = text_block->padding;

    float right = dimensions.x - padding.end;

    char32_t prior_char = '\0';
    int size = string_size(text_block->text);
    Vector2 pen = {padding.start, padding.top + font->line_height};
    FOR_N(i, size)
    {
        char32_t current = text_block->text[i];
        if(current == '\r')
        {
            bounds.x = fmax(bounds.x, pen.x);
            pen.x = padding.start;
        }
        else if(current == '\n')
        {
            bounds.x = fmax(bounds.x, pen.x);
            pen.x = padding.start;
            pen.y += font->line_height;
        }
        else
        {
            bmfont::Glyph* glyph = bmfont::find_glyph(font, current);
            if(pen.x + glyph->x_advance > right)
            {
                bounds.x = fmax(bounds.x, pen.x);
                pen.x = padding.start;
                pen.y += font->line_height;
            }
            else
            {
                float kerning = bmfont::lookup_kerning(font, prior_char, current);
                pen.x += kerning;
            }
            pen.x += glyph->x_advance;
        }
        prior_char = current;
    }

    bounds = max2(bounds, pen);
    bounds.x += padding.bottom;
    bounds.y += padding.end;

    return bounds;
}

static Vector2 measure_bound_dimensions(Container* container, Vector2 container_space, float shrink, Axis shrink_along)
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
                dimensions = measure_bound_dimensions(&item->button.text_block, space);
                break;
            }
            case ItemType::Container:
            {
                dimensions = measure_bound_dimensions(&item->container, space, shrink, shrink_along);
                break;
            }
            case ItemType::List:
            {
                dimensions = vector2_zero;
                break;
            }
            case ItemType::Text_Block:
            {
                dimensions = measure_bound_dimensions(&item->text_block, space);
                break;
            }
            case ItemType::Text_Input:
            {
                dimensions = measure_bound_dimensions(&item->text_input.text_block, space);
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

void lay_out(Item* item, Rect space)
{
    Container* container = &item->container;
    int main_axis = get_main_axis_index(container->axis);

    // Compute the ideal dimensions of the container and its contents.
    Vector2 ideal = measure_ideal_dimensions(container);
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

        Vector2 bound = measure_bound_dimensions(container, space.dimensions, shrink, container->axis);
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
    Padding padding = text_block->padding;
    float left = bounds.bottom_left.x + padding.start;
    float right = bounds.bottom_left.x + bounds.dimensions.x - padding.end;

    bmfont::Font* font = text_block->font;
    Vector2 top_left = rect_top_left(bounds);
    Vector2 baseline_start;
    baseline_start.x = top_left.x + padding.start;
    baseline_start.y = top_left.y - padding.top - font->line_height;

    Vector2 texture_dimensions;
    texture_dimensions.x = font->image_width;
    texture_dimensions.y = font->image_height;

    Vector2 pen = baseline_start;
    char32_t prior_char = '\0';

    int size = string_size(text_block->text);
    FOR_N(i, size)
    {
        char32_t current = text_block->text[i];
        if(current == '\r')
        {
            pen.x = left;
        }
        else if(current == '\n')
        {
            pen.x = left;
            pen.y -= font->line_height;
        }
        else
        {
            bmfont::Glyph* glyph = bmfont::find_glyph(font, current);

            if(pen.x + glyph->x_advance > right)
            {
                pen.x = left;
                pen.y -= font->line_height;
            }
            else
            {
                float kerning = bmfont::lookup_kerning(font, prior_char, current);
                pen.x += kerning;
            }

            Vector2 top_left = pen;
            top_left.x += glyph->offset.x;
            top_left.y -= glyph->offset.y;

            Rect viewport_rect;
            viewport_rect.dimensions = glyph->rect.dimensions;
            viewport_rect.bottom_left.x = top_left.x;
            viewport_rect.bottom_left.y = top_left.y + font->line_height - viewport_rect.dimensions.y;

            Quad quad = rect_to_quad(viewport_rect);

            Rect texture_rect = glyph->rect;
            texture_rect.bottom_left = pointwise_divide(texture_rect.bottom_left, texture_dimensions);
            texture_rect.dimensions = pointwise_divide(texture_rect.dimensions, texture_dimensions);

            immediate::add_quad_textured(&quad, texture_rect);

            pen.x += glyph->x_advance;
        }
        prior_char = current;
    }
    immediate::set_blend_mode(immediate::BlendMode::Transparent);
    immediate::draw();
}

static void draw_button(Item* item)
{
    Button* button = &item->button;

    const Vector4 non_hovered_colour = {0.145f, 0.145f, 0.145f, 1.0f};
    const Vector4 hovered_colour = {0.247f, 0.251f, 0.271f, 1.0f};
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

static void place_list_items(Item* item)
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
        }
    }
}

static void draw_list(Item* item, Context* context)
{
    ASSERT(item->type == ItemType::List);

    place_list_items(item);

    immediate::set_clip_area(item->bounds, context->viewport.x, context->viewport.y);

    List* list = &item->list;
    const Vector4 hover_colour = {1.0f, 1.0f, 1.0f, 0.3f};
    const Vector4 selection_colour = {1.0f, 1.0f, 1.0f, 0.5f};

    if(list->items)
    {
        // Draw the hover highlight for the items.
        if(is_valid_list_index(list->hovered_item_index) && list->hovered_item_index != list->selected_item_index)
        {
            Rect rect = list->items_bounds[list->hovered_item_index];
            rect.bottom_left.y += list->scroll_top;
            immediate::draw_transparent_rect(rect, hover_colour);
        }

        // Draw the selection highlight for the records.
        if(is_valid_list_index(list->selected_item_index))
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

    float right = dimensions.x - padding.end;

    char32_t prior_char = '\0';
    int stop = MIN(index, string_size(text_block->text));
    Vector2 pen = {padding.start, padding.top + font->line_height};
    FOR_N(i, stop)
    {
        char32_t current = text_block->text[i];
        if(current == '\r')
        {
            pen.x = padding.start;
        }
        else if(current == '\n')
        {
            pen.x = padding.start;
            pen.y += font->line_height;
        }
        else
        {
            bmfont::Glyph* glyph = bmfont::find_glyph(font, current);
            if(pen.x + glyph->x_advance > right)
            {
                pen.x = padding.start;
                pen.y += font->line_height;
            }
            else
            {
                float kerning = bmfont::lookup_kerning(font, prior_char, current);
                pen.x += kerning;
            }
            pen.x += glyph->x_advance;
        }
        prior_char = current;
    }

    return pen;
}

void draw_text_input(Item* item)
{
    ASSERT(item->type == ItemType::Text_Input);

    const Vector4 selection_colour = {1.0f, 1.0f, 1.0f, 0.4f};

    draw_text_block(&item->text_input.text_block, item->bounds);

    // Draw the cursor.
    TextInput* text_input = &item->text_input;
    TextBlock* text_block = &text_input->text_block;
    bmfont::Font* font = text_block->font;

    Vector2 cursor = compute_cursor_position(text_block, item->bounds.dimensions, text_input->cursor_position);
    Vector2 top_left = rect_top_left(item->bounds);
    cursor.x += top_left.x;
    cursor.y = top_left.y - cursor.y;

    Rect rect;
    rect.dimensions = {1.0f, font->line_height};
    rect.bottom_left = cursor;
    immediate::draw_opaque_rect(rect, vector4_white);

    // Draw the selection highlight.
    if(text_input->cursor_position != text_input->selection_start)
    {
        Vector2 start = compute_cursor_position(text_block, item->bounds.dimensions, text_input->selection_start);
        start.x += top_left.x;
        start.y = top_left.y - start.y;

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
            draw_text_input(item);
            break;
        }
    }
}

static void detect_hover(Item* item, Vector2 pointer_position)
{
    switch(item->type)
    {
        case ItemType::Button:
        {
            bool hovered = point_in_rect(item->bounds, pointer_position);
            item->button.hovered = hovered;
            break;
        }
        case ItemType::Container:
        {
            Container* container = &item->container;
            FOR_N(i, container->items_count)
            {
                Item* inside = &container->items[i];
                detect_hover(inside, pointer_position);
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
                }
            }
            break;
        }
        case ItemType::Text_Block:
        case ItemType::Text_Input:
        {
            break;
        }
    }
}

void detect_focus_changes_for_toplevel_containers(Context* context)
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

    bool any_changed = false;
    FOR_N(i, context->toplevel_containers_count)
    {
        Item* item = context->toplevel_containers[i];
        bool hovered = point_in_rect(item->bounds, mouse_position);
        if(hovered)
        {
            if(clicked && !item->unfocusable && !any_changed)
            {
                focus_on_container(context, item);
                any_changed = true;
            }
            detect_hover(item, mouse_position);
        }
    }
    if(clicked && !any_changed)
    {
        focus_on_container(context, nullptr);
    }
}

static void remove_selected_text(TextInput* text_input)
{
    if(text_input->cursor_position != text_input->selection_start)
    {
        remove_substring(text_input->text_block.text, text_input->selection_start, text_input->cursor_position);
        int collapsed = MIN(text_input->selection_start, text_input->cursor_position);
        text_input->cursor_position = collapsed;
        text_input->selection_start = collapsed;
    }
}

void insert_text(TextInput* text_input, const char* text_to_add, Heap* heap)
{
    TextBlock* text_block = &text_input->text_block;

    int text_to_add_size = string_size(text_to_add);
    if(text_to_add_size > 0)
    {
        remove_selected_text(text_input);

        int insert_index = text_input->cursor_position;
        char* text = insert_string(text_block->text, text_to_add, insert_index, heap);
        HEAP_DEALLOCATE(heap, text_block->text);
        text_block->text = text;

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

    char* clipboard = copy_string_to_heap(&text_block->text[start], size, heap);
    bool copied = copy_to_clipboard(platform, clipboard);
    if(!copied)
    {
        HEAP_DEALLOCATE(heap, clipboard);
    }

    return copied;
}

void update_input(Item* item, Context* context, Platform* platform)
{
    switch(item->type)
    {
        case ItemType::Button:
        {
            Button* button = &item->button;
            if(button->hovered && input::get_mouse_clicked(input::MouseButton::Left))
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
                update_input(&container->items[i], context, platform);
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

            if(is_valid_list_index(list->hovered_item_index) && input::get_mouse_clicked(input::MouseButton::Left))
            {
                list->selected_item_index = list->hovered_item_index;

                Event event;
                event.type = EventType::List_Selection;
                event.list_selection.index = list->selected_item_index;
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

            // Type out any new text.
            char* text_to_add = input::get_composed_text();
            insert_text(text_input, text_to_add, context->heap);

            if(input::get_key_auto_repeated(input::Key::Left_Arrow))
            {
                if(input::get_key_modified_by_control(input::Key::Left_Arrow))
                {
                    text_input->cursor_position = find_prior_beginning_of_word(text_block->text, text_input->cursor_position, context->heap);
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
                    text_input->cursor_position = find_next_end_of_word(text_block->text, text_input->cursor_position, context->heap);
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

            if(input::get_key_tapped(input::Key::Home))
            {
                text_input->cursor_position = 0;
                if(!input::get_key_modified_by_shift(input::Key::Home))
                {
                    text_input->selection_start = text_input->cursor_position;
                }
            }

            if(input::get_key_tapped(input::Key::End))
            {
                text_input->cursor_position = string_size(text_block->text);
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
                    start = end + 1;
                }
                else
                {
                    start = text_input->selection_start;
                    end = text_input->cursor_position;
                }
                remove_substring(text_input->text_block.text, start, end);

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
                    start = MAX(end - 1, 0);
                }
                else
                {
                    start = text_input->selection_start;
                    end = text_input->cursor_position;
                }
                remove_substring(text_input->text_block.text, start, end);

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

            if(input::get_key_tapped(input::Key::C) && input::get_key_modified_by_control(input::Key::C))
            {
                copy_selected_text(text_input, platform, context->heap);
            }

            if(input::get_key_tapped(input::Key::X) && input::get_key_modified_by_control(input::Key::X))
            {
                bool copied = copy_selected_text(text_input, platform, context->heap);
                if(copied)
                {
                    remove_selected_text(text_input);
                }
            }

            if(input::get_key_tapped(input::Key::V) && input::get_key_modified_by_control(input::Key::V))
            {
                request_paste_from_clipboard(platform);
            }
            break;
        }
    }
}

void update(Context* context, Platform* platform)
{
    detect_focus_changes_for_toplevel_containers(context);
    if(context->focused_container)
    {
        update_input(context->focused_container, context, platform);
    }
}

} // namespace ui
