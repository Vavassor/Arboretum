#include "video.h"

#include "float_utilities.h"
#include "int_utilities.h"
#include "string_utilities.h"
#include "string_build.h"
#include "vector_math.h"
#include "gl_core_3_3.h"
#include "vertex_layout.h"
#include "shader.h"
#include "jan.h"
#include "intersection.h"
#include "input.h"
#include "logging.h"
#include "memory.h"
#include "assert.h"
#include "array.h"
#include "bmp.h"
#include "obj.h"
#include "immediate.h"
#include "math_basics.h"
#include "bmfont.h"
#include "ui.h"
#include "filesystem.h"
#include "platform.h"
#include "sorting.h"
#include "history.h"
#include "colours.h"
#include "loop_macros.h"
#include "move_tool.h"
#include "object_lady.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

static const int invalid_index = -1;

namespace video {

const char* lit_vertex_source = R"(
#version 330

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec4 colour;

uniform mat4x4 model_view_projection;
uniform mat4x4 normal_matrix;

out vec3 surface_normal;
out vec3 surface_colour;

void main()
{
    gl_Position = model_view_projection * vec4(position, 1.0);
    surface_normal = (normal_matrix * vec4(normal, 0.0)).xyz;
    surface_colour = colour.rgb;
}
)";

const char* lit_fragment_source = R"(
#version 330

layout(location = 0) out vec4 output_colour;

uniform vec3 light_direction;

in vec3 surface_normal;
in vec3 surface_colour;

float half_lambert(vec3 n, vec3 l)
{
    return 0.5 * dot(n, l) + 0.5;
}

float lambert(vec3 n, vec3 l)
{
    return max(dot(n, l), 0.0);
}

void main()
{
    float light = half_lambert(surface_normal, light_direction);
    output_colour = vec4(surface_colour * vec3(light), 1.0);
}
)";

const char* vertex_source_vertex_colour = R"(
#version 330

layout(location = 0) in vec3 position;
layout(location = 2) in vec4 colour;

uniform mat4x4 model_view_projection;

out vec4 surface_colour;

void main()
{
    gl_Position = model_view_projection * vec4(position, 1.0);
    surface_colour = colour;
}
)";

const char* fragment_source_vertex_colour = R"(
#version 330

layout(location = 0) out vec4 output_colour;

in vec4 surface_colour;

void main()
{
    output_colour = surface_colour;
}
)";

const char* vertex_source_texture_only = R"(
#version 330

layout(location = 0) in vec3 position;
layout(location = 3) in vec2 texcoord;

uniform mat4x4 model_view_projection;

out vec2 surface_texcoord;

void main()
{
    gl_Position = model_view_projection * vec4(position, 1.0);
    surface_texcoord = texcoord;
}
)";

const char* fragment_source_texture_only = R"(
#version 330

uniform sampler2D texture;

layout(location = 0) out vec4 output_colour;

in vec2 surface_texcoord;

void main()
{
    output_colour = vec4(texture2D(texture, surface_texcoord).rgb, 1.0);
}
)";


const char* vertex_source_font = R"(
#version 330

layout(location = 0) in vec3 position;
layout(location = 3) in vec2 texcoord;

uniform mat4x4 model_view_projection;

out vec2 surface_texcoord;

void main()
{
    gl_Position = model_view_projection * vec4(position, 1.0);
    surface_texcoord = texcoord;
}
)";

const char* fragment_source_font = R"(
#version 330

uniform sampler2D texture;
uniform vec3 text_colour;

layout(location = 0) out vec4 output_colour;

in vec2 surface_texcoord;

void main()
{
    vec4 colour = texture2D(texture, surface_texcoord);
    colour.rgb *= text_colour;
    output_colour = colour;
}
)";

const char* vertex_source_halo = R"(
#version 330

layout(location = 0) in vec3 position;

uniform mat4x4 model_view_projection;

void main()
{
    gl_Position = model_view_projection * vec4(position, 1.0);
}
)";

const char* fragment_source_halo = R"(
#version 330

layout(location = 0) out vec4 output_colour;

uniform vec4 halo_colour;

void main()
{
    output_colour = halo_colour;
}
)";

const char* vertex_source_screen_pattern = R"(
#version 330

layout(location = 0) in vec3 position;
layout(location = 2) in vec4 colour;

uniform mat4x4 model_view_projection;

out vec3 surface_texcoord;
out vec4 surface_colour;

void main()
{
    vec4 surface_position = model_view_projection * vec4(position, 1.0);
    gl_Position = surface_position;
    surface_texcoord = vec3(surface_position.xy, surface_position.w);
    surface_colour = colour;
}
)";

const char* fragment_source_screen_pattern = R"(
#version 330

uniform sampler2D texture;
uniform vec2 pattern_scale;
uniform vec2 viewport_dimensions;
uniform vec2 texture_dimensions;

layout(location = 0) out vec4 output_colour;

in vec3 surface_texcoord;
in vec4 surface_colour;

void main()
{
    vec2 screen_texcoord = surface_texcoord.xy / surface_texcoord.z;
    screen_texcoord *= viewport_dimensions;
    screen_texcoord /= texture_dimensions;
    screen_texcoord *= pattern_scale;
    output_colour = surface_colour * texture2D(texture, screen_texcoord);
}
)";

struct Object
{
    Matrix4 model;
    Matrix4 model_view_projection;
    Matrix4 normal;

    GLuint buffers[2];
    GLuint vertex_array;
    int indices_count;
};

