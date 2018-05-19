#include "video_object.h"

#include "array2.h"
#include "float_utilities.h"
#include "jan.h"
#include "math_basics.h"
#include "vertex_layout.h"

namespace video {

void object_create(Object* object, VertexLayout vertex_layout)
{
    object->model = matrix4_identity;
    object->vertex_layout = vertex_layout;

    glGenVertexArrays(1, &object->vertex_array);
    glGenBuffers(2, object->buffers);

    // Set up the vertex array.
    glBindVertexArray(object->vertex_array);

    switch(object->vertex_layout)
    {
        case VertexLayout::PNC:
        {
            const int vertex_size = sizeof(VertexPNC);
            GLvoid* offset1 = reinterpret_cast<GLvoid*>(offsetof(VertexPNC, normal));
            GLvoid* offset2 = reinterpret_cast<GLvoid*>(offsetof(VertexPNC, colour));
            glBindBuffer(GL_ARRAY_BUFFER, object->buffers[0]);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, vertex_size, nullptr);
            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, vertex_size, offset1);
            glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, vertex_size, offset2);
            glEnableVertexAttribArray(0);
            glEnableVertexAttribArray(1);
            glEnableVertexAttribArray(2);
            break;
        }
        case VertexLayout::Point:
        {
            const int vertex_size = sizeof(PointVertex);
            GLvoid* offset0 = reinterpret_cast<GLvoid*>(offsetof(PointVertex, position));
            GLvoid* offset1 = reinterpret_cast<GLvoid*>(offsetof(PointVertex, direction));
            GLvoid* offset2 = reinterpret_cast<GLvoid*>(offsetof(PointVertex, colour));
            GLvoid* offset3 = reinterpret_cast<GLvoid*>(offsetof(PointVertex, texcoord));
            glBindBuffer(GL_ARRAY_BUFFER, object->buffers[0]);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, vertex_size, offset0);
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, vertex_size, offset1);
            glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, vertex_size, offset2);
            glVertexAttribPointer(3, 2, GL_UNSIGNED_SHORT, GL_TRUE, vertex_size, offset3);
            glEnableVertexAttribArray(0);
            glEnableVertexAttribArray(1);
            glEnableVertexAttribArray(2);
            glEnableVertexAttribArray(3);
            break;
        }
        case VertexLayout::Line:
        {
            const int vertex_size = sizeof(LineVertex);
            GLvoid* offset0 = reinterpret_cast<GLvoid*>(offsetof(LineVertex, position));
            GLvoid* offset1 = reinterpret_cast<GLvoid*>(offsetof(LineVertex, direction));
            GLvoid* offset2 = reinterpret_cast<GLvoid*>(offsetof(LineVertex, colour));
            GLvoid* offset3 = reinterpret_cast<GLvoid*>(offsetof(LineVertex, texcoord));
            GLvoid* offset4 = reinterpret_cast<GLvoid*>(offsetof(LineVertex, side));
            glBindBuffer(GL_ARRAY_BUFFER, object->buffers[0]);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, vertex_size, offset0);
            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, vertex_size, offset1);
            glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, vertex_size, offset2);
            glVertexAttribPointer(3, 2, GL_UNSIGNED_SHORT, GL_TRUE, vertex_size, offset3);
            glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, vertex_size, offset4);
            glEnableVertexAttribArray(0);
            glEnableVertexAttribArray(1);
            glEnableVertexAttribArray(2);
            glEnableVertexAttribArray(3);
            glEnableVertexAttribArray(4);
            break;
        }
    }

    glBindVertexArray(0);
}

void object_destroy(Object* object)
{
    glDeleteVertexArrays(1, &object->vertex_array);
    glDeleteBuffers(2, object->buffers);
}

static void object_set_surface(Object* object, VertexPNC* vertices, int vertices_count, u16* indices, int indices_count)
{
    glBindVertexArray(object->vertex_array);

    const int vertex_size = sizeof(VertexPNC);
    GLsizei vertices_size = vertex_size * vertices_count;
    GLvoid* offset1 = reinterpret_cast<GLvoid*>(offsetof(VertexPNC, normal));
    GLvoid* offset2 = reinterpret_cast<GLvoid*>(offsetof(VertexPNC, colour));
    glBindBuffer(GL_ARRAY_BUFFER, object->buffers[0]);
    glBufferData(GL_ARRAY_BUFFER, vertices_size, vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, vertex_size, nullptr);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, vertex_size, offset1);
    glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, vertex_size, offset2);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);

    GLsizei indices_size = sizeof(u16) * indices_count;
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, object->buffers[1]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices_size, indices, GL_STATIC_DRAW);
    object->indices_count = indices_count;

    glBindVertexArray(0);
}

