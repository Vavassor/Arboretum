#include "immediate.h"

#include "assert.h"
#include "colours.h"
#include "float_utilities.h"
#include "gl_core_3_3.h"
#include "math_basics.h"
#include "memory.h"
#include "vertex_layout.h"

#define CONTEXT_VERTICES_CAP 8192
#define CONTEXT_VERTEX_TYPE_COUNT 3

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
    GLuint vertex_arrays[CONTEXT_VERTEX_TYPE_COUNT];
    GLuint buffers[CONTEXT_VERTEX_TYPE_COUNT];
    GLuint shaders[CONTEXT_VERTEX_TYPE_COUNT];
    int filled;
    BlendMode blend_mode;
    VertexType vertex_type;
    bool blend_mode_changed;
} Context;

static Context* context;

void immediate_context_create(Heap* heap)
{
    context = HEAP_ALLOCATE(heap, Context, 1);
    Context* c = context;

    glGenVertexArrays(CONTEXT_VERTEX_TYPE_COUNT, c->vertex_arrays);
    glGenBuffers(CONTEXT_VERTEX_TYPE_COUNT, c->buffers);

    glBindVertexArray(c->vertex_arrays[0]);
    glBindBuffer(GL_ARRAY_BUFFER, c->buffers[0]);

    glBufferData(GL_ARRAY_BUFFER, sizeof(c->vertices), NULL, GL_DYNAMIC_DRAW);

    GLvoid* offset0 = (GLvoid*) offsetof(VertexPC, position);
    GLvoid* offset1 = (GLvoid*) offsetof(VertexPC, colour);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(VertexPC), offset0);
    glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(VertexPC), offset1);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(2);

    glBindVertexArray(c->vertex_arrays[1]);
    glBindBuffer(GL_ARRAY_BUFFER, c->buffers[1]);

    glBufferData(GL_ARRAY_BUFFER, sizeof(c->line_vertices), NULL, GL_DYNAMIC_DRAW);

    offset0 = (GLvoid*) offsetof(LineVertex, position);
    offset1 = (GLvoid*) offsetof(LineVertex, direction);
    GLvoid* offset2 = (GLvoid*) offsetof(LineVertex, colour);
    GLvoid* offset3 = (GLvoid*) offsetof(LineVertex, texcoord);
    GLvoid* offset4 = (GLvoid*) offsetof(LineVertex, side);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(LineVertex), offset0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(LineVertex), offset1);
    glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(LineVertex), offset2);
    glVertexAttribPointer(3, 2, GL_UNSIGNED_SHORT, GL_TRUE, sizeof(LineVertex), offset3);
    glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, sizeof(LineVertex), offset4);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
    glEnableVertexAttribArray(3);
    glEnableVertexAttribArray(4);

    glBindVertexArray(c->vertex_arrays[2]);
    glBindBuffer(GL_ARRAY_BUFFER, c->buffers[2]);

    glBufferData(GL_ARRAY_BUFFER, sizeof(c->vertices_textured), NULL, GL_DYNAMIC_DRAW);

    offset0 = (GLvoid*) offsetof(VertexPT, position);
    offset1 = (GLvoid*) offsetof(VertexPT, texcoord);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(VertexPT), offset0);
    glVertexAttribPointer(3, 2, GL_UNSIGNED_SHORT, GL_TRUE, sizeof(VertexPT), offset1);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(3);

    glBindVertexArray(0);
}

void immediate_context_destroy(Heap* heap)
{
    if(context)
    {
        glDeleteBuffers(CONTEXT_VERTEX_TYPE_COUNT, context->buffers);
        glDeleteVertexArrays(CONTEXT_VERTEX_TYPE_COUNT, context->vertex_arrays);
        HEAP_DEALLOCATE(heap, context);
    }
}

void immediate_set_matrices(Matrix4 view, Matrix4 projection)
{
    context->view_projection = matrix4_multiply(projection, view);
    context->projection = projection;
}

void immediate_set_shader(GLuint program)
{
    context->shaders[0] = program;
}

void immediate_set_line_shader(GLuint program)
{
    context->shaders[1] = program;
}

void immediate_set_textured_shader(GLuint program)
{
    context->shaders[2] = program;
}

void immediate_set_blend_mode(BlendMode mode)
{
    if(context->blend_mode != mode)
    {
        context->blend_mode = mode;
        context->blend_mode_changed = true;
    }
}

void immediate_set_text_colour(Float3 text_colour)
{
    context->text_colour = text_colour;
}

