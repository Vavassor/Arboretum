#ifndef UI_H_
#define UI_H_

#include "geometry.h"
#include "bmfont.h"

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

struct TextBlock
{
	Padding padding;
	char* text;
	bmfont::Font* font;
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

struct Button
{
	TextBlock text_block;
	bool hovered;
};

enum class ItemType
{
	Container,
	Text_Block,
	Button,
};

struct Item
{
	union
	{
		Container container;
		TextBlock text_block;
		Button button;
	};
	Rect bounds;
	Vector2 ideal_dimensions;
	Vector2 min_dimensions;
	ItemType type;
	bool growable;
	bool unfocusable;
};

void destroy_container(Container* container, Heap* heap);
void add_row(Container* container, int count, Heap* heap);
void add_column(Container* container, int count, Heap* heap);
void lay_out(Item* item, Rect space);
void draw(Item* item);
void detect_hover(Item* item, Vector2 pointer_position);

} // namespace ui

#endif // UI_H_
