#include "video_object.h"

#include "array2.h"
#include "float_utilities.h"
#include "jan.h"
#include "math_basics.h"
#include "vertex_layout.h"

void video_object_create(VideoObject* object, VertexLayout vertex_layout)
{
    object->model = matrix4_identity;
    object->vertex_layout = vertex_layout;
}

void video_object_destroy(VideoObject* object, Backend* backend)
{
    destroy_buffer(backend, object->buffers[0]);
    destroy_buffer(backend, object->buffers[1]);
}

static void ensure_buffer_room(VideoObject* object, int vertices_needed, int indices_needed, Backend* backend, Log* logger)
{
    if(vertices_needed > object->vertices_count)
    {
        destroy_buffer(backend, object->buffers[0]);
        object->buffers[0] = (BufferId){0};
    }

    if(!object->buffers[0].value && vertices_needed)
    {
        BufferSpec vertex_buffer_spec =
        {
            .format = BUFFER_FORMAT_VERTEX,
            .usage = BUFFER_USAGE_DYNAMIC,
            .size = get_vertex_layout_size(object->vertex_layout) * vertices_needed,
        };
        object->buffers[0] = create_buffer(backend, &vertex_buffer_spec, logger);
    }

    if(indices_needed > object->indices_count)
    {
        destroy_buffer(backend, object->buffers[1]);
        object->buffers[1] = (BufferId){0};
    }

    if(!object->buffers[1].value && indices_needed)
    {
        BufferSpec index_buffer_spec =
        {
            .format = BUFFER_FORMAT_INDEX,
            .usage = BUFFER_USAGE_DYNAMIC,
            .size = sizeof(uint16_t) * indices_needed,
        };
        object->buffers[1] = create_buffer(backend, &index_buffer_spec, logger);
    }
}

static void object_finish_update(VideoObject* object, Backend* backend, Log* logger, Heap* heap, VertexPNC* vertices, uint16_t* indices)
{
    ensure_buffer_room(object, array_count(vertices), array_count(indices), backend, logger);

    int vertices_size = sizeof(VertexPNC) * array_count(vertices);
    update_buffer(backend, object->buffers[0], vertices, 0, vertices_size);

    int indices_size = sizeof(uint16_t) * array_count(indices);
    update_buffer(backend, object->buffers[1], indices, 0, indices_size);

    object->vertices_count = array_count(vertices);
    object->indices_count = array_count(indices);

    ARRAY_DESTROY(vertices, heap);
    ARRAY_DESTROY(indices, heap);
}

static void object_update_points(VideoObject* object, Backend* backend, Log* logger, Heap* heap, PointVertex* vertices, uint16_t* indices)
{
    ensure_buffer_room(object, array_count(vertices), array_count(indices), backend, logger);

    int vertices_size = sizeof(PointVertex) * array_count(vertices);
    update_buffer(backend, object->buffers[0], vertices, 0, vertices_size);

    int indices_size = sizeof(uint16_t) * array_count(indices);
    update_buffer(backend, object->buffers[1], indices, 0, indices_size);

    object->vertices_count = array_count(vertices);
    object->indices_count = array_count(indices);

    ARRAY_DESTROY(vertices, heap);
    ARRAY_DESTROY(indices, heap);
}

static void object_update_lines(VideoObject* object, Backend* backend, Log* logger, Heap* heap, LineVertex* vertices, uint16_t* indices)
{
    ensure_buffer_room(object, array_count(vertices), array_count(indices), backend, logger);

    int vertices_size = sizeof(LineVertex) * array_count(vertices);
    update_buffer(backend, object->buffers[0], vertices, 0, vertices_size);

    int indices_size = sizeof(uint16_t) * array_count(indices);
    update_buffer(backend, object->buffers[1], indices, 0, indices_size);

    object->vertices_count = array_count(vertices);
    object->indices_count = array_count(indices);

    ARRAY_DESTROY(vertices, heap);
    ARRAY_DESTROY(indices, heap);
}

void video_object_update_mesh(VideoObject* object, JanMesh* mesh, Backend* backend, Log* logger, Heap* heap)
{
    Triangulation triangulation = jan_triangulate(mesh, heap);

    object_finish_update(object, backend, logger, heap, triangulation.vertices,
            triangulation.indices);
}

void video_object_update_selection(VideoObject* object, JanMesh* mesh, JanSelection* selection, Backend* backend, Log* logger, Heap* heap)
{
    Triangulation triangulation = jan_triangulate_selection(mesh, selection,
            heap);

    object_finish_update(object, backend, logger, heap, triangulation.vertices,
            triangulation.indices);
}

void video_object_update_wireframe(VideoObject* object, JanMesh* mesh, Backend* backend, Log* logger, Heap* heap)
{
    WireframeSpec spec =
    {
        .colour = (Float4){{1.0f, 0.5f, 0.0f, 1.0f}},
    };
    Wireframe wireframe = jan_make_wireframe(mesh, heap, &spec);

    object_update_lines(object, backend, logger, heap, wireframe.vertices,
            wireframe.indices);
}