static void object_finish_update(Object* object, Heap* heap, VertexPNC* vertices, u16* indices)
{
    glBindVertexArray(object->vertex_array);

    const int vertex_size = sizeof(VertexPNC);
    GLsizei vertices_size = vertex_size * array_count(vertices);
    glBindBuffer(GL_ARRAY_BUFFER, object->buffers[0]);
    glBufferData(GL_ARRAY_BUFFER, vertices_size, vertices, GL_DYNAMIC_DRAW);

    GLsizei indices_size = sizeof(u16) * array_count(indices);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, object->buffers[1]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices_size, indices, GL_DYNAMIC_DRAW);
    object->indices_count = array_count(indices);

    glBindVertexArray(0);

    ARRAY_DESTROY(vertices, heap);
    ARRAY_DESTROY(indices, heap);
}

static void object_update_points(Object* object, Heap* heap, PointVertex* vertices, u16* indices)
{
    glBindVertexArray(object->vertex_array);

    const int vertex_size = sizeof(PointVertex);
    GLsizei vertices_size = vertex_size * array_count(vertices);
    glBindBuffer(GL_ARRAY_BUFFER, object->buffers[0]);
    glBufferData(GL_ARRAY_BUFFER, vertices_size, vertices, GL_DYNAMIC_DRAW);

    GLsizei indices_size = sizeof(u16) * array_count(indices);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, object->buffers[1]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices_size, indices, GL_DYNAMIC_DRAW);
    object->indices_count = array_count(indices);

    glBindVertexArray(0);

    ARRAY_DESTROY(vertices, heap);
    ARRAY_DESTROY(indices, heap);
}

static void object_update_lines(Object* object, Heap* heap, LineVertex* vertices, u16* indices)
{
    glBindVertexArray(object->vertex_array);

    const int vertex_size = sizeof(LineVertex);
    GLsizei vertices_size = vertex_size * array_count(vertices);
    glBindBuffer(GL_ARRAY_BUFFER, object->buffers[0]);
    glBufferData(GL_ARRAY_BUFFER, vertices_size, vertices, GL_DYNAMIC_DRAW);

    GLsizei indices_size = sizeof(u16) * array_count(indices);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, object->buffers[1]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices_size, indices, GL_DYNAMIC_DRAW);
    object->indices_count = array_count(indices);

    glBindVertexArray(0);

    ARRAY_DESTROY(vertices, heap);
    ARRAY_DESTROY(indices, heap);
}

void object_update_mesh(Object* object, jan::Mesh* mesh, Heap* heap)
{
    VertexPNC* vertices;
    u16* indices;
    jan::triangulate(mesh, heap, &vertices, &indices);

    object_finish_update(object, heap, vertices, indices);
}

void object_update_selection(Object* object, jan::Mesh* mesh, jan::Selection* selection, Heap* heap)
{
    VertexPNC* vertices;
    u16* indices;
    jan::triangulate_selection(mesh, selection, heap, &vertices, &indices);

    object_finish_update(object, heap, vertices, indices);
}

void object_update_wireframe(Object* object, jan::Mesh* mesh, Heap* heap)
{
    const Vector4 colour = {1.0f, 0.5f, 0.0f, 0.8f};

    LineVertex* vertices;
    u16* indices;
    jan::make_wireframe(mesh, heap, colour, &vertices, &indices);

    object_update_lines(object, heap, vertices, indices);
}

void object_update_wireframe_selection(Object* object, jan::Mesh* mesh, jan::Selection* selection, jan::Edge* hovered, Heap* heap)
{
    const Vector4 colour = {1.0f, 1.0f, 1.0f, 1.0f};
    const Vector4 hover_colour = {0.0f, 1.0f, 1.0f, 1.0f};
    const Vector4 select_colour = {1.0f, 0.5f, 0.0f, 0.8f};

    LineVertex* vertices;
    u16* indices;
    jan::make_wireframe_selection(mesh, heap, colour, hovered, hover_colour, selection, select_colour, &vertices, &indices);

    object_update_lines(object, heap, vertices, indices);
}

void object_update_pointcloud(Object* object, jan::Mesh* mesh, Heap* heap)
{
    const Vector4 colour = {1.0f, 0.5f, 0.0f, 1.0f};

    PointVertex* vertices;
    u16* indices;
    jan::make_pointcloud(mesh, heap, colour, &vertices, &indices);

    object_update_points(object, heap, vertices, indices);
}

