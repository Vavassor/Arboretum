// Immediate Mode Drawing
//
// Immediate Mode refers to a descriptive style of interface where visuals are
// drawn on demand. This is in contrast to Retained Mode, where a model of
// objects to be drawn is maintained between frames.
//
// Immediate mode is convenient in that it frees the user from needing to do
// up-front setup of objects or manage object lifetimes. But it's generally
// slower than retained mode. So it should be reserved for convenient debug
// visualizations or shapes that are being completely regenerated every frame
// anyhow.

#ifndef IMMEDIATE_H_
#define IMMEDIATE_H_

#include "vector_math.h"
#include "geometry.h"
#include "memory.h"
#include "video_internal.h"

typedef enum BlendMode
{
    BLEND_MODE_NONE,
    BLEND_MODE_OPAQUE,
    BLEND_MODE_TRANSPARENT,
} BlendMode;

typedef struct ImmediateContextSpec
{
    ShaderId shaders[3];
    BufferId uniform_buffers[2];
    Backend* backend;
} ImmediateContextSpec;

void immediate_context_create(ImmediateContextSpec* spec, Heap* heap, Log* log);
void immediate_context_destroy(Heap* heap);

void immediate_set_matrices(Matrix4 view, Matrix4 projection);
void immediate_set_blend_mode(BlendMode mode);
void immediate_set_line_width(float line_width);
void immediate_set_text_colour(Float3 text_colour);
void immediate_set_override_pipeline(PipelineId pipeline);

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
