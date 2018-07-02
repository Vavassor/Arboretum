#include "video.h"

#include "assert.h"
#include "asset_paths.h"
#include "array2.h"
#include "bitmap.h"
#include "bmfont.h"
#include "bmp.h"
#include "closest_point_of_approach.h"
#include "colours.h"
#include "filesystem.h"
#include "float_utilities.h"
#include "gl_core_3_3.h"
#include "history.h"
#include "immediate.h"
#include "input.h"
#include "int_utilities.h"
#include "intersection.h"
#include "jan.h"
#include "logging.h"
#include "map.h"
#include "math_basics.h"
#include "obj.h"
#include "object_lady.h"
#include "shader.h"
#include "sorting.h"
#include "string_utilities.h"
#include "string_build.h"
#include "vector_math.h"
#include "vertex_layout.h"
#include "video_object.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

static GLuint nearest_repeat;
static GLuint linear_repeat;
static GLuint linear_mipmap_repeat;

static struct
{
    GLuint program;
    GLint model_view_projection;
} shader_vertex_colour;

static struct
{
    GLuint program;
    GLint model_view_projection;
    GLint texture;
} shader_texture_only;

static struct
{
    GLuint program;
    GLint model_view_projection;
    GLint texture;
    GLint text_colour;
} shader_font;

static struct
{
    GLuint program;
    GLint light_direction;
    GLint model_view_projection;
    GLint normal_matrix;
} shader_lit;

static struct
{
    GLuint program;
    GLint model_view_projection;
    GLint texture;
    GLint viewport_dimensions;
    GLint texture_dimensions;
    GLint pattern_scale;
} shader_screen_pattern;

static struct
{
    GLuint program;
    GLint model_view_projection;
    GLint halo_colour;
} shader_halo;

static struct
{
    GLuint program;
    GLint line_width;
    GLint model_view_projection;
    GLint projection_factor;
    GLint texture;
    GLint texture_dimensions;
    GLint viewport_dimensions;
} shader_line;

static struct
{
    GLuint program;
    GLint point_radius;
    GLint model_view_projection;
    GLint projection_factor;
    GLint texture;
    GLint viewport_dimensions;
} shader_point;

static VideoObject sky;
static DenseMap objects;

static Matrix4 sky_projection;
static Matrix4 screen_projection;
static Stack scratch;
static Heap heap;

static GLuint font_textures[1];
static GLuint hatch_pattern;
static GLuint line_pattern;
static GLuint point_pattern;

