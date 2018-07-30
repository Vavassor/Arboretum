#ifndef DEBUG_DRAW_H_
#define DEBUG_DRAW_H_

#include "intersection.h"

#define DEBUG_DRAW_SHAPE_CAP 8

typedef enum DebugDrawShapeType
{
    DEBUG_DRAW_SHAPE_TYPE_LINE_SEGMENT,
    DEBUG_DRAW_SHAPE_TYPE_SPHERE,
} DebugDrawShapeType;

typedef struct DebugDrawShape
{
    union
    {
        Sphere sphere;
        LineSegment line_segment;
    };
    Float4 colour;
    DebugDrawShapeType type;
} DebugDrawShape;

typedef struct DebugDraw
{
    DebugDrawShape shapes[DEBUG_DRAW_SHAPE_CAP];
    Float4 current_colour;
    int shapes_count;
} DebugDraw;

void debug_draw_reset();
void debug_draw_add_sphere(Sphere sphere);
void debug_draw_add_line_segment(LineSegment line_segment);
void debug_draw_set_colour(Float4 colour);

extern DebugDraw debug_draw;

#endif // DEBUG_DRAW_H_