void video_object_update_wireframe_selection(VideoObject* object, JanMesh* mesh, JanSelection* selection, JanEdge* hovered, Backend* backend, Log* logger, Heap* heap)
{
    WireframeSpec spec =
    {
        .colour = (Float4){{1.0f, 1.0f, 1.0f, 1.0f}},
        .hover_colour = (Float4){{0.0f, 1.0f, 1.0f, 1.0f}},
        .select_colour = (Float4){{1.0f, 0.5f, 0.0f, 0.8f}},
        .selection = selection,
        .hovered = hovered,
    };
    Wireframe wireframe = jan_make_wireframe(mesh, heap, &spec);

    object_update_lines(object, backend, logger, heap, wireframe.vertices,
            wireframe.indices);
}

void video_object_update_pointcloud(VideoObject* object, JanMesh* mesh, Backend* backend, Log* logger, Heap* heap)
{
    PointcloudSpec spec =
    {
        .colour = (Float4){{1.0f, 0.5f, 0.0f, 1.0f}},
    };
    Pointcloud pointcloud = jan_make_pointcloud(mesh, heap, &spec);

    object_update_points(object, backend, logger, heap, pointcloud.vertices,
            pointcloud.indices);
}

void video_object_update_pointcloud_selection(VideoObject* object, JanMesh* mesh, JanSelection* selection, JanVertex* hovered, Backend* backend, Log* logger, Heap* heap)
{
    PointcloudSpec spec =
    {
        .colour = (Float4){{1.0f, 1.0f, 1.0f, 1.0f}},
        .hover_colour = (Float4){{0.0f, 1.0f, 1.0f, 1.0f}},
        .select_colour = (Float4){{1.0f, 0.5f, 0.0f, 1.0f}},
        .hovered = hovered,
        .selection = selection,
    };
    Pointcloud pointcloud = jan_make_pointcloud(mesh, heap, &spec);

    object_update_points(object, backend, logger, heap, pointcloud.vertices,
            pointcloud.indices);
}

void video_object_set_matrices(VideoObject* object, Matrix4 view, Matrix4 projection)
{
    Matrix4 model_view = matrix4_multiply(view, object->model);
    object->normal = matrix4_transpose(matrix4_inverse_transform(model_view));
}

void video_object_generate_sky(VideoObject* object, Backend* backend, Log* logger, Stack* stack)
{
    const float radius = 1.0f;
    const int meridians = 9;
    const int parallels = 7;
    int rings = parallels + 1;

    int vertices_count = meridians * parallels + 2;
    VertexPC* vertices = STACK_ALLOCATE(stack, VertexPC, vertices_count);
    if(!vertices)
    {
        return;
    }

    const Float3 top_colour = (Float3){{1.0f, 1.0f, 0.2f}};
    const Float3 bottom_colour = (Float3){{0.1f, 0.7f, 0.6f}};
    vertices[0].position = float3_multiply(radius, float3_unit_z);
    vertices[0].colour = rgb_to_u32(top_colour);
    for(int i = 0; i < parallels; ++i)
    {
        float step = (i + 1) / ((float) rings);
        float theta = step * pi;
        Float3 ring_colour = float3_lerp(top_colour, bottom_colour, step);
        for(int j = 0; j < meridians; ++j)
        {
            float phi = (j + 1) / ((float) meridians) * tau;
            float x = radius * sinf(theta) * cosf(phi);
            float y = radius * sinf(theta) * sinf(phi);
            float z = radius * cosf(theta);
            Float3 position = (Float3){{x, y, z}};
            VertexPC* vertex = &vertices[meridians * i + j + 1];
            vertex->position = position;
            vertex->colour = rgb_to_u32(ring_colour);
        }
    }
    vertices[vertices_count - 1].position = float3_multiply(-radius, float3_unit_z);
    vertices[vertices_count - 1].colour = rgb_to_u32(bottom_colour);

    int indices_count = 6 * meridians * rings;
    uint16_t* indices = STACK_ALLOCATE(stack, uint16_t, indices_count);
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

    ensure_buffer_room(object, vertices_count, indices_count, backend, logger);

    int vertices_size = sizeof(VertexPC) * vertices_count;
    update_buffer(backend, object->buffers[0], vertices, 0, vertices_size);

    int indices_size = sizeof(uint16_t) * indices_count;
    update_buffer(backend, object->buffers[1], indices, 0, indices_size);

    object->vertices_count = vertices_count;
    object->indices_count = indices_count;

    STACK_DEALLOCATE(stack, vertices);
    STACK_DEALLOCATE(stack, indices);
}

void video_object_set_model(VideoObject* object, Matrix4 model)
{
    object->model = model;
}