bool video_system_start_up()
{
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

    stack_create(&scratch, (uint32_t) uptibytes(1));
    heap_create(&heap, (uint32_t) uptibytes(1));

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

        glGenSamplers(1, &linear_mipmap_repeat);
        glSamplerParameteri(linear_mipmap_repeat, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glSamplerParameteri(linear_mipmap_repeat, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glSamplerParameteri(linear_mipmap_repeat, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glSamplerParameteri(linear_mipmap_repeat, GL_TEXTURE_WRAP_T, GL_REPEAT);
    }

    // Vertex Colour Shader
    shader_vertex_colour.program = load_shader_program("Vertex Colour.vs", "Vertex Colour.fs", &scratch);
    if(shader_vertex_colour.program == 0)
    {
        LOG_ERROR("The vertex colour shader failed to load.");
        return false;
    }
    {
        shader_vertex_colour.model_view_projection = glGetUniformLocation(shader_vertex_colour.program, "model_view_projection");
    }

    // Texture Only Shader
    shader_texture_only.program = load_shader_program("Texture Only.vs", "Texture Only.fs", &scratch);
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
    shader_font.program = load_shader_program("Font.vs", "Font.fs", &scratch);
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
        const Float3 text_colour = float3_white;
        glUniform3fv(shader_font.text_colour, 1, &text_colour.e[0]);
    }

    // Lit Shader
    shader_lit.program = load_shader_program("Lit.vs", "Lit.fs", &scratch);
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
    shader_halo.program = load_shader_program("Halo.vs", "Halo.fs", &scratch);
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
    shader_screen_pattern.program = load_shader_program("Screen Pattern.vs", "Screen Pattern.fs", &scratch);
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

        const Float2 pattern_scale = (Float2){{2.5f, 2.5f}};

        glUseProgram(shader_screen_pattern.program);
        glUniform1i(shader_screen_pattern.texture, 0);
        glUniform2fv(shader_screen_pattern.pattern_scale, 1, &pattern_scale.e[0]);
    }

    // Line Shader
    shader_line.program = load_shader_program("Line.vs", "Line.fs", &scratch);
    if(shader_line.program == 0)
    {
        LOG_ERROR("The line shader failed to load.");
        return false;
    }
    {
        GLuint program = shader_line.program;
        shader_line.line_width = glGetUniformLocation(program, "line_width");
        shader_line.model_view_projection = glGetUniformLocation(program, "model_view_projection");
        shader_line.projection_factor = glGetUniformLocation(program, "projection_factor");
        shader_line.texture = glGetUniformLocation(program, "texture");
        shader_line.texture_dimensions = glGetUniformLocation(program, "texture_dimensions");
        shader_line.viewport_dimensions = glGetUniformLocation(program, "viewport");

        glUseProgram(shader_line.program);
        glUniform1f(shader_line.line_width, 4.0f);
        glUniform1i(shader_line.texture, 0);
    }

    // Point Shader
    shader_point.program = load_shader_program("Point.vs", "Point.fs", &scratch);
    if(shader_point.program == 0)
    {
        LOG_ERROR("The point shader failed to load.");
        return false;
    }
    {
        GLuint program = shader_point.program;
        shader_point.point_radius = glGetUniformLocation(program, "point_radius");
        shader_point.model_view_projection = glGetUniformLocation(program, "model_view_projection");
        shader_point.projection_factor = glGetUniformLocation(program, "projection_factor");
        shader_point.texture = glGetUniformLocation(program, "texture");
        shader_point.viewport_dimensions = glGetUniformLocation(program, "viewport");

        glUseProgram(shader_point.program);
        glUniform1f(shader_point.point_radius, 4.0f);
        glUniform1i(shader_point.texture, 0);
    }

    dense_map_create(&objects, &heap);

    // Sky
    video_object_create(&sky, VERTEX_LAYOUT_PNC);
    video_object_generate_sky(&sky, &scratch);

    // Hatch pattern texture
    {
        char* path = get_image_path_by_name("polka_dot.png", &scratch);
        Bitmap bitmap;
        bitmap.pixels = stbi_load(path, &bitmap.width, &bitmap.height, &bitmap.bytes_per_pixel, STBI_default);
        STACK_DEALLOCATE(&scratch, path);
        hatch_pattern = upload_bitmap(&bitmap);
        stbi_image_free(bitmap.pixels);

        glUseProgram(shader_screen_pattern.program);
        glUniform2i(shader_screen_pattern.texture_dimensions, bitmap.width, bitmap.height);
    }

    // Line pattern texture
    {
        char* path = get_image_path_by_name("Line Feathering.png", &scratch);
        Bitmap bitmap;
        bitmap.pixels = stbi_load(path, &bitmap.width, &bitmap.height, &bitmap.bytes_per_pixel, STBI_default);
        STACK_DEALLOCATE(&scratch, path);
        line_pattern = upload_bitmap_with_mipmaps(&bitmap, &heap);
        stbi_image_free(bitmap.pixels);

        glUseProgram(shader_line.program);
        glUniform2i(shader_line.texture_dimensions, bitmap.width, bitmap.height);
    }

    // Point pattern texture
    {
        char* path = get_image_path_by_name("Point.png", &scratch);
        Bitmap bitmap;
        bitmap.pixels = stbi_load(path, &bitmap.width, &bitmap.height, &bitmap.bytes_per_pixel, STBI_default);
        STACK_DEALLOCATE(&scratch, path);
        point_pattern = upload_bitmap_with_mipmaps(&bitmap, &heap);
        stbi_image_free(bitmap.pixels);
    }

    immediate_context_create(&heap);
    immediate_set_shader(shader_vertex_colour.program);
    immediate_set_line_shader(shader_line.program);
    immediate_set_textured_shader(shader_font.program);

    return true;
}

