#include "immediate.h"

#include "assert.h"
#include "colours.h"
#include "float_utilities.h"
#include "math_basics.h"
#include "memory.h"
#include "uniform_blocks.h"
#include "vertex_layout.h"

#define CONTEXT_VERTICES_CAP 8192
#define CONTEXT_VERTEX_TYPE_COUNT 3
#define CONTEXT_BLEND_MODE_COUNT 2

typedef enum VertexType
{
    VERTEX_TYPE_NONE,
    VERTEX_TYPE_COLOUR,
    VERTEX_TYPE_LINE,
    VERTEX_TYPE_TEXTURE,
} VertexType;

typedef struct Context
{
    union
    {
        VertexPC vertices[CONTEXT_VERTICES_CAP];
        VertexPT vertices_textured[CONTEXT_VERTICES_CAP];
        LineVertex line_vertices[CONTEXT_VERTICES_CAP];
    };
    Matrix4 view_projection;
    Matrix4 projection;
    Float3 text_colour;
    float line_width;
    BufferId buffers[CONTEXT_VERTEX_TYPE_COUNT];
    BufferId uniform_buffers[2];
    PipelineId pipelines[CONTEXT_BLEND_MODE_COUNT][CONTEXT_VERTEX_TYPE_COUNT];
    PipelineId override_pipeline;
    Backend* backend;
    int filled;
    BlendMode blend_mode;
    VertexType vertex_type;
} Context;

static Context* context;

void immediate_context_create(ImmediateContextSpec* spec, Heap* heap, Log* log)
{
    context = HEAP_ALLOCATE(heap, Context, 1);
    Context* c = context;

    c->backend = spec->backend;
    c->uniform_buffers[0] = spec->uniform_buffers[0];
    c->uniform_buffers[1] = spec->uniform_buffers[1];

    BufferSpec buffer_spec =
    {
        .format = BUFFER_FORMAT_VERTEX,
        .usage = BUFFER_USAGE_DYNAMIC,
        .size = sizeof(c->vertices),
    };
    c->buffers[0] = create_buffer(c->backend, &buffer_spec, log);

    BufferSpec textured_buffer_spec =
    {
        .format = BUFFER_FORMAT_VERTEX,
        .usage = BUFFER_USAGE_DYNAMIC,
        .size = sizeof(c->vertices_textured),
    };
    c->buffers[1] = create_buffer(c->backend, &textured_buffer_spec, log);

    BufferSpec line_buffer_spec =
    {
        .format = BUFFER_FORMAT_VERTEX,
        .usage = BUFFER_USAGE_DYNAMIC,
        .size = sizeof(c->line_vertices),
    };
    c->buffers[2] = create_buffer(c->backend, &line_buffer_spec, log);

    BlendStateSpec blend_state_specs[CONTEXT_BLEND_MODE_COUNT] =
    {
        [0] =
        {
            .enabled = false,
        },
        [1] =
        {
            .enabled = true,
            .alpha_op = BLEND_OP_ADD,
            .alpha_source_factor = BLEND_FACTOR_SRC_ALPHA,
            .alpha_destination_factor = BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
            .rgb_op = BLEND_OP_ADD,
            .rgb_source_factor = BLEND_FACTOR_SRC_ALPHA,
            .rgb_destination_factor = BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        },
    };

    DepthStencilStateSpec depth_stencil_specs[CONTEXT_BLEND_MODE_COUNT] =
    {
        [0] =
        {
            .depth_compare_enabled = true,
            .depth_write_enabled = true,
            .depth_compare_op = COMPARE_OP_LESS_OR_EQUAL,
        },
        [1] =
        {
            .depth_compare_enabled = true,
            .depth_write_enabled = false,
            .depth_compare_op = COMPARE_OP_LESS_OR_EQUAL,
        },
    };

    InputAssemblySpec input_assembly_spec =
    {
        .index_type = INDEX_TYPE_NONE,
    };

    RasterizerStateSpec rasterizer_state_specs[CONTEXT_BLEND_MODE_COUNT] =
    {
        [0] =
        {
            .cull_mode = CULL_MODE_BACK,
        },
        [1] =
        {
            .cull_mode = CULL_MODE_NONE,
        },
    };

    for(int i = 0; i < CONTEXT_BLEND_MODE_COUNT; i += 1)
    {
        PipelineSpec pipeline_spec =
        {
            .blend = blend_state_specs[i],
            .depth_stencil = depth_stencil_specs[i],
            .input_assembly = input_assembly_spec,
            .rasterizer = rasterizer_state_specs[i],
            .shader = spec->shaders[0],
            .vertex_layout =
            {
                .attributes =
                {
                    [0] = {.format = VERTEX_FORMAT_FLOAT3},
                    [1] = {.format = VERTEX_FORMAT_UBYTE4_NORM},
                },
            },
        };
        c->pipelines[i][0] = create_pipeline(c->backend, &pipeline_spec, log);

        PipelineSpec textured_pipeline_spec =
        {
            .blend = blend_state_specs[i],
            .depth_stencil = depth_stencil_specs[i],
            .input_assembly = input_assembly_spec,
            .rasterizer = rasterizer_state_specs[i],
            .shader = spec->shaders[1],
            .vertex_layout =
            {
                .attributes =
                {
                    [0] = {.format = VERTEX_FORMAT_FLOAT3},
                    [1] = {.format = VERTEX_FORMAT_USHORT2_NORM},
                },
            },
        };
        c->pipelines[i][1] = create_pipeline(c->backend, &textured_pipeline_spec, log);

        PipelineSpec line_pipeline_spec =
        {
            .blend = blend_state_specs[i],
            .depth_stencil = depth_stencil_specs[i],
            .input_assembly = input_assembly_spec,
            .rasterizer = rasterizer_state_specs[i],
            .shader = spec->shaders[2],
            .vertex_layout =
            {
                .attributes =
                {
                    [0] = {.format = VERTEX_FORMAT_FLOAT3},
                    [1] = {.format = VERTEX_FORMAT_FLOAT3},
                    [2] = {.format = VERTEX_FORMAT_UBYTE4_NORM},
                    [3] = {.format = VERTEX_FORMAT_USHORT2_NORM},
                    [4] = {.format = VERTEX_FORMAT_FLOAT1},
                },
            },
        };
        c->pipelines[i][2] = create_pipeline(c->backend, &line_pipeline_spec, log);
    }
}

