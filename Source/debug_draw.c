#include "debug_draw.h"

#include "assert.h"
#include "colours.h"

DebugDraw debug_draw;

void debug_draw_reset()
{
    debug_draw.shapes_count = 0;
    debug_draw.current_colour = float4_white;
}

static void add_shape(DebugDrawShape shape)
{
    ASSERT(debug_draw.shapes_count < DEBUG_DRAW_SHAPE_CAP);
    shape.colour = debug_draw.current_colour;
    debug_draw.shapes[debug_draw.shapes_count] = shape;
    debug_draw.shapes_count += 1;
}

void debug_draw_add_sphere(Sphere sphere)
{
    DebugDrawShape shape;
    shape.sphere = sphere;
    add_shape(shape);
}

void debug_draw_add_line_segment(LineSegment line_segment)
{
    DebugDrawShape shape;
    shape.line_segment = line_segment;
    add_shape(shape);
}

void debug_draw_set_colour(Float4 colour)
{
    debug_draw.current_colour = colour;
}