void video_system_shut_down(bool functions_loaded)
{
    if(functions_loaded)
    {
        glDeleteSamplers(1, &nearest_repeat);
        glDeleteSamplers(1, &linear_repeat);
        glDeleteSamplers(1, &linear_mipmap_repeat);

        glDeleteProgram(shader_vertex_colour.program);
        glDeleteProgram(shader_texture_only.program);
        glDeleteProgram(shader_font.program);
        glDeleteProgram(shader_lit.program);
        glDeleteProgram(shader_halo.program);
        glDeleteProgram(shader_screen_pattern.program);
        glDeleteProgram(shader_line.program);
        glDeleteProgram(shader_point.program);

        for(int i = 0; i < 1; i += 1)
        {
            glDeleteTextures(1, &font_textures[i]);
        }
        glDeleteTextures(1, &hatch_pattern);
        glDeleteTextures(1, &line_pattern);
        glDeleteTextures(1, &point_pattern);

        video_object_destroy(&sky);

        immediate_context_destroy(&heap);
    }

    dense_map_destroy(&objects, &heap);

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

    Float4 x_axis_colour = (Float4){{1.0f, 0.0314f, 0.0314f, 1.0f}};
    Float4 y_axis_colour = (Float4){{0.3569f, 1.0f, 0.0f, 1.0f}};
    Float4 z_axis_colour = (Float4){{0.0863f, 0.0314f, 1.0f, 1.0f}};
    Float4 x_axis_shadow_colour = (Float4){{0.7f, 0.0314f, 0.0314f, 1.0f}};
    Float4 y_axis_shadow_colour = (Float4){{0.3569f, 0.7f, 0.0f, 1.0f}};
    Float4 z_axis_shadow_colour = (Float4){{0.0863f, 0.0314f, 0.7f, 1.0f}};

    Float4 yz_plane_colour = (Float4){{0.9f, 0.9f, 0.9f, 1.0f}};
    Float4 xz_plane_colour = (Float4){{0.9f, 0.9f, 0.9f, 1.0f}};
    Float4 xy_plane_colour = (Float4){{0.9f, 0.9f, 0.9f, 1.0f}};

    switch(hovered_axis)
    {
        case 0:
        {
            x_axis_colour = float4_yellow;
            x_axis_shadow_colour = (Float4){{0.8f, 0.8f, 0.0f, 1.0f}};
            break;
        }
        case 1:
        {
            y_axis_colour = float4_yellow;
            y_axis_shadow_colour = (Float4){{0.8f, 0.8f, 0.0f, 1.0f}};
            break;
        }
        case 2:
        {
            z_axis_colour = float4_yellow;
            z_axis_shadow_colour = (Float4){{0.8f, 0.8f, 0.0f, 1.0f}};
            break;
        }
    }
    switch(hovered_plane)
    {
        case 0:
        {
            yz_plane_colour = float4_yellow;
            break;
        }
        case 1:
        {
            xz_plane_colour = float4_yellow;
            break;
        }
        case 2:
        {
            xy_plane_colour = float4_yellow;
            break;
        }
    }
    switch(selected_axis)
    {
        case 0:
        {
            x_axis_colour = float4_white;
            x_axis_shadow_colour = (Float4){{0.8f, 0.8f, 0.8f, 1.0f}};
            break;
        }
        case 1:
        {
            y_axis_colour = float4_white;
            y_axis_shadow_colour = (Float4){{0.8f, 0.8f, 0.8f, 1.0f}};
            break;
        }
        case 2:
        {
            z_axis_colour = float4_white;
            z_axis_shadow_colour = (Float4){{0.8f, 0.8f, 0.8f, 1.0f}};
            break;
        }
    }

    const Float4 ball_colour = (Float4){{0.9f, 0.9f, 0.9f, 1.0f}};

    Float3 shaft_axis_x = (Float3){{shaft_length, 0.0f, 0.0f}};
    Float3 shaft_axis_y = (Float3){{0.0f, shaft_length, 0.0f}};
    Float3 shaft_axis_z = (Float3){{0.0f, 0.0f, shaft_length}};

    Float3 head_axis_x = (Float3){{head_height, 0.0f, 0.0f}};
    Float3 head_axis_y = (Float3){{0.0f, head_height, 0.0f}};
    Float3 head_axis_z = (Float3){{0.0f, 0.0f, head_height}};

    Float3 yz_plane = (Float3){{0.0f, shaft_length, shaft_length}};
    Float3 xz_plane = (Float3){{shaft_length, 0.0f, shaft_length}};
    Float3 xy_plane = (Float3){{shaft_length, shaft_length, 0.0f}};

    Float3 yz_plane_extents = (Float3){{plane_thickness, plane_extent, plane_extent}};
    Float3 xz_plane_extents = (Float3){{plane_extent, plane_thickness, plane_extent}};
    Float3 xy_plane_extents = (Float3){{plane_extent, plane_extent, plane_thickness}};

    immediate_add_cone(shaft_axis_x, head_axis_x, head_radius, x_axis_colour, x_axis_shadow_colour);
    immediate_add_cylinder(float3_zero, shaft_axis_x, shaft_radius, x_axis_colour);

    immediate_add_cone(shaft_axis_y, head_axis_y, head_radius, y_axis_colour, y_axis_shadow_colour);
    immediate_add_cylinder(float3_zero, shaft_axis_y, shaft_radius, y_axis_colour);

    immediate_add_cone(shaft_axis_z, head_axis_z, head_radius, z_axis_colour, z_axis_shadow_colour);
    immediate_add_cylinder(float3_zero, shaft_axis_z, shaft_radius, z_axis_colour);

    immediate_add_box(yz_plane, yz_plane_extents, yz_plane_colour);
    immediate_add_box(xz_plane, xz_plane_extents, xz_plane_colour);
    immediate_add_box(xy_plane, xy_plane_extents, xy_plane_colour);

    immediate_add_sphere(float3_zero, ball_radius, ball_colour);

    immediate_draw();
}