void immediate_context_destroy(Heap* heap)
{
    if(context)
    {
        for(int i = 0; i < CONTEXT_VERTEX_TYPE_COUNT; i += 1)
        {
            destroy_buffer(context->backend, context->buffers[i]);
        }
        for(int i = 0; i < CONTEXT_BLEND_MODE_COUNT; i += 1)
        {
            for(int j = 0; j < CONTEXT_VERTEX_TYPE_COUNT; j += 1)
            {
                destroy_pipeline(context->backend, context->pipelines[i][j]);
            }
        }
        HEAP_DEALLOCATE(heap, context);
    }
}

void immediate_set_matrices(Matrix4 view, Matrix4 projection)
{
    context->view_projection = matrix4_multiply(projection, view);
    context->projection = projection;
}

void immediate_set_blend_mode(BlendMode mode)
{
    context->blend_mode = mode;
}

void immediate_set_line_width(float line_width)
{
    context->line_width = line_width;
}

static void set_vertex_type(VertexType type)
{
    context->vertex_type = type;
}

void immediate_set_text_colour(Float3 text_colour)
{
    context->text_colour = text_colour;
}

void immediate_set_override_pipeline(PipelineId pipeline)
{
    context->override_pipeline = pipeline;
}

void immediate_set_clip_area(Rect rect, int viewport_width, int viewport_height)
{
    ScissorRect scissor_rect =
    {
        .bottom_left =
        {
            .x = rect.bottom_left.x + (viewport_width / 2),
            .y = rect.bottom_left.y + (viewport_height / 2),
        },
        .dimensions = {rect.dimensions.x, rect.dimensions.y},
    };
    set_scissor_rect(context->backend, &scissor_rect);
}

void immediate_stop_clip_area()
{
    set_scissor_rect(context->backend, NULL);
}

static bool is_fresh(Context* c)
{
    return c->filled == 0;
}

static void make_fresh()
{
    immediate_set_blend_mode(BLEND_MODE_NONE);
    immediate_set_text_colour(float3_white);
    immediate_set_line_width(4.0f);
    context->override_pipeline.value = 0;
    context->filled = 0;
}

