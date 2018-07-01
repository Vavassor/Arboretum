#ifndef IMMEDIATE_H_
#define IMMEDIATE_H_

#include "vector_math.h"
#include "geometry.h"
#include "gl_core_3_3.h"
#include "memory.h"

typedef enum BlendMode
{
    BLEND_MODE_NONE,
    BLEND_MODE_OPAQUE,
    BLEND_MODE_TRANSPARENT,
} BlendMode;

void immediate_context_create(Heap* heap);
void immediate_context_destroy(Heap* heap);

void immediate_set_matrices(Matrix4 view, Matrix4 projection);
void immediate_set_shader(GLuint program);
void immediate_set_line_shader(GLuint program);
void immediate_set_textured_shader(GLuint program);
void immediate_set_blend_mode(BlendMode mode);
void immediate_set_text_colour(Float3 text_colour);

void immediate_set_clip_area(Rect rect, int viewport_width, int viewport_height);
void immediate_stop_clip_area();

void immediate_draw();
void immediate_add_line(Float3 start, Float3 end, Float4 colour);
void immediate_add_triangle(Triangle* triangle, Float4 colour);
void immediate_add_rect(Rect rect, Float4 colour);
void immediate_add_wire_rect(Rect rect, Float4 colour);
void immediate_add_quad(Quad* quad, Float4 colour);
void immediate_add_quad_textured(Quad* quad, Rect texture_rect);
void immediate_add_wire_quad(Quad* quad, Float4 colour);
void immediate_add_circle(Float3 center, Float3 axis, float radius, Float4 colour);
void immediate_add_cone(Float3 base_center, Float3 axis, float radius, Float4 side_colour, Float4 base_colour);
void immediate_add_cylinder(Float3 start, Float3 end, float radius, Float4 colour);
void immediate_add_box(Float3 center, Float3 extents, Float4 colour);
void immediate_add_sphere(Float3 center, float radius, Float4 colour);
void immediate_add_arc(Float3 center, Float3 axis, float angle, float start_angle, float radius, float width, Float4 colour);
void immediate_add_wire_arc(Float3 center, Float3 axis, float angle, float start_angle, float radius, Float4 colour);

void immediate_draw_opaque_rect(Rect rect, Float4 colour);
void immediate_draw_transparent_rect(Rect rect, Float4 colour);

#endif // IMMEDIATE_H_