static void draw_arrow(Float3 start, Float3 end, float shaft_radius, float head_height, float head_radius)
{
    Float4 colour = (Float4){{0.9f, 0.9f, 0.9f, 1.0f}};
    Float4 shadow_colour = (Float4){{0.5f, 0.5f, 0.5f, 1.0f}};
    Float3 arrow = float3_subtract(end, start);
    float distance = float3_length(arrow);
    if(distance > 0.0f)
    {
        Float3 direction = float3_divide(arrow, distance);
        Float3 shaft = float3_madd(distance - head_height, direction, start);
        Float3 head_axis = float3_multiply(head_height, direction);
        immediate_add_cone(shaft, head_axis, head_radius, colour, shadow_colour);
        immediate_add_cylinder(start, shaft, shaft_radius, colour);
    }
}

static void draw_move_tool_vectors(MoveTool* tool)
{
    const float scale = 0.25f;
    float shaft_radius = scale * tool->shaft_radius;
    float head_height = scale * tool->head_height;
    float head_radius = scale * tool->head_radius;

    Float3 reference = tool->reference_position;

    if(is_valid_index(tool->selected_axis))
    {
        draw_arrow(reference, tool->position, shaft_radius, head_height, head_radius);
    }
    else if(is_valid_index(tool->selected_plane))
    {
        draw_arrow(reference, tool->position, shaft_radius, head_height, head_radius);

        Float3 move = float3_subtract(reference, tool->position);

        Float3 normal;
        switch(tool->selected_plane)
        {
            case 0:
            {
                normal = float3_unit_y;
                break;
            }
            case 1:
            {
                normal = float3_unit_x;
                break;
            }
            case 2:
            {
                normal = float3_unit_x;
                break;
            }
        }
        normal = quaternion_rotate(tool->orientation, normal);
        Float3 corner = float3_add(float3_reject(move, normal), tool->position);
        draw_arrow(reference, corner, shaft_radius, head_height, head_radius);
        draw_arrow(corner, tool->position, shaft_radius, head_height, head_radius);
    }

    immediate_draw();
}

static void draw_rotate_tool(bool silhouetted)
{
    const float radius = 2.0f;
    const float width = 0.3f;

    Float4 x_axis_colour = (Float4){{1.0f, 0.0314f, 0.0314f, 1.0f}};
    Float4 y_axis_colour = (Float4){{0.3569f, 1.0f, 0.0f, 1.0f}};
    Float4 z_axis_colour = (Float4){{0.0863f, 0.0314f, 1.0f, 1.0f}};

    Float3 center = (Float3){{-1.0f, 2.0f, 0.0f}};

    immediate_add_arc(center, float3_unit_x, pi_over_2, -pi_over_2, radius, width, x_axis_colour);
    immediate_add_arc(center, float3_negate(float3_unit_x), pi_over_2, pi, radius, width, x_axis_colour);

    immediate_add_arc(center, float3_unit_y, pi_over_2, -pi_over_2, radius, width, y_axis_colour);
    immediate_add_arc(center, float3_negate(float3_unit_y), pi_over_2, pi, radius, width, y_axis_colour);

    immediate_add_arc(center, float3_unit_z, pi_over_2, pi_over_2, radius, width, z_axis_colour);
    immediate_add_arc(center, float3_negate(float3_unit_z), pi_over_2, 0.0f, radius, width, z_axis_colour);

    immediate_draw();
}