void object_update_pointcloud_selection(Object* object, jan::Mesh* mesh, jan::Selection* selection, jan::Vertex* hovered, Heap* heap)
{
    const Vector4 colour = {1.0f, 1.0f, 1.0f, 1.0f};
    const Vector4 hover_colour = {0.0f, 1.0f, 1.0f, 1.0f};
    const Vector4 select_colour = {1.0f, 0.5f, 0.0f, 1.0f};

    PointVertex* vertices;
    u16* indices;
    jan::make_pointcloud_selection(mesh, colour, hovered, hover_colour, selection, select_colour, heap, &vertices, &indices);

    object_update_points(object, heap, vertices, indices);
}

void object_set_matrices(Object* object, Matrix4 view, Matrix4 projection)
{
    Matrix4 model_view = view * object->model;
    object->model_view_projection = projection * model_view;
    object->normal = transpose(inverse_transform(model_view));
}

void object_generate_sky(Object* object, Stack* stack)
{
    const float radius = 1.0f;
    const int meridians = 9;
    const int parallels = 7;
    int rings = parallels + 1;

    int vertices_count = meridians * parallels + 2;
    VertexPNC* vertices = STACK_ALLOCATE(stack, VertexPNC, vertices_count);
    if(!vertices)
    {
        return;
    }

    const Vector3 top_colour = {1.0f, 1.0f, 0.2f};
    const Vector3 bottom_colour = {0.1f, 0.7f, 0.6f};
    vertices[0].position = radius * vector3_unit_z;
    vertices[0].normal = -vector3_unit_z;
    vertices[0].colour = rgb_to_u32(top_colour);
    for(int i = 0; i < parallels; ++i)
    {
        float step = (i + 1) / static_cast<float>(rings);
        float theta = step * pi;
        Vector3 ring_colour = lerp(top_colour, bottom_colour, step);
        for(int j = 0; j < meridians; ++j)
        {
            float phi = (j + 1) / static_cast<float>(meridians) * tau;
            float x = radius * sin(theta) * cos(phi);
            float y = radius * sin(theta) * sin(phi);
            float z = radius * cos(theta);
            Vector3 position = {x, y, z};
            VertexPNC* vertex = &vertices[meridians * i + j + 1];
            vertex->position = position;
            vertex->normal = -normalise(position);
            vertex->colour = rgb_to_u32(ring_colour);
        }
    }
    vertices[vertices_count - 1].position = radius * -vector3_unit_z;
    vertices[vertices_count - 1].normal = vector3_unit_z;
    vertices[vertices_count - 1].colour = rgb_to_u32(bottom_colour);

    int indices_count = 6 * meridians * rings;
    u16* indices = STACK_ALLOCATE(stack, u16, indices_count);
    if(!indices)
    {
        STACK_DEALLOCATE(stack, vertices);
        return;
    }

    int out_base = 0;
    int in_base = 1;
    for(int i = 0; i < meridians; ++i)
    {
        int o = out_base + 3 * i;
        indices[o + 0] = 0;
        indices[o + 1] = in_base + (i + 1) % meridians;
        indices[o + 2] = in_base + i;
    }
    out_base += 3 * meridians;
    for(int i = 0; i < rings - 2; ++i)
    {
        for(int j = 0; j < meridians; ++j)
        {
            int x = meridians * i + j;
            int o = out_base + 6 * x;
            int k0 = in_base + x;
            int k1 = in_base + meridians * i;

            indices[o + 0] = k0;
            indices[o + 1] = k1 + (j + 1) % meridians;
            indices[o + 2] = k0 + meridians;

            indices[o + 3] = k0 + meridians;
            indices[o + 4] = k1 + (j + 1) % meridians;
            indices[o + 5] = k1 + meridians + (j + 1) % meridians;
        }
    }
    out_base += 6 * meridians * (rings - 2);
    in_base += meridians * (parallels - 2);
    for(int i = 0; i < meridians; ++i)
    {
        int o = out_base + 3 * i;
        indices[o + 0] = vertices_count - 1;
        indices[o + 1] = in_base + i;
        indices[o + 2] = in_base + (i + 1) % meridians;
    }

    object_set_surface(object, vertices, vertices_count, indices, indices_count);

    STACK_DEALLOCATE(stack, vertices);
    STACK_DEALLOCATE(stack, indices);
}

void object_set_model(Object* object, Matrix4 model)
{
    object->model = model;
}

} // namespace video