void immediate_draw()
{
    Context* c = context;
    if(c->filled == 0 || c->vertex_type == VERTEX_TYPE_NONE)
    {
        make_fresh();
        return;
    }

    int vertex_type_index;
    switch(c->vertex_type)
    {
        case VERTEX_TYPE_NONE:
        case VERTEX_TYPE_COLOUR:
        {
            vertex_type_index = 0;
            BufferId buffer = c->buffers[vertex_type_index];
            int filled = sizeof(VertexPC) * c->filled;
            update_buffer(c->backend, buffer, c->vertices, 0, filled);
            break;
        }
        case VERTEX_TYPE_TEXTURE:
        {
            vertex_type_index = 1;
            BufferId buffer = c->buffers[vertex_type_index];
            int filled = sizeof(VertexPT) * c->filled;
            update_buffer(c->backend, buffer, c->vertices_textured, 0, filled);
            break;
        }
        case VERTEX_TYPE_LINE:
        {
            vertex_type_index = 2;
            BufferId buffer = c->buffers[vertex_type_index];
            int filled = sizeof(LineVertex) * c->filled;
            update_buffer(c->backend, buffer, c->line_vertices, 0, filled);
            break;
        }
    }

    int blend_index;
    switch(c->blend_mode)
    {
        case BLEND_MODE_NONE:
        case BLEND_MODE_OPAQUE:
        {
            blend_index = 0;
            break;
        }
        case BLEND_MODE_TRANSPARENT:
        {
            blend_index = 1;
            break;
        }
    }

    if(!c->override_pipeline.value)
    {
        set_pipeline(c->backend, c->pipelines[blend_index][vertex_type_index]);
    }

    if(c->vertex_type == VERTEX_TYPE_TEXTURE)
    {
        PerSpan per_span =
        {
            .tint = c->text_colour,
        };
        update_buffer(c->backend, c->uniform_buffers[1], &per_span, 0, sizeof(per_span));
    }
    else if(c->vertex_type == VERTEX_TYPE_LINE)
    {
        PerLine per_line =
        {
            .line_width = c->line_width,
            .projection_factor = c->projection.e[0],
        };
        update_buffer(c->backend, c->uniform_buffers[0], &per_line, 0, sizeof(per_line));
    }

    DrawAction draw_action =
    {
        .vertex_buffers[0] = c->buffers[vertex_type_index],
        .indices_count = c->filled,
    };
    draw(c->backend, &draw_action);

    make_fresh();
}

void immediate_add_line(Float3 start, Float3 end, Float4 colour)
{
    Context* c = context;
    ASSERT(is_fresh(c) || c->vertex_type == VERTEX_TYPE_LINE);
    ASSERT(c->filled + 6 < CONTEXT_VERTICES_CAP);
    uint32_t colour_u32 = rgba_to_u32(colour);
    uint32_t texcoords[4] =
    {
        texcoord_to_u32((Float2){{0.0f, 0.0f}}),
        texcoord_to_u32((Float2){{0.0f, 1.0f}}),
        texcoord_to_u32((Float2){{1.0f, 1.0f}}),
        texcoord_to_u32((Float2){{1.0f, 0.0f}}),
    };
    Float3 direction = float3_subtract(end, start);
    c->line_vertices[c->filled + 0] = (LineVertex){start, direction, colour_u32, texcoords[1], -1.0f};
    c->line_vertices[c->filled + 1] = (LineVertex){start, direction, colour_u32, texcoords[2], 1.0f};
    c->line_vertices[c->filled + 2] = (LineVertex){end, float3_negate(direction), colour_u32, texcoords[0], 1.0f};
    c->line_vertices[c->filled + 3] = (LineVertex){end, float3_negate(direction), colour_u32, texcoords[0], 1.0f};
    c->line_vertices[c->filled + 4] = (LineVertex){start, direction, colour_u32, texcoords[2], 1.0f};
    c->line_vertices[c->filled + 5] = (LineVertex){end, float3_negate(direction), colour_u32, texcoords[3], -1.0f};
    c->filled += 6;
    set_vertex_type(VERTEX_TYPE_LINE);
}

void immediate_add_triangle(Triangle* triangle, Float4 colour)
{
    Context* c = context;
    ASSERT(is_fresh(c) || c->vertex_type == VERTEX_TYPE_COLOUR);
    ASSERT(c->filled + 3 < CONTEXT_VERTICES_CAP);
    for(int i = 0; i < 3; ++i)
    {
        c->vertices[c->filled + i].position = triangle->vertices[i];
        c->vertices[c->filled + i].colour = rgba_to_u32(colour);
    }
    c->filled += 3;
    set_vertex_type(VERTEX_TYPE_COLOUR);
}

