#ifndef GEOMETRY_H_
#define GEOMETRY_H_

#include "vector_math.h"

#include <stdbool.h>

typedef struct Rect
{
    Float2 bottom_left;
    Float2 dimensions;
} Rect;

typedef struct Triangle
{
    // assumes counter-clockwise winding for the front face
    Float3 vertices[3];
} Triangle;

typedef struct Quad
{
    // assumes counter-clockwise winding for the front face
    // also, there's nothing guaranteeing these are coplanar
    Float3 vertices[4];
} Quad;

Quad rect_to_quad(Rect r);
Float2 rect_top_left(Rect rect);
Float2 rect_top_right(Rect rect);
Float2 rect_bottom_right(Rect rect);
float rect_top(Rect rect);
float rect_right(Rect rect);
bool point_in_rect(Rect rect, Float2 point);
bool clip_rects(Rect inner, Rect outer, Rect* result);

#endif // GEOMETRY_H_