void immediate_set_clip_area(Rect rect, int viewport_width, int viewport_height)
{
    glEnable(GL_SCISSOR_TEST);
    int x = rect.bottom_left.x + (viewport_width / 2);
    int y = rect.bottom_left.y + (viewport_height / 2);
    glScissor(x, y, rect.dimensions.x, rect.dimensions.y);
}

void immediate_stop_clip_area()
{
    glDisable(GL_SCISSOR_TEST);
}

void immediate_draw()
{
    Context* c = context;
    if(c->filled == 0 || c->vertex_type == VERTEX_TYPE_NONE)
    {
        return;
    }

    if(c->blend_mode_changed)
    {
        switch(c->blend_mode)
        {
            case BLEND_MODE_NONE:
            case BLEND_MODE_OPAQUE:
            {
                glDisable(GL_BLEND);
                glDepthMask(GL_TRUE);
                glEnable(GL_CULL_FACE);
                break;
            }
            case BLEND_MODE_TRANSPARENT:
            {
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                glDepthMask(GL_FALSE);
                glDisable(GL_CULL_FACE);
                break;
            }
        }
        c->blend_mode_changed = false;
    }

    GLuint shader;
    switch(c->vertex_type)
    {
        case VERTEX_TYPE_NONE:
        case VERTEX_TYPE_COLOUR:
        {
            glBindBuffer(GL_ARRAY_BUFFER, c->buffers[0]);
            glBufferData(GL_ARRAY_BUFFER, sizeof(VertexPC) * c->filled, c->vertices, GL_DYNAMIC_DRAW);
            glBindVertexArray(c->vertex_arrays[0]);
            shader = c->shaders[0];
            break;
        }
        case VERTEX_TYPE_LINE:
        {
            glBindBuffer(GL_ARRAY_BUFFER, c->buffers[1]);
            glBufferData(GL_ARRAY_BUFFER, sizeof(LineVertex) * c->filled, c->line_vertices, GL_DYNAMIC_DRAW);
            glBindVertexArray(c->vertex_arrays[1]);
            shader = c->shaders[1];
            break;
        }
        case VERTEX_TYPE_TEXTURE:
        {
            glBindBuffer(GL_ARRAY_BUFFER, c->buffers[2]);
            glBufferData(GL_ARRAY_BUFFER, sizeof(VertexPT) * c->filled, c->vertices_textured, GL_DYNAMIC_DRAW);
            glBindVertexArray(c->vertex_arrays[2]);
            shader = c->shaders[2];
            break;
        }
    }

    glUseProgram(shader);
    GLint location = glGetUniformLocation(shader, "model_view_projection");
    glUniformMatrix4fv(location, 1, GL_TRUE, c->view_projection.e);

    location = glGetUniformLocation(shader, "text_colour");
    if(location != -1)
    {
        glUniform3fv(location, 1, &c->text_colour.e[0]);
    }

    location = glGetUniformLocation(shader, "projection_factor");
    if(location != -1)
    {
        glUniform1f(location, c->projection.e[0]);
    }

    glDrawArrays(GL_TRIANGLES, 0, c->filled);

    immediate_set_blend_mode(BLEND_MODE_NONE);
    immediate_set_text_colour(float3_white);
    c->vertex_type = VERTEX_TYPE_NONE;
    c->filled = 0;
}

void immediate_add_line(Float3 start, Float3 end, Float4 colour)
{
    Context* c = context;
    ASSERT(c->vertex_type == VERTEX_TYPE_LINE || c->vertex_type == VERTEX_TYPE_NONE);
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
    c->vertex_type = VERTEX_TYPE_LINE;
}

void immediate_add_triangle(Triangle* triangle, Float4 colour)
{
    Context* c = context;
    ASSERT(c->vertex_type == VERTEX_TYPE_COLOUR || c->vertex_type == VERTEX_TYPE_NONE);
    ASSERT(c->filled + 3 < CONTEXT_VERTICES_CAP);
    for(int i = 0; i < 3; ++i)
    {
        c->vertices[c->filled + i].position = triangle->vertices[i];
        c->vertices[c->filled + i].colour = rgba_to_u32(colour);
    }
    c->filled += 3;
    c->vertex_type = VERTEX_TYPE_COLOUR;
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
    ASSERT(c->vertex_type == VERTEX_TYPE_COLOUR || c->vertex_type == VERTEX_TYPE_NONE);
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
    c->vertex_type = VERTEX_TYPE_COLOUR;
}

void immediate_add_quad_textured(Quad* quad, Rect texture_rect)
{
    Context* c = context;
    ASSERT(c->vertex_type == VERTEX_TYPE_TEXTURE || c->vertex_type == VERTEX_TYPE_NONE);
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
    c->vertex_type = VERTEX_TYPE_TEXTURE;
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