void object_create(Object* object)
{
    object->model = matrix4_identity;

    glGenVertexArrays(1, &object->vertex_array);
    glGenBuffers(2, object->buffers);

    // Set up the vertex array.
    glBindVertexArray(object->vertex_array);

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

void object_update_mesh(Object* object, jan::Mesh* mesh, Heap* heap)
{
    VertexPNC* vertices;
    int vertices_count;
    u16* indices;
    int indices_count;
    jan::triangulate(mesh, heap, &vertices, &vertices_count, &indices, &indices_count);

    glBindVertexArray(object->vertex_array);

    const int vertex_size = sizeof(VertexPNC);
    GLsizei vertices_size = vertex_size * vertices_count;
    glBindBuffer(GL_ARRAY_BUFFER, object->buffers[0]);
    glBufferData(GL_ARRAY_BUFFER, vertices_size, vertices, GL_DYNAMIC_DRAW);

    GLsizei indices_size = sizeof(u16) * indices_count;
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, object->buffers[1]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices_size, indices, GL_DYNAMIC_DRAW);
    object->indices_count = indices_count;

    glBindVertexArray(0);

    HEAP_DEALLOCATE(heap, vertices);
    HEAP_DEALLOCATE(heap, indices);
}

static void object_set_matrices(Object* object, Matrix4 view, Matrix4 projection)
{
    Matrix4 model_view = view * object->model;
    object->model_view_projection = projection * model_view;
    object->normal = transpose(inverse_transform(model_view));
}

static void object_generate_sky(Object* object, Stack* stack)
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

struct Pixel8
{
    u8 r;
};

struct Pixel24
{
    u8 r, g, b;
};

struct Pixel32
{
    u8 a, r, g, b;
};

struct Bitmap
{
    void* pixels;
    int width;
    int height;
    int bytes_per_pixel;
};

GLuint upload_bitmap(Bitmap* bitmap)
{
    GLuint texture;

    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    switch(bitmap->bytes_per_pixel)
    {
        case 1:
        {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, bitmap->width, bitmap->height, 0, GL_RED, GL_UNSIGNED_BYTE, bitmap->pixels);
            break;
        }
        case 2:
        {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RG8, bitmap->width, bitmap->height, 0, GL_RG, GL_UNSIGNED_BYTE, bitmap->pixels);
            break;
        }
        case 3:
        {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, bitmap->width, bitmap->height, 0, GL_RGB, GL_UNSIGNED_BYTE, bitmap->pixels);
            break;
        }
        case 4:
        {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, bitmap->width, bitmap->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, bitmap->pixels);
            break;
        }
    }

    return texture;
}

DEFINE_ARRAY(Object);

struct DenseMap
{
    ArrayObject array;
    int* id_table_keys;
    DenseMapId* id_table_values;
    DenseMapId* index_table_keys;
    int* index_table_values;
    DenseMapId seed;
    int table_size;
    int table_filled;
};

static void create(DenseMap* map, Heap* heap)
{
    const int table_size = 1024;

    create(&map->array, heap);
    map->id_table_keys = HEAP_ALLOCATE(heap, int, table_size);
    map->id_table_values = HEAP_ALLOCATE(heap, DenseMapId, table_size);
    map->index_table_keys = HEAP_ALLOCATE(heap, DenseMapId, table_size);
    map->index_table_values = HEAP_ALLOCATE(heap, int, table_size);
    set_memory(map->id_table_keys, 0xff, sizeof(int) * table_size);
    set_memory(map->index_table_values, 0xff, sizeof(int) * table_size);
    map->seed = 1;
    map->table_size = table_size;
    map->table_filled = 0;
}

static void destroy(DenseMap* map, Heap* heap)
{
    if(map)
    {
        destroy(&map->array);
        SAFE_HEAP_DEALLOCATE(heap, map->id_table_keys);
        SAFE_HEAP_DEALLOCATE(heap, map->id_table_values);
        SAFE_HEAP_DEALLOCATE(heap, map->index_table_keys);
        SAFE_HEAP_DEALLOCATE(heap, map->index_table_values);
    }
}

static DenseMapId generate_id(DenseMap* map)
{
    DenseMapId id = map->seed;
    map->seed += 1;
    return id;
}

// a public domain 4-byte hash function by Bob Jenkins, adapted from a
// multiplicative method by Thomas Wang, to do it 6-shifts
// http://burtleburtle.net/bob/hash/integer.html
static u32 hash_bj6(u32 a)
{
    a = (a + 0x7ed55d16) + (a << 12);
    a = (a ^ 0xc761c23c) ^ (a >> 19);
    a = (a + 0x165667b1) + (a << 5);
    a = (a + 0xd3a2646c) ^ (a << 9);
    a = (a + 0xfd7046c5) + (a << 3);
    a = (a ^ 0xb55a4f09) ^ (a >> 16);
    return a;
}

static int hash_id(DenseMapId id, int n)
{
    return hash_bj6(id) & (n - 1);
}

static int hash_index(int index, int n)
{
    return hash_bj6(index) & (n - 1);
}

static void add_pair(DenseMap* map, DenseMapId id, int index)
{
    // Insert the ID into the ID table.
    ASSERT(can_use_bitwise_and_to_cycle(map->table_size));
    int probe = hash_index(index, map->table_size);
    while(map->id_table_keys[probe] != invalid_index)
    {
        probe = (probe + 1) & (map->table_size - 1);
    }
    map->id_table_keys[probe] = index;
    map->id_table_values[probe] = id;

    // Insert the index into the index table.
    probe = hash_id(id, map->table_size);
    while(map->index_table_keys[probe])
    {
        probe = (probe + 1) & (map->table_size - 1);
    }
    map->index_table_keys[probe] = id;
    map->index_table_values[probe] = index;
}

