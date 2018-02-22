#ifndef GEOMETRY_H_
#define GEOMETRY_H_

#include "vector_math.h"

struct Rect
{
    Vector2 bottom_left;
    Vector2 dimensions;
};

struct Triangle
{
    // assumes counter-clockwise winding for the front face
    Vector3 vertices[3];
};

struct Quad
{
    // assumes counter-clockwise winding for the front face
    // also, there's nothing guaranteeing these are coplanar
    Vector3 vertices[4];
};

Quad rect_to_quad(Rect r);
Vector2 rect_top_left(Rect rect);
Vector2 rect_top_right(Rect rect);
Vector2 rect_bottom_right(Rect rect);
float rect_top(Rect rect);
float rect_right(Rect rect);
bool point_in_rect(Rect rect, Vector2 point);
bool clip_rects(Rect inner, Rect outer, Rect* result);

#endif // GEOMETRY_H_
