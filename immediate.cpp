#include "immediate.h"

#include "assert.h"
#include "colours.h"
#include "float_utilities.h"
#include "gl_core_3_3.h"
#include "math_basics.h"
#include "memory.h"
#include "vertex_layout.h"

namespace immediate {

enum class VertexType
{
    None,
    Colour,
    Line,
    Texture,
};

struct Context;

namespace
{
    const int context_vertices_cap = 8192;
    const int context_vertex_type_count = 3;
    Context* context;
}

struct Context
{
    union
    {
        VertexPC vertices[context_vertices_cap];
        VertexPT vertices_textured[context_vertices_cap];
        LineVertex line_vertices[context_vertices_cap];
    };
    Matrix4 view_projection;
    Matrix4 projection;
    Vector3 text_colour;
    GLuint vertex_arrays[context_vertex_type_count];
    GLuint buffers[context_vertex_type_count];
    GLuint shaders[context_vertex_type_count];
    int filled;
    BlendMode blend_mode;
    VertexType vertex_type;
    bool blend_mode_changed;
};

void context_create(Heap* heap)
{
    context = HEAP_ALLOCATE(heap, Context, 1);
    Context* c = context;

    glGenVertexArrays(context_vertex_type_count, c->vertex_arrays);
    glGenBuffers(context_vertex_type_count, c->buffers);

    glBindVertexArray(c->vertex_arrays[0]);
    glBindBuffer(GL_ARRAY_BUFFER, c->buffers[0]);

    glBufferData(GL_ARRAY_BUFFER, sizeof(c->vertices), nullptr, GL_DYNAMIC_DRAW);

    GLvoid* offset0 = reinterpret_cast<GLvoid*>(offsetof(VertexPC, position));
    GLvoid* offset1 = reinterpret_cast<GLvoid*>(offsetof(VertexPC, colour));
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(VertexPC), offset0);
    glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(VertexPC), offset1);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(2);

    glBindVertexArray(c->vertex_arrays[1]);
    glBindBuffer(GL_ARRAY_BUFFER, c->buffers[1]);

    glBufferData(GL_ARRAY_BUFFER, sizeof(c->line_vertices), nullptr, GL_DYNAMIC_DRAW);

    offset0 = reinterpret_cast<GLvoid*>(offsetof(LineVertex, position));
    offset1 = reinterpret_cast<GLvoid*>(offsetof(LineVertex, direction));
    GLvoid* offset2 = reinterpret_cast<GLvoid*>(offsetof(LineVertex, colour));
    GLvoid* offset3 = reinterpret_cast<GLvoid*>(offsetof(LineVertex, texcoord));
    GLvoid* offset4 = reinterpret_cast<GLvoid*>(offsetof(LineVertex, side));
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

    glBufferData(GL_ARRAY_BUFFER, sizeof(c->vertices_textured), nullptr, GL_DYNAMIC_DRAW);

    offset0 = reinterpret_cast<GLvoid*>(offsetof(VertexPT, position));
    offset1 = reinterpret_cast<GLvoid*>(offsetof(VertexPT, texcoord));
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(VertexPT), offset0);
    glVertexAttribPointer(3, 2, GL_UNSIGNED_SHORT, GL_TRUE, sizeof(VertexPT), offset1);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(3);

    glBindVertexArray(0);
}

void context_destroy(Heap* heap)
{
    if(context)
    {
        glDeleteBuffers(context_vertex_type_count, context->buffers);
        glDeleteVertexArrays(context_vertex_type_count, context->vertex_arrays);
        HEAP_DEALLOCATE(heap, context);
    }
}

void set_matrices(Matrix4 view, Matrix4 projection)
{
    context->view_projection = projection * view;
    context->projection = projection;
}

void set_shader(GLuint program)
{
    context->shaders[0] = program;
}

void set_line_shader(GLuint program)
{
    context->shaders[1] = program;
}

void set_textured_shader(GLuint program)
{
    context->shaders[2] = program;
}

void set_blend_mode(BlendMode mode)
{
    if(context->blend_mode != mode)
    {
        context->blend_mode = mode;
        context->blend_mode_changed = true;
    }
}

void set_text_colour(Vector3 text_colour)
{
    context->text_colour = text_colour;
}

void set_clip_area(Rect rect, int viewport_width, int viewport_height)
{
    glEnable(GL_SCISSOR_TEST);
    int x = rect.bottom_left.x + (viewport_width / 2);
    int y = rect.bottom_left.y + (viewport_height / 2);
    glScissor(x, y, rect.dimensions.x, rect.dimensions.y);
}

void stop_clip_area()
{
    glDisable(GL_SCISSOR_TEST);
}