static DenseMapId add(DenseMap* map)
{
    reserve(&map->array, 1);
    int index = map->array.count;
    map->array.count += 1;

    DenseMapId id = generate_id(map);
    add_pair(map, id, index);

    map->table_filled += 1;
    if(map->table_filled >= map->table_size)
    {
        ASSERT(false);
        // TODO: resize
    }

    return id;
}

static int look_up_index(DenseMap* map, DenseMapId id)
{
    int probe = hash_id(id, map->table_size);
    while(map->index_table_keys[probe] != id && map->index_table_keys[probe])
    {
        probe = (probe + 1) & (map->table_size - 1);
    }
    int index = map->index_table_values[probe];
    return index;
}

static DenseMapId look_up_id(DenseMap* map, int index)
{
    int probe = hash_index(index, map->table_size);
    while(map->id_table_keys[probe] != index && map->id_table_keys[probe] != invalid_index)
    {
        probe = (probe + 1) & (map->table_size - 1);
    }
    DenseMapId id = map->id_table_values[probe];
    return id;
}

static Object* look_up(DenseMap* map, DenseMapId id)
{
    int index = look_up_index(map, id);
    return &map->array[index];
}

static void remove_pair(DenseMap* map, DenseMapId id, int index)
{
    int probe = hash_index(index, map->table_size);
    while(map->id_table_keys[probe] != index && map->id_table_keys[probe] != invalid_index)
    {
        probe = (probe + 1) & (map->table_size - 1);
    }
    map->id_table_keys[probe] = invalid_index;
    map->id_table_values[probe] = 0;

    probe = hash_id(id, map->table_size);
    while(map->index_table_keys[probe] != id && map->index_table_keys[probe])
    {
        probe = (probe + 1) & (map->table_size - 1);
    }
    map->index_table_keys[probe] = 0;
    map->index_table_values[probe] = invalid_index;
}

static void remove(DenseMap* map, DenseMapId id)
{
    int index = look_up_index(map, id);
    Object* object = &map->array[index];
    remove(&map->array, object);

    remove_pair(map, id, index);

    int moved_index = map->array.count;
    DenseMapId moved_id = look_up_id(map, moved_index);
    remove_pair(map, moved_id, moved_index);
    add_pair(map, moved_id, index);
}

namespace
{
    GLuint nearest_repeat;
    GLuint linear_repeat;

    struct
    {
        GLuint program;
        GLint model_view_projection;
    } shader_vertex_colour;

    struct
    {
        GLuint program;
        GLint model_view_projection;
        GLint texture;
    } shader_texture_only;

    struct
    {
        GLuint program;
        GLint model_view_projection;
        GLint texture;
        GLint text_colour;
    } shader_font;

    struct
    {
        GLuint program;
        GLint light_direction;
        GLint model_view_projection;
        GLint normal_matrix;
    } shader_lit;

    struct
    {
        GLuint program;
        GLint model_view_projection;
        GLint texture;
        GLint viewport_dimensions;
        GLint texture_dimensions;
        GLint pattern_scale;
    } shader_screen_pattern;

    struct
    {
        GLuint program;
        GLint model_view_projection;
        GLint halo_colour;
    } shader_halo;

    Object sky;
    DenseMap objects;

    Matrix4 sky_projection;
    Matrix4 screen_projection;
    Stack scratch;
    Heap heap;

    GLuint font_textures[1];
    GLuint hatch_pattern;
}