static void draw_scale_tool(bool silhouetted)
{
    const float shaft_length = 2.0f * sqrtf(3.0f);
    const float shaft_radius = 0.125f;
    const float knob_extent = 0.3333f;
    const float brace = 0.6666f * shaft_length;
    const float brace_radius = shaft_radius / 2.0f;

    Float3 knob_extents = (Float3){{knob_extent, knob_extent, knob_extent}};

    Float3 shaft_axis_x = (Float3){{shaft_length, 0.0f, 0.0f}};
    Float3 shaft_axis_y = (Float3){{0.0f, shaft_length, 0.0f}};
    Float3 shaft_axis_z = (Float3){{0.0f, 0.0f, shaft_length}};

    Float3 brace_x = (Float3){{brace, 0.0f, 0.0f}};
    Float3 brace_y = (Float3){{0.0f, brace, 0.0f}};
    Float3 brace_z = (Float3){{0.0f, 0.0f, brace}};
    Float3 brace_xy = float3_divide(float3_add(brace_x, brace_y), 2.0f);
    Float3 brace_yz = float3_divide(float3_add(brace_y, brace_z), 2.0f);
    Float3 brace_xz = float3_divide(float3_add(brace_x, brace_z), 2.0f);

    const Float4 origin_colour = (Float4){{0.9f, 0.9f, 0.9f, 1.0f}};
    const Float4 x_axis_colour = (Float4){{1.0f, 0.0314f, 0.0314f, 1.0f}};
    const Float4 y_axis_colour = (Float4){{0.3569f, 1.0f, 0.0f, 1.0f}};
    const Float4 z_axis_colour = (Float4){{0.0863f, 0.0314f, 1.0f, 1.0f}};

    immediate_add_cylinder(float3_zero, shaft_axis_x, shaft_radius, x_axis_colour);
    immediate_add_box(shaft_axis_x, knob_extents, x_axis_colour);
    immediate_add_cylinder(brace_x, brace_xy, brace_radius, x_axis_colour);
    immediate_add_cylinder(brace_x, brace_xz, brace_radius, x_axis_colour);

    immediate_add_cylinder(float3_zero, shaft_axis_y, shaft_radius, y_axis_colour);
    immediate_add_box(shaft_axis_y, knob_extents, y_axis_colour);
    immediate_add_cylinder(brace_y, brace_xy, brace_radius, y_axis_colour);
    immediate_add_cylinder(brace_y, brace_yz, brace_radius, y_axis_colour);

    immediate_add_cylinder(float3_zero, shaft_axis_z, shaft_radius, z_axis_colour);
    immediate_add_box(shaft_axis_z, knob_extents, z_axis_colour);
    immediate_add_cylinder(brace_z, brace_xz, brace_radius, z_axis_colour);
    immediate_add_cylinder(brace_z, brace_yz, brace_radius, z_axis_colour);

    immediate_add_box(float3_zero, knob_extents, origin_colour);

    immediate_draw();
}

static void draw_object_with_halo(ObjectLady* lady, int index, Float4 colour)
{
    ASSERT(index != invalid_index);
    ASSERT(index >= 0 && index < array_count(lady->objects));

    VideoObject object = *video_get_object(lady->objects[index].video_object);

    // Draw the object.
    glUseProgram(shader_lit.program);

    glEnable(GL_STENCIL_TEST);
    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

    glStencilMask(0xff);
    glClear(GL_STENCIL_BUFFER_BIT);

    glStencilFunc(GL_ALWAYS, 1, 0xff);

    glUniformMatrix4fv(shader_lit.model_view_projection, 1, GL_TRUE, object.model_view_projection.e);
    glUniformMatrix4fv(shader_lit.normal_matrix, 1, GL_TRUE, object.normal.e);
    glBindVertexArray(object.vertex_array);
    glDrawElements(GL_TRIANGLES, object.indices_count, GL_UNSIGNED_SHORT, NULL);

    // Draw the halo.
    glUseProgram(shader_halo.program);

    glUniform4fv(shader_halo.halo_colour, 1, &colour.e[0]);

    glStencilFunc(GL_NOTEQUAL, 1, 0xff);
    glStencilMask(0x00);

    glLineWidth(3.0f);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    glUniformMatrix4fv(shader_halo.model_view_projection, 1, GL_TRUE, object.model_view_projection.e);
    glBindVertexArray(object.vertex_array);
    glDrawElements(GL_TRIANGLES, object.indices_count, GL_UNSIGNED_SHORT, NULL);

    glLineWidth(1.0f);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glDisable(GL_STENCIL_TEST);
}

static void draw_selection_object(VideoObject* object, VideoObject* pointcloud, VideoObject* wireframe, Matrix4 projection)
{
    const Float4 colour = (Float4){{1.0f, 0.5f, 0.0f, 0.8f}};

    // Draw selected faces.
    glUseProgram(shader_halo.program);

    glUniform4fv(shader_halo.halo_colour, 1, &colour.e[0]);

    glDepthFunc(GL_EQUAL);
    glDepthMask(GL_FALSE);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    if(object)
    {
        glUniformMatrix4fv(shader_halo.model_view_projection, 1, GL_TRUE, object->model_view_projection.e);
        glBindVertexArray(object->vertex_array);
        glDrawElements(GL_TRIANGLES, object->indices_count, GL_UNSIGNED_SHORT, NULL);
    }

    // Draw the wireframe.
    glUseProgram(shader_line.program);

    glDepthFunc(GL_LEQUAL);

    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(-1.0f, -1.0f);

    glActiveTexture(GL_TEXTURE0 + 0);
    glBindTexture(GL_TEXTURE_2D, line_pattern);
    glBindSampler(0, linear_mipmap_repeat);

    if(wireframe)
    {
        glUniformMatrix4fv(shader_line.model_view_projection, 1, GL_TRUE, wireframe->model_view_projection.e);
        glUniform1f(shader_line.projection_factor, projection.e[0]);
        glBindVertexArray(wireframe->vertex_array);
        glDrawElements(GL_TRIANGLES, wireframe->indices_count, GL_UNSIGNED_SHORT, NULL);
    }

    // Draw the pointcloud.
    glUseProgram(shader_point.program);

    glDisable(GL_POLYGON_OFFSET_FILL);

    glActiveTexture(GL_TEXTURE0 + 0);
    glBindTexture(GL_TEXTURE_2D, point_pattern);
    glBindSampler(0, linear_mipmap_repeat);

    if(pointcloud)
    {
        glUniformMatrix4fv(shader_point.model_view_projection, 1, GL_TRUE, pointcloud->model_view_projection.e);
        glUniform1f(shader_point.projection_factor, projection.e[0]);
        glBindVertexArray(pointcloud->vertex_array);
        glDrawElements(GL_TRIANGLES, pointcloud->indices_count, GL_UNSIGNED_SHORT, NULL);
    }

    // Reset to defaults.
    glDisable(GL_POLYGON_OFFSET_FILL);
    glDisable(GL_BLEND);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
}