void immediate_add_rect(Rect rect, Float4 colour)
{
    Quad quad = rect_to_quad(rect);
    immediate_add_quad(&quad, colour);
}

void immediate_add_wire_rect(Rect rect, Float4 colour)
{
    Quad quad = rect_to_quad(rect);
    immediate_add_wire_quad(&quad, colour);
}

void immediate_add_quad(Quad* quad, Float4 colour)
{
    Context* c = context;
    ASSERT(is_fresh(c) || c->vertex_type == VERTEX_TYPE_COLOUR);
    ASSERT(c->filled + 6 < CONTEXT_VERTICES_CAP);
    c->vertices[c->filled + 0].position = quad->vertices[0];
    c->vertices[c->filled + 1].position = quad->vertices[1];
    c->vertices[c->filled + 2].position = quad->vertices[2];
    c->vertices[c->filled + 3].position = quad->vertices[0];
    c->vertices[c->filled + 4].position = quad->vertices[2];
    c->vertices[c->filled + 5].position = quad->vertices[3];
    for(int i = 0; i < 6; ++i)
    {
        c->vertices[c->filled + i].colour = rgba_to_u32(colour);
    }
    c->filled += 6;
    set_vertex_type(VERTEX_TYPE_COLOUR);
}

void immediate_add_quad_textured(Quad* quad, Rect texture_rect)
{
    Context* c = context;
    ASSERT(is_fresh(c) || c->vertex_type == VERTEX_TYPE_TEXTURE);
    ASSERT(c->filled + 6 < CONTEXT_VERTICES_CAP);
    c->vertices_textured[c->filled + 0].position = quad->vertices[0];
    c->vertices_textured[c->filled + 1].position = quad->vertices[1];
    c->vertices_textured[c->filled + 2].position = quad->vertices[2];
    c->vertices_textured[c->filled + 3].position = quad->vertices[0];
    c->vertices_textured[c->filled + 4].position = quad->vertices[2];
    c->vertices_textured[c->filled + 5].position = quad->vertices[3];
    const Float2 texcoords[4] =
    {
        {{texture_rect.bottom_left.x, texture_rect.bottom_left.y + texture_rect.dimensions.y}},
        float2_add(texture_rect.bottom_left, texture_rect.dimensions),
        {{texture_rect.bottom_left.x + texture_rect.dimensions.x, texture_rect.bottom_left.y}},
        texture_rect.bottom_left,
    };
    c->vertices_textured[c->filled + 0].texcoord = texcoord_to_u32(texcoords[0]);
    c->vertices_textured[c->filled + 1].texcoord = texcoord_to_u32(texcoords[1]);
    c->vertices_textured[c->filled + 2].texcoord = texcoord_to_u32(texcoords[2]);
    c->vertices_textured[c->filled + 3].texcoord = texcoord_to_u32(texcoords[0]);
    c->vertices_textured[c->filled + 4].texcoord = texcoord_to_u32(texcoords[2]);
    c->vertices_textured[c->filled + 5].texcoord = texcoord_to_u32(texcoords[3]);
    c->filled += 6;
    set_vertex_type(VERTEX_TYPE_TEXTURE);
}

void immediate_add_wire_quad(Quad* quad, Float4 colour)
{
    immediate_add_line(quad->vertices[0], quad->vertices[1], colour);
    immediate_add_line(quad->vertices[1], quad->vertices[2], colour);
    immediate_add_line(quad->vertices[2], quad->vertices[3], colour);
    immediate_add_line(quad->vertices[3], quad->vertices[0], colour);
}

void immediate_add_circle(Float3 center, Float3 axis, float radius, Float4 colour)
{
    const int segments = 16;
    Quaternion orientation = quaternion_axis_angle(axis, 0.0f);
    Float3 arm = float3_multiply(radius, float3_normalise(float3_perp(axis)));
    Float3 position = float3_add(quaternion_rotate(orientation, arm), center);

    for(int i = 1; i <= segments; i += 1)
    {
        Float3 prior = position;
        float t = (((float) i) / segments) * tau;
        orientation = quaternion_axis_angle(axis, t);
        position = float3_add(quaternion_rotate(orientation, arm), center);

        Triangle triangle;
        triangle.vertices[0] = position;
        triangle.vertices[1] = prior;
        triangle.vertices[2] = center;
        immediate_add_triangle(&triangle, colour);
    }
}

