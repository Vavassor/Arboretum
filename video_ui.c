#include "video_ui.h"

#include "assert.h"
#include "float_utilities.h"
#include "immediate.h"
#include "int_utilities.h"
#include "invalid_index.h"
#include "math_basics.h"
#include "ui_internal.h"

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

static void draw_item_hover_highlight(UiList* list, Float4 hover_colour)
{
    if(is_valid_index(list->hovered_item_index) && list->hovered_item_index != list->selected_item_index)
    {
        Rect rect = list->items_bounds[list->hovered_item_index];
        rect.bottom_left.y += list->scroll_top;
        immediate_draw_transparent_rect(rect, hover_colour);
    }
}

static void draw_item_selection_highlight(UiList* list, Float4 selection_colour)
{
    if(is_valid_index(list->selected_item_index))
    {
        Rect rect = list->items_bounds[list->selected_item_index];
        rect.bottom_left.y += list->scroll_top;
        immediate_draw_transparent_rect(rect, selection_colour);
    }
}

static void draw_items(UiItem* item, float line_height)
{
    ASSERT(item->type == UI_ITEM_TYPE_LIST);

    UiList* list = &item->list;

    float item_height = ui_get_list_item_height(list, &list->items[0], line_height);
    float scroll = list->scroll_top;
    int start_index = (int) (scroll / item_height);
    start_index = imax(start_index, 0);
    int end_index = (int) ceilf((scroll + item->bounds.dimensions.y) / item_height);
    end_index = imin(end_index, list->items_count);

    for(int i = start_index; i < end_index; i += 1)
    {
        UiTextBlock* next = &list->items[i];
        Rect bounds = list->items_bounds[i];
        bounds.bottom_left.y += list->scroll_top;
        draw_text_block(next, bounds);
    }
}

static void draw_list(UiItem* item, UiContext* context)
{
    ASSERT(item->type == UI_ITEM_TYPE_LIST);

    immediate_set_clip_area(item->bounds, (int) context->viewport.x, (int) context->viewport.y);

    UiList* list = &item->list;

    UiTheme* theme = &context->theme;
    float line_height = theme->font->line_height;
    Float4 hover_colour = theme->colours.list_item_background_hovered;
    Float4 selection_colour = theme->colours.list_item_background_selected;

    if(list->items_count > 0)
    {
        draw_item_hover_highlight(list, hover_colour);
        draw_item_selection_highlight(list, selection_colour);
        draw_items(item, line_height);
    }

    immediate_stop_clip_area();
}

static Float2 draw_cursor(UiItem* item, UiContext* context)
{
    ASSERT(item->type == UI_ITEM_TYPE_TEXT_INPUT);

    UiTextInput* text_input = &item->text_input;
    UiTextBlock* text_block = &text_input->text_block;

    Float4 cursor_colour = context->theme.colours.text_input_cursor;
    float line_height = context->theme.font->line_height;

    Float2 cursor = ui_compute_cursor_position(text_block, item->bounds.dimensions, line_height, text_input->cursor_position);
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

    return cursor;
}

static void draw_selection_highlight(UiItem* item, UiContext* context, Float2 cursor)
{
    ASSERT(item->type == UI_ITEM_TYPE_TEXT_INPUT);

    UiTextInput* text_input = &item->text_input;
    UiTextBlock* text_block = &text_input->text_block;

    Float4 selection_colour = context->theme.colours.text_input_selection;
    float line_height = context->theme.font->line_height;

    Float2 start = ui_compute_cursor_position(text_block, item->bounds.dimensions, line_height, text_input->selection_start);
    Float2 top_left = rect_top_left(item->bounds);
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
        Rect rect;
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

        Rect rect;
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

static void draw_selection(UiItem* item, UiContext* context)
{
    ASSERT(item->type == UI_ITEM_TYPE_TEXT_INPUT);

    UiTextInput* text_input = &item->text_input;

    Float2 cursor = draw_cursor(item, context);

    if(text_input->cursor_position != text_input->selection_start)
    {
        draw_selection_highlight(item, context, cursor);
    }
}

static void draw_text_input(UiItem* item, UiContext* context)
{
    ASSERT(item->type == UI_ITEM_TYPE_TEXT_INPUT);

    UiTextInput* text_input = &item->text_input;
    UiTextBlock* text_block = &text_input->text_block;

    bool in_focus = ui_focused_on(context, item);

    if(in_focus || text_block->glyphs_count > 0)
    {
        draw_text_block(text_block, item->bounds);
    }
    else
    {
        // Show the label only when the field isn't focused. If it's been
        // edited keep showing the entered text even if focus leaves.
        draw_text_block(&text_input->label, item->bounds);
    }

    if(in_focus)
    {
        draw_selection(item, context);
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
    if(context->focused_item && ui_in_focus_scope(item, context->focused_item))
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
