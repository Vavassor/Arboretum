#include "video.h"

#include "assert.h"
#include "asset_paths.h"
#include "array2.h"
#include "bitmap.h"
#include "bmfont.h"
#include "bmp.h"
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
#include "memory.h"
#include "move_tool.h"
#include "obj.h"
#include "object_lady.h"
#include "platform.h"
#include "shader.h"
#include "sorting.h"
#include "string_utilities.h"
#include "string_build.h"
#include "ui.h"
#include "vector_math.h"
#include "vertex_layout.h"
#include "video_object.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace video {

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

    struct
    {
        GLuint program;
        GLint line_width;
        GLint model_view_projection;
        GLint projection_factor;
        GLint texture;
        GLint texture_dimensions;
        GLint viewport_dimensions;
    } shader_line;

    Object sky;
    DenseMap objects;

    Matrix4 sky_projection;
    Matrix4 screen_projection;
    Stack scratch;
    Heap heap;

    GLuint font_textures[1];
    GLuint hatch_pattern;
    GLuint line_pattern;
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
        const Vector3 text_colour = vector3_white;
        glUniform3fv(shader_font.text_colour, 1, &text_colour[0]);
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

        const Vector2 pattern_scale = {2.5f, 2.5f};

        glUseProgram(shader_screen_pattern.program);
        glUniform1i(shader_screen_pattern.texture, 0);
        glUniform2fv(shader_screen_pattern.pattern_scale, 1, &pattern_scale[0]);
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
        glUniform1f(shader_line.line_width, 3.0f);
        glUniform1i(shader_line.texture, 0);
    }

    create(&objects, &heap);

    // Sky
    object_create(&sky, VertexLayout::PNC);
    object_generate_sky(&sky, &scratch);

    // Hatch pattern texture
    {
        char* path = get_image_path_by_name("polka_dot.png", &scratch);
        Bitmap bitmap;
        bitmap.pixels = stbi_load(path, &bitmap.width, &bitmap.height, &bitmap.bytes_per_pixel, STBI_default);
        STACK_DEALLOCATE(&scratch, path);
        hatch_pattern = upload_bitmap(&bitmap);
        stbi_image_free(bitmap.pixels);

        glUseProgram(shader_screen_pattern.program);
        glUniform2f(shader_screen_pattern.texture_dimensions, bitmap.width, bitmap.height);
    }

    // Line pattern texture
    {
        char* path = get_image_path_by_name("Dot.png", &scratch);
        Bitmap bitmap;
        bitmap.pixels = stbi_load(path, &bitmap.width, &bitmap.height, &bitmap.bytes_per_pixel, STBI_default);
        STACK_DEALLOCATE(&scratch, path);
        line_pattern = upload_bitmap(&bitmap);
        stbi_image_free(bitmap.pixels);

        glUseProgram(shader_line.program);
        glUniform2f(shader_line.texture_dimensions, bitmap.width, bitmap.height);
    }

    immediate::context_create(&heap);
    immediate::set_shader(shader_vertex_colour.program);
    immediate::set_line_shader(shader_line.program);
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
        glDeleteProgram(shader_line.program);

        for(int i = 0; i < 1; i += 1)
        {
            glDeleteTextures(1, &font_textures[i]);
        }
        glDeleteTextures(1, &hatch_pattern);
        glDeleteTextures(1, &line_pattern);

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

    if(is_valid_index(tool->selected_axis))
    {
        draw_arrow(reference, tool->position, shaft_radius, head_height, head_radius);
    }
    else if(is_valid_index(tool->selected_plane))
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
    ASSERT(index >= 0 && index < ARRAY_COUNT(lady->objects));

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

static void draw_selection_object(Object object, Object wireframe, Matrix4 projection)
{
    const Vector4 colour = {1.0f, 0.5f, 0.0f, 0.8f};

    // Draw selected faces.
    glUseProgram(shader_halo.program);

    glUniform4fv(shader_halo.halo_colour, 1, &colour[0]);

    glDepthFunc(GL_EQUAL);
    glDepthMask(GL_FALSE);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUniformMatrix4fv(shader_halo.model_view_projection, 1, GL_TRUE, object.model_view_projection.elements);
    glBindVertexArray(object.vertex_array);
    glDrawElements(GL_TRIANGLES, object.indices_count, GL_UNSIGNED_SHORT, nullptr);

    // Draw the wireframe.
    glUseProgram(shader_line.program);

    glDepthFunc(GL_LEQUAL);

    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(-1.0f, -1.0f);

    glActiveTexture(GL_TEXTURE0 + 0);
    glBindTexture(GL_TEXTURE_2D, line_pattern);
    glBindSampler(0, linear_repeat);

    glUniformMatrix4fv(shader_line.model_view_projection, 1, GL_TRUE, wireframe.model_view_projection.elements);
    glUniform1f(shader_line.projection_factor, projection[0]);
    glBindVertexArray(wireframe.vertex_array);
    glDrawElements(GL_TRIANGLES, wireframe.indices_count, GL_UNSIGNED_SHORT, nullptr);

    // Reset to defaults.
    glDisable(GL_POLYGON_OFFSET_FILL);
    glDisable(GL_BLEND);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
}

