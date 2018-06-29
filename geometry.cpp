#include "geometry.h"

#include "math_basics.h"

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

Float2 rect_top_left(Rect rect)
{
    return {rect.bottom_left.x, rect.bottom_left.y + rect.dimensions.y};
}

Float2 rect_top_right(Rect rect)
{
    return float2_add(rect.bottom_left, rect.dimensions);
}

Float2 rect_bottom_right(Rect rect)
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

bool point_in_rect(Rect rect, Float2 point)
{
    Float2 min = rect.bottom_left;
    Float2 max = float2_add(min, rect.dimensions);
    return
        point.x >= min.x && point.x <= max.x &&
        point.y >= min.y && point.y <= max.y;
}

bool clip_rects(Rect inner, Rect outer, Rect* result)
{
    float i_right = inner.bottom_left.x + inner.dimensions.x;
    float o_right = outer.bottom_left.x + outer.dimensions.x;
    i_right = fmin(i_right, o_right);

    float i_top = inner.bottom_left.y + inner.dimensions.y;
    float o_top = outer.bottom_left.y + outer.dimensions.y;
    i_top = fmin(i_top, o_top);

    float x = fmax(inner.bottom_left.x, outer.bottom_left.x);
    float y = fmax(inner.bottom_left.y, outer.bottom_left.y);
    float width = i_right - x;
    float height = i_top - y;

    if(width <= 0.0f && height <= 0.0f)
    {
        return false;
    }
    else
    {
        result->bottom_left.x = x;
        result->bottom_left.y = y;
        result->dimensions.x = width;
        result->dimensions.y = height;
        return true;
    }
}
