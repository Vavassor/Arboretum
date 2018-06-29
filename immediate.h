#ifndef IMMEDIATE_H_
#define IMMEDIATE_H_

#include "vector_math.h"
#include "geometry.h"
#include "gl_core_3_3.h"

struct Heap;

namespace immediate {

enum class BlendMode
{
    None,
    Opaque,
    Transparent,
};

void context_create(Heap* heap);
void context_destroy(Heap* heap);

void set_matrices(Matrix4 view, Matrix4 projection);
void set_shader(GLuint program);
void set_line_shader(GLuint program);
void set_textured_shader(GLuint program);
void set_blend_mode(BlendMode mode);
void set_text_colour(Float3 text_colour);

void set_clip_area(Rect rect, int viewport_width, int viewport_height);
void stop_clip_area();

void draw();
void add_line(Float3 start, Float3 end, Float4 colour);
void add_triangle(Triangle* triangle, Float4 colour);
void add_rect(Rect rect, Float4 colour);
void add_wire_rect(Rect rect, Float4 colour);
void add_quad(Quad* quad, Float4 colour);
void add_quad_textured(Quad* quad, Rect texture_rect);
void add_wire_quad(Quad* quad, Float4 colour);
void add_circle(Float3 center, Float3 axis, float radius, Float4 colour);
void add_cone(Float3 base_center, Float3 axis, float radius, Float4 side_colour, Float4 base_colour);
void add_cylinder(Float3 start, Float3 end, float radius, Float4 colour);
void add_box(Float3 center, Float3 extents, Float4 colour);
void add_sphere(Float3 center, float radius, Float4 colour);
void add_arc(Float3 center, Float3 axis, float angle, float start_angle, float radius, float width, Float4 colour);
void add_wire_arc(Float3 center, Float3 axis, float angle, float start_angle, float radius, Float4 colour);

void draw_opaque_rect(Rect rect, Float4 colour);
void draw_transparent_rect(Rect rect, Float4 colour);

} // namespace immediate

#endif // IMMEDIATE_H_