void draw()
{
    Context* c = context;
    if(c->filled == 0 || c->vertex_type == VertexType::None)
    {
        return;
    }

    if(c->blend_mode_changed)
    {
        switch(c->blend_mode)
        {
            case BlendMode::None:
            case BlendMode::Opaque:
            {
                glDisable(GL_BLEND);
                glDepthMask(GL_TRUE);
                glEnable(GL_CULL_FACE);
                break;
            }
            case BlendMode::Transparent:
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
        case VertexType::None:
        case VertexType::Colour:
        {
            glBindBuffer(GL_ARRAY_BUFFER, c->buffers[0]);
            glBufferData(GL_ARRAY_BUFFER, sizeof(VertexPC) * c->filled, c->vertices, GL_DYNAMIC_DRAW);
            glBindVertexArray(c->vertex_arrays[0]);
            shader = c->shaders[0];
            break;
        }
        case VertexType::Line:
        {
            glBindBuffer(GL_ARRAY_BUFFER, c->buffers[1]);
            glBufferData(GL_ARRAY_BUFFER, sizeof(LineVertex) * c->filled, c->line_vertices, GL_DYNAMIC_DRAW);
            glBindVertexArray(c->vertex_arrays[1]);
            shader = c->shaders[1];
            break;
        }
        case VertexType::Texture:
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
    glUniformMatrix4fv(location, 1, GL_TRUE, c->view_projection.elements);

    location = glGetUniformLocation(shader, "text_colour");
    if(location != -1)
    {
        glUniform3fv(location, 1, &c->text_colour[0]);
    }

    location = glGetUniformLocation(shader, "projection_factor");
    if(location != -1)
    {
        glUniform1f(location, c->projection[0]);
    }

    glDrawArrays(GL_TRIANGLES, 0, c->filled);

    set_blend_mode(BlendMode::None);
    set_text_colour(vector3_white);
    c->vertex_type = VertexType::None;
    c->filled = 0;
}

void add_line(Vector3 start, Vector3 end, Vector4 colour)
{
    Context* c = context;
    ASSERT(c->vertex_type == VertexType::Line || c->vertex_type == VertexType::None);
    ASSERT(c->filled + 6 < context_vertices_cap);
    u32 colour_u32 = rgba_to_u32(colour);
    u32 texcoords[4] =
    {
        texcoord_to_u32({0.0f, 0.0f}),
        texcoord_to_u32({0.0f, 1.0f}),
        texcoord_to_u32({1.0f, 1.0f}),
        texcoord_to_u32({1.0f, 0.0f}),
    };
    Vector3 direction = end - start;
    c->line_vertices[c->filled + 0] = {start, direction, colour_u32, texcoords[1], -1.0f};
    c->line_vertices[c->filled + 1] = {start, direction, colour_u32, texcoords[2], 1.0f};
    c->line_vertices[c->filled + 2] = {end, -direction, colour_u32, texcoords[0], 1.0f};
    c->line_vertices[c->filled + 3] = {end, -direction, colour_u32, texcoords[0], 1.0f};
    c->line_vertices[c->filled + 4] = {start, direction, colour_u32, texcoords[2], 1.0f};
    c->line_vertices[c->filled + 5] = {end, -direction, colour_u32, texcoords[3], -1.0f};
    c->filled += 6;
    c->vertex_type = VertexType::Line;
}

void add_triangle(Triangle* triangle, Vector4 colour)
{
    Context* c = context;
    ASSERT(c->vertex_type == VertexType::Colour || c->vertex_type == VertexType::None);
    ASSERT(c->filled + 3 < context_vertices_cap);
    for(int i = 0; i < 3; ++i)
    {
        c->vertices[c->filled + i].position = triangle->vertices[i];
        c->vertices[c->filled + i].colour = rgba_to_u32(colour);
    }
    c->filled += 3;
    c->vertex_type = VertexType::Colour;
}

void add_rect(Rect rect, Vector4 colour)
{
    Quad quad = rect_to_quad(rect);
    add_quad(&quad, colour);
}

void add_wire_rect(Rect rect, Vector4 colour)
{
    Quad quad = rect_to_quad(rect);
    add_wire_quad(&quad, colour);
}

void add_quad(Quad* quad, Vector4 colour)
{
    Context* c = context;
    ASSERT(c->vertex_type == VertexType::Colour || c->vertex_type == VertexType::None);
    ASSERT(c->filled + 6 < context_vertices_cap);
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
    c->vertex_type = VertexType::Colour;
}

void add_quad_textured(Quad* quad, Rect texture_rect)
{
    Context* c = context;
    ASSERT(c->vertex_type == VertexType::Texture || c->vertex_type == VertexType::None);
    ASSERT(c->filled + 6 < context_vertices_cap);
    c->vertices_textured[c->filled + 0].position = quad->vertices[0];
    c->vertices_textured[c->filled + 1].position = quad->vertices[1];
    c->vertices_textured[c->filled + 2].position = quad->vertices[2];
    c->vertices_textured[c->filled + 3].position = quad->vertices[0];
    c->vertices_textured[c->filled + 4].position = quad->vertices[2];
    c->vertices_textured[c->filled + 5].position = quad->vertices[3];
    const Vector2 texcoords[4] =
    {
        {texture_rect.bottom_left.x, texture_rect.bottom_left.y + texture_rect.dimensions.y},
        texture_rect.bottom_left + texture_rect.dimensions,
        {texture_rect.bottom_left.x + texture_rect.dimensions.x, texture_rect.bottom_left.y},
        texture_rect.bottom_left,
    };
    c->vertices_textured[c->filled + 0].texcoord = texcoord_to_u32(texcoords[0]);
    c->vertices_textured[c->filled + 1].texcoord = texcoord_to_u32(texcoords[1]);
    c->vertices_textured[c->filled + 2].texcoord = texcoord_to_u32(texcoords[2]);
    c->vertices_textured[c->filled + 3].texcoord = texcoord_to_u32(texcoords[0]);
    c->vertices_textured[c->filled + 4].texcoord = texcoord_to_u32(texcoords[2]);
    c->vertices_textured[c->filled + 5].texcoord = texcoord_to_u32(texcoords[3]);
    c->filled += 6;
    c->vertex_type = VertexType::Texture;
}

void add_wire_quad(Quad* quad, Vector4 colour)
{
    add_line(quad->vertices[0], quad->vertices[1], colour);
    add_line(quad->vertices[1], quad->vertices[2], colour);
    add_line(quad->vertices[2], quad->vertices[3], colour);
    add_line(quad->vertices[3], quad->vertices[0], colour);
}

void add_circle(Vector3 center, Vector3 axis, float radius, Vector4 colour)
{
    const int segments = 16;
    Quaternion orientation = axis_angle_rotation(axis, 0.0f);
    Vector3 arm = radius * normalise(perp(axis));
    Vector3 position = (orientation * arm) + center;

    for(int i = 1; i <= segments; i += 1)
    {
        Vector3 prior = position;
        float t = (static_cast<float>(i) / segments) * tau;
        orientation = axis_angle_rotation(axis, t);
        position = (orientation * arm) + center;

        Triangle triangle;
        triangle.vertices[0] = position;
        triangle.vertices[1] = prior;
        triangle.vertices[2] = center;
        add_triangle(&triangle, colour);
    }
}

void add_cone(Vector3 base_center, Vector3 axis, float radius, Vector4 side_colour, Vector4 base_colour)
{
    const int segments = 16;
    Quaternion orientation = axis_angle_rotation(axis, 0.0f);
    Vector3 arm = radius * normalise(perp(axis));
    Vector3 position = (orientation * arm) + base_center;
    Vector3 apex = axis + base_center;

    for(int i = 1; i <= segments; i += 1)
    {
        Vector3 prior = position;
        float t = (static_cast<float>(i) / segments) * tau;
        orientation = axis_angle_rotation(axis, t);
        position = (orientation * arm) + base_center;

        Triangle triangle;
        triangle.vertices[0] = prior;
        triangle.vertices[1] = position;
        triangle.vertices[2] = apex;
        add_triangle(&triangle, side_colour);
    }

    add_circle(base_center, axis, radius, base_colour);
}

void add_cylinder(Vector3 start, Vector3 end, float radius, Vector4 colour)
{
    const int segments = 16;
    Vector3 axis = end - start;
    Quaternion orientation = axis_angle_rotation(axis, 0.0f);
    Vector3 arm = radius * normalise(perp(axis));
    Vector3 next_top = (orientation * arm) + start;
    Vector3 next_bottom = (orientation * arm) + end;

    for(int i = 1; i <= segments; i += 1)
    {
        Vector3 prior_top = next_top;
        Vector3 prior_bottom = next_bottom;
        float t = (static_cast<float>(i) / segments) * tau;
        orientation = axis_angle_rotation(axis, t);
        next_top = (orientation * arm) + start;
        next_bottom = (orientation * arm) + end;

        Quad quad;
        quad.vertices[0] = prior_top;
        quad.vertices[1] = next_top;
        quad.vertices[2] = next_bottom;
        quad.vertices[3] = prior_bottom;
        add_quad(&quad, colour);
    }

    add_circle(start, axis, radius, colour);
    add_circle(end, -axis, radius, colour);
}

void add_box(Vector3 center, Vector3 extents, Vector4 colour)
{
    // corners
    Vector3 c[8] =
    {
        {+extents.x, +extents.y, +extents.z},
        {+extents.x, +extents.y, -extents.z},
        {+extents.x, -extents.y, +extents.z},
        {+extents.x, -extents.y, -extents.z},
        {-extents.x, +extents.y, +extents.z},
        {-extents.x, +extents.y, -extents.z},
        {-extents.x, -extents.y, +extents.z},
        {-extents.x, -extents.y, -extents.z},
    };
    for(int i = 0; i < 8; i += 1)
    {
        c[i] += center;
    }

    Quad sides[6] =
    {
        {c[1], c[0], c[2], c[3]},
        {c[4], c[5], c[7], c[6]},
        {c[0], c[1], c[5], c[4]},
        {c[3], c[2], c[6], c[7]},
        {c[2], c[0], c[4], c[6]},
        {c[1], c[3], c[7], c[5]},
    };

    add_quad(&sides[0], colour);
    add_quad(&sides[1], colour);
    add_quad(&sides[2], colour);
    add_quad(&sides[3], colour);
    add_quad(&sides[4], colour);
    add_quad(&sides[5], colour);
}

static Vector3 polar_to_cartesian(float theta, float phi, float radius)
{
    Vector3 point;
    point.x = radius * sin(theta) * cos(phi);
    point.y = radius * sin(theta) * sin(phi);
    point.z = radius * cos(theta);
    return point;
}

void add_sphere(Vector3 center, float radius, Vector4 colour)
{
    const int meridians = 9;
    const int parallels = 7;
    const int rings = parallels + 1;

    Vector3 top = {0.0f, 0.0f, radius};
    for(int i = 0; i < meridians; i += 1)
    {
        float theta = 1.0f / static_cast<float>(rings) * pi;
        float phi0 = (i    ) / static_cast<float>(meridians) * tau;
        float phi1 = (i + 1) / static_cast<float>(meridians) * tau;

        Triangle triangle;
        triangle.vertices[0] = polar_to_cartesian(theta, phi0, radius) + center;
        triangle.vertices[1] = polar_to_cartesian(theta, phi1, radius) + center;
        triangle.vertices[2] = top + center;
        add_triangle(&triangle, colour);
    }

    Vector3 bottom = {0.0f, 0.0f, -radius};
    for(int i = 0; i < meridians; i += 1)
    {
        float theta = (parallels) / static_cast<float>(rings) * pi;
        float phi0 = (i + 1) / static_cast<float>(meridians) * tau;
        float phi1 = (i    ) / static_cast<float>(meridians) * tau;

        Triangle triangle;
        triangle.vertices[0] = polar_to_cartesian(theta, phi0, radius) + center;
        triangle.vertices[1] = polar_to_cartesian(theta, phi1, radius) + center;
        triangle.vertices[2] = bottom + center;
        add_triangle(&triangle, colour);
    }

    for(int i = 1; i < parallels; i += 1)
    {
        float theta0 = (i    ) / static_cast<float>(rings) * pi;
        float theta1 = (i + 1) / static_cast<float>(rings) * pi;
        for(int j = 0; j < meridians; j += 1)
        {
            float phi0 = (j    ) / static_cast<float>(meridians) * tau;
            float phi1 = (j + 1) / static_cast<float>(meridians) * tau;

            Quad quad;
            quad.vertices[0] = polar_to_cartesian(theta0, phi0, radius) + center;
            quad.vertices[1] = polar_to_cartesian(theta1, phi0, radius) + center;
            quad.vertices[2] = polar_to_cartesian(theta1, phi1, radius) + center;
            quad.vertices[3] = polar_to_cartesian(theta0, phi1, radius) + center;
            add_quad(&quad, colour);
        }
    }
}

void add_arc(Vector3 center, Vector3 axis, float angle, float start_angle, float radius, float width, Vector4 colour)
{
    const int segments = 11;

    Vector3 arm = radius * normalise(perp(axis));
    float half_width = width / 2.0f;
    Quaternion orientation = axis_angle_rotation(axis, start_angle);
    Vector3 medial = orientation * arm;
    Vector3 direction = normalise(medial);

    Quad quad;
    quad.vertices[0] = (half_width * -direction) + medial + center;
    quad.vertices[1] = (half_width * direction) + medial + center;

    for(int i = 1; i <= segments; i += 1)
    {
        float theta = (i / static_cast<float>(segments) * angle) + start_angle;
        orientation = axis_angle_rotation(axis, theta);
        medial = orientation * arm;
        direction = normalise(medial);

        quad.vertices[2] = (half_width * direction) + medial + center;
        quad.vertices[3] = (half_width * -direction) + medial + center;
        add_quad(&quad, colour);

        quad.vertices[0] = quad.vertices[3];
        quad.vertices[1] = quad.vertices[2];
    }
}

void draw_opaque_rect(Rect rect, Vector4 colour)
{
    add_rect(rect, colour);
    draw();
}

void draw_transparent_rect(Rect rect, Vector4 colour)
{
    set_blend_mode(immediate::BlendMode::Transparent);
    add_rect(rect, colour);
    draw();
}

} // namespace immediate