void immediate_add_cone(Float3 base_center, Float3 axis, float radius, Float4 side_colour, Float4 base_colour)
{
    const int segments = 16;
    Quaternion orientation = quaternion_axis_angle(axis, 0.0f);
    Float3 arm = float3_multiply(radius, float3_normalise(float3_perp(axis)));
    Float3 position = float3_add(quaternion_rotate(orientation, arm), base_center);
    Float3 apex = float3_add(axis, base_center);

    for(int i = 1; i <= segments; i += 1)
    {
        Float3 prior = position;
        float t = (((float) i) / segments) * tau;
        orientation = quaternion_axis_angle(axis, t);
        position = float3_add(quaternion_rotate(orientation, arm), base_center);

        Triangle triangle;
        triangle.vertices[0] = prior;
        triangle.vertices[1] = position;
        triangle.vertices[2] = apex;
        immediate_add_triangle(&triangle, side_colour);
    }

    immediate_add_circle(base_center, axis, radius, base_colour);
}

void immediate_add_cylinder(Float3 start, Float3 end, float radius, Float4 colour)
{
    const int segments = 16;
    Float3 axis = float3_subtract(end, start);
    Quaternion orientation = quaternion_axis_angle(axis, 0.0f);
    Float3 arm = float3_multiply(radius, float3_normalise(float3_perp(axis)));
    Float3 next_top = float3_add(quaternion_rotate(orientation, arm), start);
    Float3 next_bottom = float3_add(quaternion_rotate(orientation, arm), end);

    for(int i = 1; i <= segments; i += 1)
    {
        Float3 prior_top = next_top;
        Float3 prior_bottom = next_bottom;
        float t = (((float) i) / segments) * tau;
        orientation = quaternion_axis_angle(axis, t);
        next_top = float3_add(quaternion_rotate(orientation, arm), start);
        next_bottom = float3_add(quaternion_rotate(orientation, arm), end);

        Quad quad;
        quad.vertices[0] = prior_top;
        quad.vertices[1] = next_top;
        quad.vertices[2] = next_bottom;
        quad.vertices[3] = prior_bottom;
        immediate_add_quad(&quad, colour);
    }

    immediate_add_circle(start, axis, radius, colour);
    immediate_add_circle(end, float3_negate(axis), radius, colour);
}

void immediate_add_box(Float3 center, Float3 extents, Float4 colour)
{
    // corners
    Float3 c[8] =
    {
        {{+extents.x, +extents.y, +extents.z}},
        {{+extents.x, +extents.y, -extents.z}},
        {{+extents.x, -extents.y, +extents.z}},
        {{+extents.x, -extents.y, -extents.z}},
        {{-extents.x, +extents.y, +extents.z}},
        {{-extents.x, +extents.y, -extents.z}},
        {{-extents.x, -extents.y, +extents.z}},
        {{-extents.x, -extents.y, -extents.z}},
    };
    for(int i = 0; i < 8; i += 1)
    {
        c[i] = float3_add(c[i], center);
    }

    Quad sides[6] =
    {
        {{c[1], c[0], c[2], c[3]}},
        {{c[4], c[5], c[7], c[6]}},
        {{c[0], c[1], c[5], c[4]}},
        {{c[3], c[2], c[6], c[7]}},
        {{c[2], c[0], c[4], c[6]}},
        {{c[1], c[3], c[7], c[5]}},
    };

    immediate_add_quad(&sides[0], colour);
    immediate_add_quad(&sides[1], colour);
    immediate_add_quad(&sides[2], colour);
    immediate_add_quad(&sides[3], colour);
    immediate_add_quad(&sides[4], colour);
    immediate_add_quad(&sides[5], colour);
}

static Float3 polar_to_cartesian(float theta, float phi, float radius)
{
    Float3 point;
    point.x = radius * sinf(theta) * cosf(phi);
    point.y = radius * sinf(theta) * sinf(phi);
    point.z = radius * cosf(theta);
    return point;
}

