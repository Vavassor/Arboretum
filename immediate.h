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
void set_text_colour(Vector3 text_colour);

void set_clip_area(Rect rect, int viewport_width, int viewport_height);
void stop_clip_area();

void draw();
void add_line(Vector3 start, Vector3 end, Vector4 colour);
void add_triangle(Triangle* triangle, Vector4 colour);
void add_rect(Rect rect, Vector4 colour);
void add_wire_rect(Rect rect, Vector4 colour);
void add_quad(Quad* quad, Vector4 colour);
void add_quad_textured(Quad* quad, Rect texture_rect);
void add_wire_quad(Quad* quad, Vector4 colour);
void add_circle(Vector3 center, Vector3 axis, float radius, Vector4 colour);
void add_cone(Vector3 base_center, Vector3 axis, float radius, Vector4 side_colour, Vector4 base_colour);
void add_cylinder(Vector3 start, Vector3 end, float radius, Vector4 colour);
void add_box(Vector3 center, Vector3 extents, Vector4 colour);
void add_sphere(Vector3 center, float radius, Vector4 colour);
void add_arc(Vector3 center, Vector3 axis, float angle, float start_angle, float radius, float width, Vector4 colour);
void add_wire_arc(Vector3 center, Vector3 axis, float angle, float start_angle, float radius, Vector4 colour);

void draw_opaque_rect(Rect rect, Vector4 colour);
void draw_transparent_rect(Rect rect, Vector4 colour);

} // namespace immediate

#endif // IMMEDIATE_H_