void video_system_update(VideoUpdateState* update, Platform* platform)
{
    Camera* camera = update->camera;
    Int2 viewport = update->viewport;
    MoveTool* move_tool = update->move_tool;
    RotateTool* rotate_tool = update->rotate_tool;
    UiContext* ui_context = update->ui_context;
    UiItem* main_menu = update->main_menu;
    UiItem* dialog_panel = update->dialog_panel;
    bool dialog_enabled = update->dialog_enabled;
    ObjectLady* lady = update->lady;
    int hovered_object_index = update->hovered_object_index;
    int selected_object_index = update->selected_object_index;
    DenseMapId selection_id = update->selection_id;
    DenseMapId selection_pointcloud_id = update->selection_pointcloud_id;
    DenseMapId selection_wireframe_id = update->selection_wireframe_id;

    Matrix4 projection;

    // Update matrices.
    {
        projection = matrix4_perspective_projection(camera->field_of_view, (float) viewport.x, (float) viewport.y, camera->near_plane, camera->far_plane);

        Float3 direction = float3_normalise(float3_subtract(camera->target, camera->position));
        Matrix4 view = matrix4_look_at(float3_zero, direction, float3_unit_z);
        video_object_set_matrices(&sky, view, sky_projection);

        view = matrix4_look_at(camera->position, camera->target, float3_unit_z);

        FOR_ALL(VideoObject, objects.array)
        {
            video_object_set_matrices(it, view, projection);
        }

        immediate_set_matrices(view, projection);

        // Update light parameters.

        Float3 light_direction = (Float3){{0.7f, 0.4f, -1.0f}};
        light_direction = float3_normalise(float3_negate(matrix4_transform_vector(view, light_direction)));

        glUseProgram(shader_lit.program);
        glUniform3fv(shader_lit.light_direction, 1, &light_direction.e[0]);
    }

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(shader_lit.program);

    // Draw all unselected models.
    for(int i = 0; i < array_count(lady->objects); i += 1)
    {
        if(i == hovered_object_index || i == selected_object_index)
        {
            continue;
        }
        VideoObject object = *video_get_object(lady->objects[i].video_object);
        glUniformMatrix4fv(shader_lit.model_view_projection, 1, GL_TRUE, object.model_view_projection.e);
        glUniformMatrix4fv(shader_lit.normal_matrix, 1, GL_TRUE, object.normal.e);
        glBindVertexArray(object.vertex_array);
        glDrawElements(GL_TRIANGLES, object.indices_count, GL_UNSIGNED_SHORT, NULL);
    }

    // Draw the selected and hovered models.
    if(is_valid_index(selected_object_index))
    {
        draw_object_with_halo(lady, selected_object_index, float4_white);
    }
    if(is_valid_index(hovered_object_index) && hovered_object_index != selected_object_index)
    {
        draw_object_with_halo(lady, hovered_object_index, float4_yellow);
    }

    glUseProgram(shader_lit.program);

    // rotate tool
    {
        Matrix4 view = matrix4_look_at(camera->position, camera->target, float3_unit_z);
        immediate_set_matrices(view, projection);
        draw_rotate_tool(false);
    }

    // move tool
    if(is_valid_index(selected_object_index))
    {
        Matrix4 model = matrix4_compose_transform(move_tool->position, move_tool->orientation, float3_set_all(move_tool->scale));
        Matrix4 view = matrix4_look_at(camera->position, camera->target, float3_unit_z);

        // silhouette
        glDisable(GL_DEPTH_TEST);
        immediate_set_shader(shader_screen_pattern.program);
        glActiveTexture(GL_TEXTURE0 + 0);
        glBindTexture(GL_TEXTURE_2D, hatch_pattern);
        glBindSampler(0, linear_repeat);

        immediate_set_matrices(matrix4_multiply(view, model), projection);
        draw_move_tool(move_tool, true);

        immediate_set_matrices(view, projection);
        draw_move_tool_vectors(move_tool);

        // draw solid parts on top of silhouette
        glEnable(GL_DEPTH_TEST);
        immediate_set_shader(shader_vertex_colour.program);

        immediate_set_matrices(matrix4_multiply(view, model), projection);
        draw_move_tool(move_tool, false);

        immediate_set_matrices(view, projection);
        draw_move_tool_vectors(move_tool);
    }

    // Draw the sky behind everything else.
    {
        glDepthFunc(GL_EQUAL);
        glDepthRange(1.0, 1.0);
        glUseProgram(shader_vertex_colour.program);
        glUniformMatrix4fv(shader_vertex_colour.model_view_projection, 1, GL_TRUE, sky.model_view_projection.e);
        glBindVertexArray(sky.vertex_array);
        glDrawElements(GL_TRIANGLES, sky.indices_count, GL_UNSIGNED_SHORT, NULL);
        glDepthFunc(GL_LESS);
        glDepthRange(0.0, 1.0);
    }

    // Draw the selection itself.
    if(selection_id || selection_pointcloud_id || selection_wireframe_id)
    {
        VideoObject* object = NULL;
        if(selection_id)
        {
            object = video_get_object(selection_id);
        }

        VideoObject* pointcloud = NULL;
        if(selection_pointcloud_id)
        {
            pointcloud = video_get_object(selection_pointcloud_id);
        }

        VideoObject* wireframe = NULL;
        if(selection_wireframe_id)
        {
            wireframe = video_get_object(selection_wireframe_id);
        }

        draw_selection_object(object, pointcloud, wireframe, projection);
    }

    // Draw rotatory boys
    {
        glActiveTexture(GL_TEXTURE0 + 0);
        glBindTexture(GL_TEXTURE_2D, line_pattern);
        glBindSampler(0, linear_mipmap_repeat);

        const Float4 x_axis_colour = (Float4){{1.0f, 0.0314f, 0.0314f, 1.0f}};
        const Float4 y_axis_colour = (Float4){{0.3569f, 1.0f, 0.0f, 1.0f}};
        const Float4 z_axis_colour = (Float4){{0.0863f, 0.0314f, 1.0f, 1.0f}};

        Float3 center = rotate_tool->position;
        float radius = rotate_tool->radius;

        glUseProgram(shader_line.program);
        glUniform1f(shader_line.line_width, 8.0f);

        immediate_add_wire_arc(center, float3_unit_x, pi, rotate_tool->angles[0], radius, x_axis_colour);
        immediate_add_wire_arc(center, float3_unit_y, pi, rotate_tool->angles[1], radius, y_axis_colour);
        immediate_add_wire_arc(center, float3_unit_z, pi, rotate_tool->angles[2], radius, z_axis_colour);
        immediate_set_blend_mode(BLEND_MODE_TRANSPARENT);
        immediate_draw();

        glUniform1f(shader_line.line_width, 4.0f);
    }

    // Draw the little axes in the corner.
    {
        const int scale = 40;
        const int padding = 10;

        const Float3 x_axis = (Float3){{sqrtf(3.0f) / 2.0f, 0.0f, 0.0f}};
        const Float3 y_axis = (Float3){{0.0f, sqrtf(3.0f) / 2.0f, 0.0f}};
        const Float3 z_axis = (Float3){{0.0f, 0.0f, sqrtf(3.0f) / 2.0f}};

        const Float4 x_axis_colour = (Float4){{1.0f, 0.0314f, 0.0314f, 1.0f}};
        const Float4 y_axis_colour = (Float4){{0.3569f, 1.0f, 0.0f, 1.0f}};
        const Float4 z_axis_colour = (Float4){{0.0863f, 0.0314f, 1.0f, 1.0f}};
        const Float4 x_axis_shadow_colour = (Float4){{0.7f, 0.0314f, 0.0314f, 1.0f}};
        const Float4 y_axis_shadow_colour = (Float4){{0.3569f, 0.7f, 0.0f, 1.0f}};
        const Float4 z_axis_shadow_colour = (Float4){{0.0863f, 0.0314f, 0.7f, 1.0f}};

        int corner_x = viewport.x - scale - padding;
        int corner_y = padding;
        glViewport(corner_x, corner_y, scale, scale);
        glClear(GL_DEPTH_BUFFER_BIT);

        const float across = 3.0f * sqrtf(3.0f);
        const float extent = across / 2.0f;

        Float3 direction = float3_normalise(float3_subtract(camera->target, camera->position));
        Matrix4 view = matrix4_look_at(float3_zero, direction, float3_unit_z);
        Matrix4 axis_projection = matrix4_orthographic_projection(across, across, -extent, extent);
        immediate_set_matrices(view, axis_projection);

        Float3 double_x = float3_multiply(2.0f, x_axis);
        Float3 double_y = float3_multiply(2.0f, y_axis);
        Float3 double_z = float3_multiply(2.0f, z_axis);

        immediate_add_cone(double_x, x_axis, 0.5f, x_axis_colour, x_axis_shadow_colour);
        immediate_add_cylinder(float3_zero, double_x, 0.125f, x_axis_colour);
        immediate_add_cone(double_y, y_axis, 0.5f, y_axis_colour, y_axis_shadow_colour);
        immediate_add_cylinder(float3_zero, double_y, 0.125f, y_axis_colour);
        immediate_add_cone(double_z, z_axis, 0.5f, z_axis_colour, z_axis_shadow_colour);
        immediate_add_cylinder(float3_zero, double_z, 0.125f, z_axis_colour);
        immediate_draw();
    }

    // Draw the screen-space UI.
    glViewport(0, 0, viewport.x, viewport.y);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);

    Matrix4 screen_view = matrix4_identity;
    immediate_set_matrices(screen_view, screen_projection);

    // Draw the open file dialog.
    if(dialog_enabled)
    {
        glActiveTexture(GL_TEXTURE0 + 0);
        glBindTexture(GL_TEXTURE_2D, font_textures[0]);
        glBindSampler(0, nearest_repeat);

        Rect space;
        space.bottom_left = (Float2){{0.0f, -250.0f}};
        space.dimensions = (Float2){{300.0f, 500.0f}};
        ui_lay_out(dialog_panel, space, ui_context);
        ui_draw(dialog_panel, ui_context);

        ui_draw_focus_indicator(dialog_panel, ui_context);
    }

    // Draw the main menu.
    {
        glActiveTexture(GL_TEXTURE0 + 0);
        glBindTexture(GL_TEXTURE_2D, font_textures[0]);
        glBindSampler(0, nearest_repeat);

        Rect space;
        space.bottom_left = (Float2){{-viewport.x / 2.0f, viewport.y / 2.0f}};
        space.dimensions.x = (float) viewport.x;
        space.dimensions.y = 60.0f;
        space.bottom_left.y -= space.dimensions.y;
        ui_lay_out(main_menu, space, ui_context);
        ui_draw(main_menu, ui_context);

        ui_draw_focus_indicator(main_menu, ui_context);
    }

    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
}

