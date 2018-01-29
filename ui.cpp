#include "ui.h"

#include "memory.h"
#include "math_basics.h"
#include "vector_math.h"
#include "string_utilities.h"
#include "immediate.h"
#include "input.h"
#include "logging.h"
#include "assert.h"
#include "loop_macros.h"

namespace ui {

void destroy_container(Container* container, Heap* heap)
{
	if(container)
	{
		FOR_N(i, container->items_count)
		{
			Item* item = &container->items[i];
			if(item->type == ItemType::Container)
			{
				destroy_container(container, heap);
			}
		}
		SAFE_HEAP_DEALLOCATE(heap, container->items);
	}
}

void add_row(Container* container, int count, Heap* heap)
{
	container->axis = Axis::Horizontal;
	container->items = HEAP_ALLOCATE(heap, Item, count);
	container->items_count = count;
}

void add_column(Container* container, int count, Heap* heap)
{
	container->axis = Axis::Vertical;
	container->items = HEAP_ALLOCATE(heap, Item, count);
	container->items_count = count;
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
			case ItemType::Container:
			{
				item_ideal = measure_ideal_dimensions(&item->container);
				break;
			}
			case ItemType::Text_Block:
			{
				item_ideal = measure_ideal_dimensions(&item->text_block);
				break;
			}
			case ItemType::Button:
			{
				item_ideal = measure_ideal_dimensions(&item->button.text_block);
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

static void stretch_items_crosswise(Container* container, float length)
{
	if(container->alignment == Alignment::Stretch)
	{
		int axis = get_cross_axis_index(container->axis);
		float padded_length = length - padding_along_axis(container->padding, axis);
		FOR_N(i, container->items_count)
		{
			Item* item = &container->items[i];
			item->bounds.dimensions[axis] = padded_length;
			if(item->type == ItemType::Container)
			{
				stretch_items_crosswise(&item->container, item->bounds.dimensions[axis]);
			}
		}
	}
}

static Vector2 measure_bound_dimensions(Container* container, Vector2 container_space, bool container_growable, float shrink, Axis shrink_along)
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
			case ItemType::Container:
			{
				dimensions = measure_bound_dimensions(&item->container, space, item->growable, shrink, shrink_along);
				break;
			}
			case ItemType::Text_Block:
			{
				dimensions = measure_bound_dimensions(&item->text_block, space);
				break;
			}
			case ItemType::Button:
			{
				dimensions = measure_bound_dimensions(&item->button.text_block, space);
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
	if(container_growable && result[main_axis] < container_space[main_axis])
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

static void commit_ideal_dimensions(Item* item)
{
	Container* container = &item->container;
	item->bounds.dimensions = item->ideal_dimensions;
	FOR_N(i, container->items_count)
	{
		Item* next = &container->items[i];
		next->bounds.dimensions = next->ideal_dimensions;
		if(next->type == ItemType::Container && next->container.items_count > 0)
		{
			commit_ideal_dimensions(next);
		}
	}
}

static void grow_or_commit_ideal_dimensions(Item* item, Vector2 container_space)
{
	Container* container = &item->container;
	int main_axis = get_main_axis_index(container->axis);
	int cross_axis = get_cross_axis_index(container->axis);

	if(item->growable)
	{
		item->bounds.dimensions[main_axis] = container_space[main_axis];
		item->bounds.dimensions[cross_axis] = item->ideal_dimensions[cross_axis];

		// Commit the ideal dimensions for items that won't grow and
		// record how much space they took up.
		float ungrowable_length = 0.0f;
		float ideal_length = 0.0f;
		FOR_N(i, container->items_count)
		{
			Item* next = &container->items[i];
			if(!next->growable)
			{
				commit_ideal_dimensions(next);
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
			if(next->growable)
			{
				Vector2 space;
				space[main_axis] = grow * next->ideal_dimensions[main_axis];
				space[cross_axis] = next->ideal_dimensions[cross_axis];
				switch(next->type)
				{
					case ItemType::Container:
					{
						grow_or_commit_ideal_dimensions(next, space);
						break;
					}
					case ItemType::Text_Block:
					{
						next->bounds.dimensions = space;
						break;
					}
				}
			}
		}
	}
	else
	{
		commit_ideal_dimensions(item);
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
	int cross_axis = get_cross_axis_index(container->axis);

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

		Vector2 bound = measure_bound_dimensions(container, space.dimensions, item->growable, shrink, container->axis);
		if(item->growable)
		{
			bound[main_axis] = space.dimensions[main_axis];
		}
		item->bounds.dimensions = max2(bound, item->min_dimensions);
	}

	stretch_items_crosswise(container, item->bounds.dimensions[cross_axis]);

	// Now that all the items have been sized, just place them where they need to be.
	place_items_along_main_axis(item, space);
	place_items_along_cross_axis(item, space);
}

static void draw_container(Item* item)
{
	ASSERT(item->type == ItemType::Container);

	immediate::draw_opaque_rect(item->bounds, item->container.background_colour);

	FOR_N(i, item->container.items_count)
	{
		draw(&item->container.items[i]);
	}
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

void draw(Item* item)
{
	switch(item->type)
	{
		case ItemType::Container:
		{
			draw_container(item);
			break;
		}
		case ItemType::Text_Block:
		{
			draw_text_block(&item->text_block, item->bounds);
			break;
		}
		case ItemType::Button:
		{
			draw_button(item);
			break;
		}
	}
}

void detect_hover(Item* item, Vector2 pointer_position)
{
	switch(item->type)
	{
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
		case ItemType::Button:
		{
			bool hovered = point_in_rect(item->bounds, pointer_position);
			item->button.hovered = hovered;
			break;
		}
		default:
		{
			break;
		}
	}
}

} // namespace ui