bool system_start_up()
{
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

    stack_create(&scratch, MEBIBYTES(16));
    heap_create(&heap, MEBIBYTES(16));

    // Setup samplers.
    {
        glGenSamplers(1, &nearest_repeat);
        glSamplerParameteri(nearest_repeat, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glSamplerParameteri(nearest_repeat, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glSamplerParameteri(nearest_repeat, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glSamplerParameteri(nearest_repeat, GL_TEXTURE_WRAP_T, GL_REPEAT);

        glGenSamplers(1, &linear_repeat);
        glSamplerParameteri(linear_repeat, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glSamplerParameteri(linear_repeat, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glSamplerParameteri(linear_repeat, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glSamplerParameteri(linear_repeat, GL_TEXTURE_WRAP_T, GL_REPEAT);
    }

    // Vertex Colour Shader
    shader_vertex_colour.program = load_shader_program(vertex_source_vertex_colour, fragment_source_vertex_colour, &scratch);
    if(shader_vertex_colour.program == 0)
    {
        LOG_ERROR("The vertex colour shader failed to load.");
        return false;
    }
    {
        shader_vertex_colour.model_view_projection = glGetUniformLocation(shader_vertex_colour.program, "model_view_projection");
    }

    // Texture Only Shader
    shader_texture_only.program = load_shader_program(vertex_source_texture_only, fragment_source_texture_only, &scratch);
    if(shader_texture_only.program == 0)
    {
        LOG_ERROR("The texture-only shader failed to load.");
        return false;
    }
    {
        GLuint program = shader_texture_only.program;
        shader_texture_only.model_view_projection = glGetUniformLocation(program, "model_view_projection");
        shader_texture_only.texture = glGetUniformLocation(program, "texture");

        glUseProgram(shader_texture_only.program);
        glUniform1i(shader_texture_only.texture, 0);
    }

    // Font Shader
    shader_font.program = load_shader_program(vertex_source_font, fragment_source_font, &scratch);
    if(shader_font.program == 0)
    {
        LOG_ERROR("The font shader failed to load.");
        return false;
    }
    {
        GLuint program = shader_font.program;
        shader_font.model_view_projection = glGetUniformLocation(program, "model_view_projection");
        shader_font.texture = glGetUniformLocation(program, "texture");
        shader_font.text_colour = glGetUniformLocation(program, "text_colour");

        glUseProgram(shader_font.program);
        glUniform1i(shader_font.texture, 0);
        const Vector3 text_colour = vector3_one;
        glUniform3fv(shader_font.text_colour, 1, &text_colour[0]);
    }

    // Lit Shader
    shader_lit.program = load_shader_program(lit_vertex_source, lit_fragment_source, &scratch);
    if(shader_lit.program == 0)
    {
        LOG_ERROR("The lit shader failed to load.");
        return false;
    }
    {
        GLuint program = shader_lit.program;
        shader_lit.model_view_projection = glGetUniformLocation(program, "model_view_projection");
        shader_lit.normal_matrix = glGetUniformLocation(program, "normal_matrix");
        shader_lit.light_direction = glGetUniformLocation(program, "light_direction");
    }

    // Halo Shader
    shader_halo.program = load_shader_program(vertex_source_halo, fragment_source_halo, &scratch);
    if(shader_halo.program == 0)
    {
        LOG_ERROR("The halo shader failed to load.");
        return false;
    }
    {
        GLuint program = shader_halo.program;
        shader_halo.model_view_projection = glGetUniformLocation(program, "model_view_projection");
        shader_halo.halo_colour = glGetUniformLocation(program, "halo_colour");
    }

    // Screen Pattern Shader
    shader_screen_pattern.program = load_shader_program(vertex_source_screen_pattern, fragment_source_screen_pattern, &scratch);
    if(shader_screen_pattern.program == 0)
    {
        LOG_ERROR("The screen pattern shader failed to load.");
        return false;
    }
    {
        GLuint program = shader_screen_pattern.program;
        shader_screen_pattern.model_view_projection = glGetUniformLocation(program, "model_view_projection");
        shader_screen_pattern.texture = glGetUniformLocation(program, "texture");
        shader_screen_pattern.viewport_dimensions = glGetUniformLocation(program, "viewport_dimensions");
        shader_screen_pattern.texture_dimensions = glGetUniformLocation(program, "texture_dimensions");
        shader_screen_pattern.pattern_scale = glGetUniformLocation(program, "pattern_scale");

        const Vector2 pattern_scale = {2.5f, 2.5f};

        glUseProgram(shader_screen_pattern.program);
        glUniform1i(shader_screen_pattern.texture, 0);
        glUniform2fv(shader_screen_pattern.pattern_scale, 1, &pattern_scale[0]);
    }

    create(&objects, &heap);

    // Sky
    object_create(&sky);
    object_generate_sky(&sky, &scratch);

    // Hatch pattern texture
    {
        const char* path = "polka_dot.png";
        Bitmap bitmap;
        bitmap.pixels = stbi_load(path, &bitmap.width, &bitmap.height, &bitmap.bytes_per_pixel, STBI_default);
        hatch_pattern = upload_bitmap(&bitmap);
        stbi_image_free(bitmap.pixels);

        glUseProgram(shader_screen_pattern.program);
        glUniform2f(shader_screen_pattern.texture_dimensions, bitmap.width, bitmap.height);
    }

    immediate::context_create(&heap);
    immediate::set_shader(shader_vertex_colour.program);
    immediate::set_textured_shader(shader_font.program);

    return true;
}

void system_shut_down(bool functions_loaded)
{
    if(functions_loaded)
    {
        glDeleteSamplers(1, &nearest_repeat);
        glDeleteSamplers(1, &linear_repeat);

        glDeleteProgram(shader_vertex_colour.program);
        glDeleteProgram(shader_texture_only.program);
        glDeleteProgram(shader_font.program);
        glDeleteProgram(shader_lit.program);
        glDeleteProgram(shader_halo.program);
        glDeleteProgram(shader_screen_pattern.program);

        FOR_N(i, 1)
        {
            glDeleteTextures(1, &font_textures[i]);
        }
        glDeleteTextures(1, &hatch_pattern);

        object_destroy(&sky);

        immediate::context_destroy(&heap);
    }

    destroy(&objects, &heap);

    stack_destroy(&scratch);
    heap_destroy(&heap);
}

static void draw_move_tool(MoveTool* tool, bool silhouetted)
{
    const float ball_radius = 0.4f;

    float shaft_length = tool->shaft_length;
    float shaft_radius = tool->shaft_radius;
    float head_height = tool->head_height;
    float head_radius = tool->head_radius;
    float plane_extent = tool->plane_extent;
    float plane_thickness = tool->plane_thickness;
    int hovered_axis = tool->hovered_axis;
    int hovered_plane = tool->hovered_plane;
    int selected_axis = tool->selected_axis;

    Vector4 x_axis_colour = {1.0f, 0.0314f, 0.0314f, 1.0f};
    Vector4 y_axis_colour = {0.3569f, 1.0f, 0.0f, 1.0f};
    Vector4 z_axis_colour = {0.0863f, 0.0314f, 1.0f, 1.0f};
    Vector4 x_axis_shadow_colour = {0.7f, 0.0314f, 0.0314f, 1.0f};
    Vector4 y_axis_shadow_colour = {0.3569f, 0.7f, 0.0f, 1.0f};
    Vector4 z_axis_shadow_colour = {0.0863f, 0.0314f, 0.7f, 1.0f};

    Vector4 yz_plane_colour = {0.9f, 0.9f, 0.9f, 1.0f};
    Vector4 xz_plane_colour = {0.9f, 0.9f, 0.9f, 1.0f};
    Vector4 xy_plane_colour = {0.9f, 0.9f, 0.9f, 1.0f};

    switch(hovered_axis)
    {
        case 0:
        {
            x_axis_colour = vector4_yellow;
            x_axis_shadow_colour = {0.8f, 0.8f, 0.0f, 1.0f};
            break;
        }
        case 1:
        {
            y_axis_colour = vector4_yellow;
            y_axis_shadow_colour = {0.8f, 0.8f, 0.0f, 1.0f};
            break;
        }
        case 2:
        {
            z_axis_colour = vector4_yellow;
            z_axis_shadow_colour = {0.8f, 0.8f, 0.0f, 1.0f};
            break;
        }
    }
    switch(hovered_plane)
    {
        case 0:
        {
            yz_plane_colour = vector4_yellow;
            break;
        }
        case 1:
        {
            xz_plane_colour = vector4_yellow;
            break;
        }
        case 2:
        {
            xy_plane_colour = vector4_yellow;
            break;
        }
    }
    switch(selected_axis)
    {
        case 0:
        {
            x_axis_colour = vector4_white;
            x_axis_shadow_colour = {0.8f, 0.8f, 0.8f, 1.0f};
            break;
        }
        case 1:
        {
            y_axis_colour = vector4_white;
            y_axis_shadow_colour = {0.8f, 0.8f, 0.8f, 1.0f};
            break;
        }
        case 2:
        {
            z_axis_colour = vector4_white;
            z_axis_shadow_colour = {0.8f, 0.8f, 0.8f, 1.0f};
            break;
        }
    }

    const Vector4 ball_colour = {0.9f, 0.9f, 0.9f, 1.0f};

    Vector3 shaft_axis_x = {shaft_length, 0.0f, 0.0f};
    Vector3 shaft_axis_y = {0.0f, shaft_length, 0.0f};
    Vector3 shaft_axis_z = {0.0f, 0.0f, shaft_length};

    Vector3 head_axis_x = {head_height, 0.0f, 0.0f};
    Vector3 head_axis_y = {0.0f, head_height, 0.0f};
    Vector3 head_axis_z = {0.0f, 0.0f, head_height};

    Vector3 yz_plane = {0.0f, shaft_length, shaft_length};
    Vector3 xz_plane = {shaft_length, 0.0f, shaft_length};
    Vector3 xy_plane = {shaft_length, shaft_length, 0.0f};

    Vector3 yz_plane_extents = {plane_thickness, plane_extent, plane_extent};
    Vector3 xz_plane_extents = {plane_extent, plane_thickness, plane_extent};
    Vector3 xy_plane_extents = {plane_extent, plane_extent, plane_thickness};

    immediate::add_cone(shaft_axis_x, head_axis_x, head_radius, x_axis_colour, x_axis_shadow_colour);
    immediate::add_cylinder(vector3_zero, shaft_axis_x, shaft_radius, x_axis_colour);

    immediate::add_cone(shaft_axis_y, head_axis_y, head_radius, y_axis_colour, y_axis_shadow_colour);
    immediate::add_cylinder(vector3_zero, shaft_axis_y, shaft_radius, y_axis_colour);

    immediate::add_cone(shaft_axis_z, head_axis_z, head_radius, z_axis_colour, z_axis_shadow_colour);
    immediate::add_cylinder(vector3_zero, shaft_axis_z, shaft_radius, z_axis_colour);

    immediate::add_box(yz_plane, yz_plane_extents, yz_plane_colour);
    immediate::add_box(xz_plane, xz_plane_extents, xz_plane_colour);
    immediate::add_box(xy_plane, xy_plane_extents, xy_plane_colour);

    immediate::add_sphere(vector3_zero, ball_radius, ball_colour);

    immediate::draw();
}

static void draw_arrow(Vector3 start, Vector3 end, float shaft_radius, float head_height, float head_radius)
{
    Vector4 colour = {0.9f, 0.9f, 0.9f, 1.0f};
    Vector4 shadow_colour = {0.5f, 0.5f, 0.5f, 1.0f};
    Vector3 arrow = end - start;
    float distance = length(arrow);
    if(distance > 0.0f)
    {
        Vector3 direction = arrow / distance;
        Vector3 shaft = (distance - head_height) * direction + start;
        Vector3 head_axis = head_height * direction;
        immediate::add_cone(shaft, head_axis, head_radius, colour, shadow_colour);
        immediate::add_cylinder(start, shaft, shaft_radius, colour);
    }
}

static void draw_move_tool_vectors(MoveTool* tool)
{
    const float scale = 0.25f;
    float shaft_radius = scale * tool->shaft_radius;
    float head_height = scale * tool->head_height;
    float head_radius = scale * tool->head_radius;

    Vector3 reference = tool->reference_position;

    if(tool->selected_axis != invalid_index)
    {
        draw_arrow(reference, tool->position, shaft_radius, head_height, head_radius);
    }
    else if(tool->selected_plane != invalid_index)
    {
        draw_arrow(reference, tool->position, shaft_radius, head_height, head_radius);

        Vector3 move = reference - tool->position;

        Vector3 normal;
        switch(tool->selected_plane)
        {
            case 0:
            {
                normal = vector3_unit_y;
                break;
            }
            case 1:
            {
                normal = vector3_unit_x;
                break;
            }
            case 2:
            {
                normal = vector3_unit_x;
                break;
            }
        }
        normal = tool->orientation * normal;
        Vector3 corner = project_onto_plane(move, normal) + tool->position;
        draw_arrow(reference, corner, shaft_radius, head_height, head_radius);
        draw_arrow(corner, tool->position, shaft_radius, head_height, head_radius);
    }

    immediate::draw();
}

void draw_rotate_tool(bool silhouetted)
{
    const float radius = 2.0f;
    const float width = 0.3f;

    Vector4 x_axis_colour = {1.0f, 0.0314f, 0.0314f, 1.0f};
    Vector4 y_axis_colour = {0.3569f, 1.0f, 0.0f, 1.0f};
    Vector4 z_axis_colour = {0.0863f, 0.0314f, 1.0f, 1.0f};

    immediate::add_arc(vector3_zero, vector3_unit_x, pi_over_2, -pi_over_2, radius, width, x_axis_colour);
    immediate::add_arc(vector3_zero, -vector3_unit_x, pi_over_2, pi, radius, width, x_axis_colour);

    immediate::add_arc(vector3_zero, vector3_unit_y, pi_over_2, -pi_over_2, radius, width, y_axis_colour);
    immediate::add_arc(vector3_zero, -vector3_unit_y, pi_over_2, pi, radius, width, y_axis_colour);

    immediate::add_arc(vector3_zero, vector3_unit_z, pi_over_2, pi_over_2, radius, width, z_axis_colour);
    immediate::add_arc(vector3_zero, -vector3_unit_z, pi_over_2, 0.0f, radius, width, z_axis_colour);

    immediate::draw();
}

void draw_scale_tool(bool silhouetted)
{
    const float shaft_length = 2.0f * sqrt(3.0f);
    const float shaft_radius = 0.125f;
    const float knob_extent = 0.3333f;
    const float brace = 0.6666f * shaft_length;
    const float brace_radius = shaft_radius / 2.0f;

    Vector3 knob_extents = {knob_extent, knob_extent, knob_extent};

    Vector3 shaft_axis_x = {shaft_length, 0.0f, 0.0f};
    Vector3 shaft_axis_y = {0.0f, shaft_length, 0.0f};
    Vector3 shaft_axis_z = {0.0f, 0.0f, shaft_length};

    Vector3 brace_x = {brace, 0.0f, 0.0f};
    Vector3 brace_y = {0.0f, brace, 0.0f};
    Vector3 brace_z = {0.0f, 0.0f, brace};
    Vector3 brace_xy = (brace_x + brace_y) / 2.0f;
    Vector3 brace_yz = (brace_y + brace_z) / 2.0f;
    Vector3 brace_xz = (brace_x + brace_z) / 2.0f;

    const Vector4 origin_colour = {0.9f, 0.9f, 0.9f, 1.0f};
    const Vector4 x_axis_colour = {1.0f, 0.0314f, 0.0314f, 1.0f};
    const Vector4 y_axis_colour = {0.3569f, 1.0f, 0.0f, 1.0f};
    const Vector4 z_axis_colour = {0.0863f, 0.0314f, 1.0f, 1.0f};

    immediate::add_cylinder(vector3_zero, shaft_axis_x, shaft_radius, x_axis_colour);
    immediate::add_box(shaft_axis_x, knob_extents, x_axis_colour);
    immediate::add_cylinder(brace_x, brace_xy, brace_radius, x_axis_colour);
    immediate::add_cylinder(brace_x, brace_xz, brace_radius, x_axis_colour);

    immediate::add_cylinder(vector3_zero, shaft_axis_y, shaft_radius, y_axis_colour);
    immediate::add_box(shaft_axis_y, knob_extents, y_axis_colour);
    immediate::add_cylinder(brace_y, brace_xy, brace_radius, y_axis_colour);
    immediate::add_cylinder(brace_y, brace_yz, brace_radius, y_axis_colour);

    immediate::add_cylinder(vector3_zero, shaft_axis_z, shaft_radius, z_axis_colour);
    immediate::add_box(shaft_axis_z, knob_extents, z_axis_colour);
    immediate::add_cylinder(brace_z, brace_xz, brace_radius, z_axis_colour);
    immediate::add_cylinder(brace_z, brace_yz, brace_radius, z_axis_colour);

    immediate::add_box(vector3_zero, knob_extents, origin_colour);

    immediate::draw();
}

static void draw_object_with_halo(ObjectLady* lady, int index, Vector4 colour)
{
    ASSERT(index != invalid_index);
    ASSERT(index >= 0 && index < lady->objects_count);

    Object object = *get_object(lady->objects[index].video_object);

    // Draw the object.
    glUseProgram(shader_lit.program);

    glEnable(GL_STENCIL_TEST);
    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

    glStencilMask(0xff);
    glClear(GL_STENCIL_BUFFER_BIT);

    glStencilFunc(GL_ALWAYS, 1, 0xff);

    glUniformMatrix4fv(shader_lit.model_view_projection, 1, GL_TRUE, object.model_view_projection.elements);
    glUniformMatrix4fv(shader_lit.normal_matrix, 1, GL_TRUE, object.normal.elements);
    glBindVertexArray(object.vertex_array);
    glDrawElements(GL_TRIANGLES, object.indices_count, GL_UNSIGNED_SHORT, nullptr);

    // Draw the halo.
    glUseProgram(shader_halo.program);

    glUniform4fv(shader_halo.halo_colour, 1, &colour[0]);

    glStencilFunc(GL_NOTEQUAL, 1, 0xff);
    glStencilMask(0x00);

    glLineWidth(3.0f);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    glUniformMatrix4fv(shader_halo.model_view_projection, 1, GL_TRUE, object.model_view_projection.elements);
    glBindVertexArray(object.vertex_array);
    glDrawElements(GL_TRIANGLES, object.indices_count, GL_UNSIGNED_SHORT, nullptr);

    glLineWidth(1.0f);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glDisable(GL_STENCIL_TEST);
}

void system_update(UpdateState* update, Platform* platform)
{
    Camera* camera = update->camera;
    Viewport viewport = update->viewport;
    MoveTool* move_tool = update->move_tool;
    ui::Context* ui_context = update->ui_context;
    ui::Item* main_menu = update->main_menu;
    ui::Item* dialog_panel = update->dialog_panel;
    bool dialog_enabled = update->dialog_enabled;
    ObjectLady* lady = update->lady;
    int hovered_object_index = update->hovered_object_index;
    int selected_object_index = update->selected_object_index;

    Matrix4 projection;

    // Update matrices.
    {
        projection = perspective_projection_matrix(camera->field_of_view, viewport.width, viewport.height, camera->near_plane, camera->far_plane);

        Vector3 direction = normalise(camera->target - camera->position);
        Matrix4 view = look_at_matrix(vector3_zero, direction, vector3_unit_z);
        object_set_matrices(&sky, view, sky_projection);

        view = look_at_matrix(camera->position, camera->target, vector3_unit_z);

        FOR_N(i, objects.array.count)
        {
            Object* object = &objects.array[i];
            object_set_matrices(object, view, projection);
        }

        immediate::set_matrices(view, projection);

        // Update light parameters.

        Vector3 light_direction = {0.7f, 0.4f, -1.0f};
        light_direction = normalise(-transform_vector(view, light_direction));

        glUseProgram(shader_lit.program);
        glUniform3fv(shader_lit.light_direction, 1, &light_direction[0]);
    }

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(shader_lit.program);

    // Draw all unselected models.
    FOR_N(i, lady->objects_count)
    {
        if(i == selected_object_index || i == hovered_object_index)
        {
            continue;
        }
        Object object = *get_object(lady->objects[i].video_object);
        glUniformMatrix4fv(shader_lit.model_view_projection, 1, GL_TRUE, object.model_view_projection.elements);
        glUniformMatrix4fv(shader_lit.normal_matrix, 1, GL_TRUE, object.normal.elements);
        glBindVertexArray(object.vertex_array);
        glDrawElements(GL_TRIANGLES, object.indices_count, GL_UNSIGNED_SHORT, nullptr);
    }

    // Draw the selected and hovered models.
    if(selected_object_index != invalid_index)
    {
        draw_object_with_halo(lady, selected_object_index, vector4_white);
    }
    if(hovered_object_index != invalid_index && hovered_object_index != selected_object_index)
    {
        draw_object_with_halo(lady, hovered_object_index, vector4_yellow);
    }

    glUseProgram(shader_lit.program);

    // rotate tool
    {
        Matrix4 view = look_at_matrix(camera->position, camera->target, vector3_unit_z);
        immediate::set_matrices(view, projection);
        draw_rotate_tool(false);
    }

    // move tool
    if(selected_object_index != invalid_index)
    {
        Matrix4 model = compose_transform(move_tool->position, move_tool->orientation, set_all_vector3(move_tool->scale));
        Matrix4 view = look_at_matrix(camera->position, camera->target, vector3_unit_z);

        // silhouette
        glDisable(GL_DEPTH_TEST);
        immediate::set_shader(shader_screen_pattern.program);
        glActiveTexture(GL_TEXTURE0 + 0);
        glBindTexture(GL_TEXTURE_2D, hatch_pattern);
        glBindSampler(0, linear_repeat);

        immediate::set_matrices(view * model, projection);
        draw_move_tool(move_tool, true);

        immediate::set_matrices(view, projection);
        draw_move_tool_vectors(move_tool);

        // draw solid parts on top of silhouette
        glEnable(GL_DEPTH_TEST);
        immediate::set_shader(shader_vertex_colour.program);

        immediate::set_matrices(view * model, projection);
        draw_move_tool(move_tool, false);

        immediate::set_matrices(view, projection);
        draw_move_tool_vectors(move_tool);
    }

    // Draw the sky behind everything else.
    {
        glDepthFunc(GL_EQUAL);
        glDepthRange(1.0, 1.0);
        glUseProgram(shader_vertex_colour.program);
        glUniformMatrix4fv(shader_vertex_colour.model_view_projection, 1, GL_TRUE, sky.model_view_projection.elements);
        glBindVertexArray(sky.vertex_array);
        glDrawElements(GL_TRIANGLES, sky.indices_count, GL_UNSIGNED_SHORT, nullptr);
        glDepthFunc(GL_LESS);
        glDepthRange(0.0, 1.0);
    }

    // Draw the little axes in the corner.
    {
        const int scale = 40;
        const int padding = 10;

        const Vector3 x_axis = {sqrt(3.0f) / 2.0f, 0.0f, 0.0f};
        const Vector3 y_axis = {0.0f, sqrt(3.0f) / 2.0f, 0.0f};
        const Vector3 z_axis = {0.0f, 0.0f, sqrt(3.0f) / 2.0f};

        const Vector4 x_axis_colour = {1.0f, 0.0314f, 0.0314f, 1.0f};
        const Vector4 y_axis_colour = {0.3569f, 1.0f, 0.0f, 1.0f};
        const Vector4 z_axis_colour = {0.0863f, 0.0314f, 1.0f, 1.0f};
        const Vector4 x_axis_shadow_colour = {0.7f, 0.0314f, 0.0314f, 1.0f};
        const Vector4 y_axis_shadow_colour = {0.3569f, 0.7f, 0.0f, 1.0f};
        const Vector4 z_axis_shadow_colour = {0.0863f, 0.0314f, 0.7f, 1.0f};

        int corner_x = viewport.width - scale - padding;
        int corner_y = padding;
        glViewport(corner_x, corner_y, scale, scale);
        glClear(GL_DEPTH_BUFFER_BIT);

        const float across = 3.0f * sqrt(3.0f);
        const float extent = across / 2.0f;

        Vector3 direction = normalise(camera->target - camera->position);
        Matrix4 view = look_at_matrix(vector3_zero, direction, vector3_unit_z);
        Matrix4 axis_projection = orthographic_projection_matrix(across, across, -extent, extent);
        immediate::set_matrices(view, axis_projection);

        immediate::add_cone(2.0f * x_axis, x_axis, 0.5f, x_axis_colour, x_axis_shadow_colour);
        immediate::add_cylinder(vector3_zero, 2.0f * x_axis, 0.125f, x_axis_colour);
        immediate::add_cone(2.0f * y_axis, y_axis, 0.5f, y_axis_colour, y_axis_shadow_colour);
        immediate::add_cylinder(vector3_zero, 2.0f * y_axis, 0.125f, y_axis_colour);
        immediate::add_cone(2.0f * z_axis, z_axis, 0.5f, z_axis_colour, z_axis_shadow_colour);
        immediate::add_cylinder(vector3_zero, 2.0f * z_axis, 0.125f, z_axis_colour);
        immediate::draw();
    }

    // Draw the screen-space UI.
    glViewport(0, 0, viewport.width, viewport.height);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);

    Matrix4 screen_view = matrix4_identity;
    immediate::set_matrices(screen_view, screen_projection);

    // Draw the open file dialog.
    if(dialog_enabled)
    {
        glActiveTexture(GL_TEXTURE0 + 0);
        glBindTexture(GL_TEXTURE_2D, font_textures[0]);
        glBindSampler(0, nearest_repeat);

        Rect space;
        space.bottom_left = {0.0f, -250.0f};
        space.dimensions = {300.0f, 500.0f};
        ui::lay_out(dialog_panel, space, &scratch);
        ui::draw(dialog_panel, ui_context);
    }

    // Draw the main menu.
    {
        glActiveTexture(GL_TEXTURE0 + 0);
        glBindTexture(GL_TEXTURE_2D, font_textures[0]);
        glBindSampler(0, nearest_repeat);

        Rect space;
        space.bottom_left = {-viewport.width / 2.0f, viewport.height / 2.0f};
        space.dimensions = {viewport.width, 60.0f};
        space.bottom_left.y -= space.dimensions.y;
        ui::lay_out(main_menu, space, &scratch);
        ui::draw(main_menu, ui_context);
    }

    // test anime
    {
        glActiveTexture(GL_TEXTURE0 + 0);
        glBindTexture(GL_TEXTURE_2D, font_textures[0]);
        glBindSampler(0, nearest_repeat);

        Rect space;
        space.bottom_left = {-200.0f, -250.0f};
        space.dimensions = {300.0f, 500.0f};
        ui::lay_out(update->test_anime, space, &scratch);
        ui::draw(update->test_anime, ui_context);
    }

    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

    // Output a test screenshot.
    if(input::get_key_tapped(input::Key::F12))
    {
        int pixels_count = viewport.width * viewport.height;
        Pixel24* pixels = STACK_ALLOCATE(&scratch, Pixel24, pixels_count);
        glReadPixels(0, 0, viewport.width, viewport.height, GL_BGR, GL_UNSIGNED_BYTE, pixels);
        bmp::write_file("test.bmp", reinterpret_cast<const u8*>(pixels), viewport.width, viewport.height);
        STACK_DEALLOCATE(&scratch, pixels);
    }
}

void resize_viewport(int width, int height, double dots_per_millimeter, float fov)
{
    sky_projection = perspective_projection_matrix(fov, width, height, 0.001f, 1.0f);
    screen_projection = orthographic_projection_matrix(width, height, -1.0f, 1.0f);

    // Update any shaders that use the viewport dimensions.
    glUseProgram(shader_screen_pattern.program);
    glUniform2f(shader_screen_pattern.viewport_dimensions, width, height);
}

DenseMapId add_object()
{
    DenseMapId id = add(&objects);
    Object* object = look_up(&objects, id);
    object_create(object);
    return id;
}

void remove_object(DenseMapId id)
{
    Object* object = look_up(&objects, id);
    object_destroy(object);
    remove(&objects, id);
}

Object* get_object(DenseMapId id)
{
    return look_up(&objects, id);
}

void set_up_font(bmfont::Font* font)
{
    FOR_N(i, font->pages_count)
    {
        const char* path = font->pages[i].bitmap_filename;
        Bitmap bitmap;
        bitmap.pixels = stbi_load(path, &bitmap.width, &bitmap.height, &bitmap.bytes_per_pixel, STBI_default);
        font_textures[i] = upload_bitmap(&bitmap);
        stbi_image_free(bitmap.pixels);
    }
}

} // namespace video