void system_update(UpdateState* update, Platform* platform)
{
    Camera* camera = update->camera;
    Int2 viewport = update->viewport;
    MoveTool* move_tool = update->move_tool;
    ui::Context* ui_context = update->ui_context;
    ui::Item* main_menu = update->main_menu;
    ui::Item* dialog_panel = update->dialog_panel;
    bool dialog_enabled = update->dialog_enabled;
    ObjectLady* lady = update->lady;
    int hovered_object_index = update->hovered_object_index;
    int selected_object_index = update->selected_object_index;
    DenseMapId selection_id = update->selection_id;
    DenseMapId selection_wireframe_id = update->selection_wireframe_id;

    Matrix4 projection;

    // Update matrices.
    {
        projection = perspective_projection_matrix(camera->field_of_view, viewport.x, viewport.y, camera->near_plane, camera->far_plane);

        Vector3 direction = normalise(camera->target - camera->position);
        Matrix4 view = look_at_matrix(vector3_zero, direction, vector3_unit_z);
        object_set_matrices(&sky, view, sky_projection);

        view = look_at_matrix(camera->position, camera->target, vector3_unit_z);

        FOR_ALL(objects.array)
        {
            object_set_matrices(it, view, projection);
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
    for(int i = 0; i < ARRAY_COUNT(lady->objects); i += 1)
    {
        if(i == hovered_object_index || i == selected_object_index)
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
    if(is_valid_index(selected_object_index))
    {
        draw_object_with_halo(lady, selected_object_index, vector4_white);
    }
    if(is_valid_index(hovered_object_index) && hovered_object_index != selected_object_index)
    {
        draw_object_with_halo(lady, hovered_object_index, vector4_yellow);
    }

    // Draw the selection itself.
    if(selection_id)
    {
        Object object = *get_object(selection_id);
        Object wireframe = *get_object(selection_wireframe_id);
        draw_selection_object(object, wireframe, projection);
    }

    glUseProgram(shader_lit.program);

    // rotate tool
    {
        Matrix4 view = look_at_matrix(camera->position, camera->target, vector3_unit_z);
        immediate::set_matrices(view, projection);
        draw_rotate_tool(false);
    }

    // move tool
    if(is_valid_index(selected_object_index))
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

        int corner_x = viewport.x - scale - padding;
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
    glViewport(0, 0, viewport.x, viewport.y);
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
        ui::lay_out(dialog_panel, space, ui_context);
        ui::draw(dialog_panel, ui_context);

        ui::draw_focus_indicator(dialog_panel, ui_context);
    }

    // Draw the main menu.
    {
        glActiveTexture(GL_TEXTURE0 + 0);
        glBindTexture(GL_TEXTURE_2D, font_textures[0]);
        glBindSampler(0, nearest_repeat);

        Rect space;
        space.bottom_left = {-viewport.x / 2.0f, viewport.y / 2.0f};
        space.dimensions.x = viewport.x;
        space.dimensions.y = 60.0f;
        space.bottom_left.y -= space.dimensions.y;
        ui::lay_out(main_menu, space, ui_context);
        ui::draw(main_menu, ui_context);

        ui::draw_focus_indicator(main_menu, ui_context);
    }

    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
}

void resize_viewport(Int2 dimensions, double dots_per_millimeter, float fov)
{
    int width = dimensions.x;
    int height = dimensions.y;
    sky_projection = perspective_projection_matrix(fov, width, height, 0.001f, 1.0f);
    screen_projection = orthographic_projection_matrix(width, height, -1.0f, 1.0f);

    // Update any shaders that use the viewport dimensions.
    glUseProgram(shader_screen_pattern.program);
    glUniform2f(shader_screen_pattern.viewport_dimensions, width, height);

    glUseProgram(shader_line.program);
    glUniform2f(shader_line.viewport_dimensions, width, height);
}

DenseMapId add_object(VertexLayout vertex_layout)
{
    DenseMapId id = add(&objects, &heap);
    Object* object = look_up(&objects, id);
    object_create(object, vertex_layout);
    return id;
}

void remove_object(DenseMapId id)
{
    Object* object = look_up(&objects, id);
    object_destroy(object);
    remove(&objects, id, &heap);
}

Object* get_object(DenseMapId id)
{
    return look_up(&objects, id);
}

void set_up_font(bmfont::Font* font)
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

} // namespace video
