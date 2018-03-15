#ifndef UI_H_
#define UI_H_

#include "bmfont.h"
#include "geometry.h"
#include "u32_map.h"

struct Platform;
struct Heap;
struct Stack;

namespace ui {

struct Padding
{
    float start;
    float top;
    float end;
    float bottom;

    float& operator [] (int index) {return reinterpret_cast<float*>(this)[index];}
    const float& operator [] (int index) const {return reinterpret_cast<const float*>(this)[index];}
};

enum class Direction
{
    Left_To_Right,
    Right_To_Left,
};

enum class Axis
{
    Horizontal,
    Vertical,
};

enum class Justification
{
    Start,
    End,
    Center,
    Space_Around,
    Space_Between,
};

enum class Alignment
{
    Start,
    End,
    Center,
    Stretch,
};

struct Item;

struct Glyph
{
    Rect rect;
    Rect texture_rect;
    Vector2 baseline_start;
    float x_advance;
    int text_index;
};

struct TextBlock
{
    U32Map glyph_map;
    Padding padding;
    char* text;
    bmfont::Font* font;
    Glyph* glyphs;
    int glyphs_cap;
    int glyphs_count;
};

struct Button
{
    TextBlock text_block;
    bool enabled;
    bool hovered;
};

struct Container
{
    Padding padding;
    Vector4 background_colour;
    Item* items;
    int items_count;
    Direction direction;
    Axis axis;
    Justification justification;
    Alignment alignment;
};

struct List
{
    TextBlock* items;
    Rect* items_bounds;
    float item_spacing;
    float side_margin;
    float scroll_top;
    int items_count;
    int hovered_item_index;
    int selected_item_index;
};

struct TextInput
{
    TextBlock text_block;
    int cursor_position;
    int selection_start;
};

enum class ItemType
{
    Button,
    Container,
    List,
    Text_Block,
    Text_Input,
};

// A UI-Id, User Interface Identifier, not to be confused with a UUID, a
// Universally Unique Identifier. This id is meant to be unique among items
// but not consistent between separate runs of the program.
typedef unsigned int Id;

struct Item
{
    union
    {
        Button button;
        Container container;
        List list;
        TextBlock text_block;
        TextInput text_input;
    };
    Rect bounds;
    Vector2 ideal_dimensions;
    Vector2 min_dimensions;
    ItemType type;
    Id id;
    bool growable;
    bool unfocusable;
};

enum class EventType
{
    Button,
    List_Selection,
    Focus_Change,
};

struct Event
{
    EventType type;
    union
    {
        struct
        {
            Id id;
        } button;

        struct
        {
            int index;
        } list_selection;

        struct
        {
            Id now_focused;
            Id now_unfocused;
        } focus_change;
    };
};

struct EventQueue
{
    Event* events;
    int head;
    int tail;
    int cap;
};

struct Context
{
    EventQueue queue;
    Vector2 viewport;
    Heap* heap;
    Stack* scratch;
    Item* focused_container;
    Item** toplevel_containers;
    int toplevel_containers_count;
    Id seed;
};

bool dequeue(EventQueue* queue, Event* event);
void create_context(Context* context, Heap* heap, Stack* stack);
void destroy_context(Context* context, Heap* heap);
bool focused_on(Context* context, Item* item);
void focus_on_container(Context* context, Item* item);
Item* create_toplevel_container(Context* context, Heap* heap);
void destroy_toplevel_container(Context* context, Item* item, Heap* heap);
void add_row(Container* container, int count, Context* context, Heap* heap);
void add_column(Container* container, int count, Context* context, Heap* heap);
void set_text(TextBlock* text_block, const char* text, Heap* heap);
void insert_text(Item* item, const char* text_to_add, Heap* heap, Stack* stack);
void create_items(Item* item, int lines_count, Heap* heap);
void lay_out(Item* item, Rect space, Stack* stack);
void draw(Item* item, Context* context);

void update(Context* context, Platform* platform);

} // namespace ui

#endif // UI_H_
