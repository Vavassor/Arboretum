#ifndef UI_H_
#define UI_H_

#include "bmfont.h"
#include "geometry.h"
#include "memory.h"
#include "map.h"
#include "platform.h"

typedef union UiPadding
{
    struct
    {
        float start;
        float top;
        float end;
        float bottom;
    };
    float e[4];
} UiPadding;

typedef enum UiDirection
{
    UI_DIRECTION_LEFT_TO_RIGHT,
    UI_DIRECTION_RIGHT_TO_LEFT,
} UiDirection;

typedef enum UiAxis
{
    UI_AXIS_HORIZONTAL,
    UI_AXIS_VERTICAL,
} UiAxis;

typedef enum UiJustification
{
    UI_JUSTIFICATION_START,
    UI_JUSTIFICATION_END,
    UI_JUSTIFICATION_CENTER,
    UI_JUSTIFICATION_SPACE_AROUND,
    UI_JUSTIFICATION_SPACE_BETWEEN,
} UiJustification;

typedef enum UiAlignment
{
    UI_ALIGNMENT_START,
    UI_ALIGNMENT_END,
    UI_ALIGNMENT_CENTER,
    UI_ALIGNMENT_STRETCH,
} UiAlignment;

typedef enum UiTextOverflow
{
    UI_TEXT_OVERFLOW_WRAP,
    UI_TEXT_OVERFLOW_ELLIPSIZE_END,
} UiTextOverflow;

typedef struct UiItem UiItem;

typedef struct UiGlyph
{
    Rect rect;
    Rect texture_rect;
    Float2 baseline_start;
    float x_advance;
    int text_index;
} UiGlyph;

typedef struct UiTextBlock
{
    Map glyph_map;
    UiPadding padding;
    char* text;
    UiGlyph* glyphs;
    int glyphs_cap;
    int glyphs_count;
    UiTextOverflow text_overflow;
} UiTextBlock;

typedef struct UiButton
{
    UiTextBlock text_block;
    bool enabled;
    bool hovered;
} UiButton;

typedef enum UiStyleType
{
    UI_STYLE_TYPE_DEFAULT,
    UI_STYLE_TYPE_FOOTER,
    UI_STYLE_TYPE_MENU_BAR,
    UI_STYLE_TYPE_PATH_BAR,
} UiStyleType;

typedef struct UiContainer
{
    UiPadding padding;
    UiItem* items;
    int items_count;
    UiStyleType style_type;
    UiDirection direction;
    UiAxis axis;
    UiJustification justification;
    UiAlignment alignment;
} UiContainer;

typedef struct UiList
{
    UiTextBlock* items;
    Rect* items_bounds;
    float item_spacing;
    float side_margin;
    float scroll_top;
    int items_count;
    int hovered_item_index;
    int selected_item_index;
} UiList;

typedef struct UiTextInput
{
    UiTextBlock text_block;
    UiTextBlock label;
    int cursor_position;
    int selection_start;
    int cursor_blink_frame;
} UiTextInput;

typedef enum UiItemType
{
    UI_ITEM_TYPE_BUTTON,
    UI_ITEM_TYPE_CONTAINER,
    UI_ITEM_TYPE_LIST,
    UI_ITEM_TYPE_TEXT_BLOCK,
    UI_ITEM_TYPE_TEXT_INPUT,
} UiItemType;

// A UI-Id, User Interface Identifier, not to be confused with a UUID, a
// Universally Unique Identifier. This id is meant to be unique among items
// but not consistent between separate runs of the program.
typedef unsigned int UiId;

struct UiItem
{
    union
    {
        UiButton button;
        UiContainer container;
        UiList list;
        UiTextBlock text_block;
        UiTextInput text_input;
    };
    Rect bounds;
    Float2 ideal_dimensions;
    Float2 min_dimensions;
    UiItemType type;
    UiId id;
    bool growable;
};

typedef enum UiEventType
{
    UI_EVENT_TYPE_BUTTON,
    UI_EVENT_TYPE_FOCUS_CHANGE,
    UI_EVENT_TYPE_LIST_SELECTION,
    UI_EVENT_TYPE_TEXT_CHANGE,
} UiEventType;

typedef struct UiEvent
{
    UiEventType type;
    union
    {
        struct
        {
            UiId id;
        } button;

        struct
        {
            UiId now_focused;
            UiId now_unfocused;
            UiId current_scope;
        } focus_change;

        struct
        {
            int index;
            bool expand;
        } list_selection;

        struct
        {
            UiId id;
        } text_change;
    };
} UiEvent;

typedef struct UiEventQueue
{
    UiEvent* events;
    int head;
    int tail;
    int cap;
} UiEventQueue;

typedef struct UiStyle
{
    Float4 background;
} UiStyle;

typedef struct UiTheme
{
    BmfFont* font;

    UiStyle styles[4];

    struct
    {
        Float4 button_cap_disabled;
        Float4 button_cap_enabled;
        Float4 button_cap_hovered_disabled;
        Float4 button_cap_hovered_enabled;
        Float3 button_label_disabled;
        Float3 button_label_enabled;
        Float4 focus_indicator;
        Float4 list_item_background_hovered;
        Float4 list_item_background_selected;
        Float4 text_input_cursor;
        Float4 text_input_selection;
    } colours;
} UiTheme;

typedef struct UiContext
{
    UiTheme theme;
    UiEventQueue queue;
    Float2 viewport;
    Heap* heap;
    Stack* scratch;
    UiItem* focused_item;
    UiItem* captor_item;
    UiItem** tab_navigation_list;
    UiItem** toplevel_containers;
    UiId seed;

    // This is for detecting when the mouse cursor is hovering over nothing
    // in particular, to set the mouse cursor shape to its default (arrow).
    bool anything_hovered;
} UiContext;

bool ui_dequeue(UiEventQueue* queue, UiEvent* event);
void ui_create_context(UiContext* context, Heap* heap, Stack* stack);
void ui_destroy_context(UiContext* context, Heap* heap);
bool ui_focused_on(UiContext* context, UiItem* item);
void ui_focus_on_item(UiContext* context, UiItem* item);
bool ui_is_captor(UiContext* context, UiItem* item);
void ui_capture(UiContext* context, UiItem* item);
bool ui_is_within_container(UiItem* outer, UiItem* item);
UiItem* ui_create_toplevel_container(UiContext* context, Heap* heap);
void ui_destroy_toplevel_container(UiContext* context, UiItem* item, Heap* heap);
void ui_empty_item(UiContext* context, UiItem* item);
void ui_add_row(UiContainer* container, int count, UiContext* context, Heap* heap);
void ui_add_column(UiContainer* container, int count, UiContext* context, Heap* heap);
void ui_set_text(UiTextBlock* text_block, const char* text, Heap* heap);
void ui_create_items(UiItem* item, int lines_count, Heap* heap);
void ui_lay_out(UiItem* item, Rect space, UiContext* context);

void ui_update(UiContext* context, Platform* platform);
void ui_accept_paste_from_clipboard(UiContext* context, const char* clipboard, Platform* platform);

extern const UiId ui_invalid_id;

#endif // UI_H_
