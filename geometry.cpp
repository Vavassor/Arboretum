#include "geometry.h"

Quad rect_to_quad(Rect r)
{
	float left = r.bottom_left.x;
	float right = r.bottom_left.x + r.dimensions.x;
	float bottom = r.bottom_left.y;
	float top = r.bottom_left.y + r.dimensions.y;

	Quad result;
	result.vertices[0] = {left, bottom, 0.0f};
	result.vertices[1] = {right, bottom, 0.0f};
	result.vertices[2] = {right, top, 0.0f};
	result.vertices[3] = {left, top, 0.0f};
	return result;
}

Vector2 rect_top_left(Rect rect)
{
	return {rect.bottom_left.x, rect.bottom_left.y + rect.dimensions.y};
}

Vector2 rect_top_right(Rect rect)
{
	return rect.bottom_left + rect.dimensions;
}

Vector2 rect_bottom_right(Rect rect)
{
	return {rect.bottom_left.x + rect.dimensions.x, rect.bottom_left.y};
}

float rect_top(Rect rect)
{
	return rect.bottom_left.y + rect.dimensions.y;
}

float rect_right(Rect rect)
{
	return rect.bottom_left.x + rect.dimensions.x;
}

bool point_in_rect(Rect rect, Vector2 point)
{
	Vector2 min = rect.bottom_left;
	Vector2 max = min + rect.dimensions;
	return
		point.x >= min.x && point.x <= max.x &&
		point.y >= min.y && point.y <= max.y;
}