void video_resize_viewport(Int2 dimensions, double dots_per_millimeter, float fov)
{
    float width = (float) dimensions.x;
    float height = (float) dimensions.y;
    sky_projection = matrix4_perspective_projection(fov, width, height, 0.001f, 1.0f);
    screen_projection = matrix4_orthographic_projection(width, height, -1.0f, 1.0f);

    // Update any shaders that use the viewport dimensions.
    glUseProgram(shader_screen_pattern.program);
    glUniform2f(shader_screen_pattern.viewport_dimensions, width, height);

    glUseProgram(shader_line.program);
    glUniform2f(shader_line.viewport_dimensions, width, height);

    glUseProgram(shader_point.program);
    glUniform2f(shader_point.viewport_dimensions, width, height);
}

DenseMapId video_add_object(VertexLayout vertex_layout)
{
    DenseMapId id = dense_map_add(&objects, &heap);
    VideoObject* object = dense_map_look_up(&objects, id);
    video_object_create(object, vertex_layout);
    return id;
}

void video_remove_object(DenseMapId id)
{
    VideoObject* object = dense_map_look_up(&objects, id);
    video_object_destroy(object);
    dense_map_remove(&objects, id, &heap);
}

VideoObject* video_get_object(DenseMapId id)
{
    return dense_map_look_up(&objects, id);
}

void video_set_up_font(BmfFont* font)
{
    for(int i = 0; i < font->pages_count; i += 1)
    {
        char* filename = font->pages[i].bitmap_filename;
        char* path = get_image_path_by_name(filename, &scratch);
        Bitmap bitmap;
        bitmap.pixels = stbi_load(path, &bitmap.width, &bitmap.height, &bitmap.bytes_per_pixel, STBI_default);
        STACK_DEALLOCATE(&scratch, filename);
        font_textures[i] = upload_bitmap(&bitmap);
        stbi_image_free(bitmap.pixels);
    }
}