void immediate_add_sphere(Float3 center, float radius, Float4 colour)
{
    const int meridians = 9;
    const int parallels = 7;
    const int rings = parallels + 1;

    Float3 top = {{0.0f, 0.0f, radius}};
    for(int i = 0; i < meridians; i += 1)
    {
        float theta = 1.0f / ((float) rings) * pi;
        float phi0 = (i    ) / ((float) meridians) * tau;
        float phi1 = (i + 1) / ((float) meridians) * tau;

        Triangle triangle;
        triangle.vertices[0] = float3_add(polar_to_cartesian(theta, phi0, radius), center);
        triangle.vertices[1] = float3_add(polar_to_cartesian(theta, phi1, radius), center);
        triangle.vertices[2] = float3_add(top, center);
        immediate_add_triangle(&triangle, colour);
    }

    Float3 bottom = {{0.0f, 0.0f, -radius}};
    for(int i = 0; i < meridians; i += 1)
    {
        float theta = (parallels) / ((float) rings) * pi;
        float phi0 = (i + 1) / ((float) meridians) * tau;
        float phi1 = (i    ) / ((float) meridians) * tau;

        Triangle triangle;
        triangle.vertices[0] = float3_add(polar_to_cartesian(theta, phi0, radius), center);
        triangle.vertices[1] = float3_add(polar_to_cartesian(theta, phi1, radius), center);
        triangle.vertices[2] = float3_add(bottom, center);
        immediate_add_triangle(&triangle, colour);
    }

    for(int i = 1; i < parallels; i += 1)
    {
        float theta0 = (i    ) / ((float) rings) * pi;
        float theta1 = (i + 1) / ((float) rings) * pi;
        for(int j = 0; j < meridians; j += 1)
        {
            float phi0 = (j    ) / ((float) meridians) * tau;
            float phi1 = (j + 1) / ((float) meridians) * tau;

            Quad quad;
            quad.vertices[0] = float3_add(polar_to_cartesian(theta0, phi0, radius), center);
            quad.vertices[1] = float3_add(polar_to_cartesian(theta1, phi0, radius), center);
            quad.vertices[2] = float3_add(polar_to_cartesian(theta1, phi1, radius), center);
            quad.vertices[3] = float3_add(polar_to_cartesian(theta0, phi1, radius), center);
            immediate_add_quad(&quad, colour);
        }
    }
}

void immediate_add_arc(Float3 center, Float3 axis, float angle, float start_angle, float radius, float width, Float4 colour)
{
    const int segments = 11;

    Float3 arm = float3_multiply(radius, float3_normalise(float3_perp(axis)));
    float half_width = width / 2.0f;
    Quaternion orientation = quaternion_axis_angle(axis, start_angle);
    Float3 medial = quaternion_rotate(orientation, arm);
    Float3 direction = float3_normalise(medial);
    Float3 medial_world = float3_add(medial, center);

    Quad quad;
    quad.vertices[0] = float3_add(float3_multiply(-half_width, direction), medial_world);
    quad.vertices[1] = float3_add(float3_multiply(half_width, direction), medial_world);

    for(int i = 1; i <= segments; i += 1)
    {
        float theta = (i / ((float) segments) * angle) + start_angle;
        orientation = quaternion_axis_angle(axis, theta);
        medial = quaternion_rotate(orientation, arm);
        direction = float3_normalise(medial);
        medial_world = float3_add(medial, center);

        quad.vertices[2] = float3_add(float3_multiply(half_width, direction), medial_world);
        quad.vertices[3] = float3_add(float3_multiply(-half_width, direction), medial_world);
        immediate_add_quad(&quad, colour);

        quad.vertices[0] = quad.vertices[3];
        quad.vertices[1] = quad.vertices[2];
    }
}

void immediate_add_wire_arc(Float3 center, Float3 axis, float angle, float start_angle, float radius, Float4 colour)
{
    const int segments = 11;

    Float3 arm = float3_normalise(float3_perp(axis));
    Quaternion orientation = quaternion_axis_angle(axis, start_angle);
    Float3 medial = quaternion_rotate(orientation, arm);
    Float3 direction = float3_normalise(medial);
    Float3 start = float3_add(float3_multiply(radius, direction), center);

    for(int i = 1; i <= segments; i += 1)
    {
        float theta = (i / ((float) segments) * angle) + start_angle;
        orientation = quaternion_axis_angle(axis, theta);
        medial = quaternion_rotate(orientation, arm);
        direction = float3_normalise(medial);

        Float3 end = float3_add(float3_multiply(radius, direction), center);
        immediate_add_line(start, end, colour);

        start = end;
    }
}

void immediate_draw_opaque_rect(Rect rect, Float4 colour)
{
    immediate_add_rect(rect, colour);
    immediate_draw();
}

void immediate_draw_transparent_rect(Rect rect, Float4 colour)
{
    immediate_set_blend_mode(BLEND_MODE_TRANSPARENT);
    immediate_add_rect(rect, colour);
    immediate_draw();
}
