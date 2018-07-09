#include "video.h"

#include "assert.h"
#include "asset_paths.h"
#include "array2.h"
#include "bmfont.h"
#include "bmp.h"
#include "closest_point_of_approach.h"
#include "colours.h"
#include "filesystem.h"
#include "float_utilities.h"
#include "history.h"
#include "immediate.h"
#include "input.h"
#include "int_utilities.h"
#include "intersection.h"
#include "jan.h"
#include "log.h"
#include "logging.h"
#include "map.h"
#include "math_basics.h"
#include "obj.h"
#include "object_lady.h"
#include "shader.h"
#include "sorting.h"
#include "string_utilities.h"
#include "string_build.h"
#include "uniform_blocks.h"
#include "vector_math.h"
#include "vertex_layout.h"
#include "video_object.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

static PixelFormat get_pixel_format(int bytes_per_pixel)
{
    switch(bytes_per_pixel)
    {
        case 1:  return PIXEL_FORMAT_R8;
        case 2:  return PIXEL_FORMAT_RG8;
        case 3:  return PIXEL_FORMAT_RGB8;
        case 4:  return PIXEL_FORMAT_RGBA8;
        default: return PIXEL_FORMAT_INVALID;
    }
}

static ImageId build_image(Backend* backend, Log* logger, const char* name, Stack* stack, bool with_mipmaps, Int2* dimensions)
{
    char* path = get_image_path_by_name(name, stack);
    int width;
    int height;
    int bytes_per_pixel;
    void* pixels = stbi_load(path, &width, &height, &bytes_per_pixel, STBI_default);
    STACK_DEALLOCATE(stack, path);

    ImageId id = {0};

    if(pixels)
    {
        int bitmap_size = bytes_per_pixel * width * height;

        int mip_levels = 1;
        if(with_mipmaps)
        {
            mip_levels = count_mip_levels(width, height);
        }

        ImageSpec spec =
        {
            .content =
            {
                .subimages[0][0] =
                {
                    .content = pixels,
                    .size = bitmap_size,
                },
            },
            .height = height,
            .mipmap_count = mip_levels,
            .pixel_format = get_pixel_format(bytes_per_pixel),
            .type = IMAGE_TYPE_2D,
            .width = width,
        };
        id = create_image(backend, &spec, logger);

        if(dimensions)
        {
            dimensions->x = width;
            dimensions->y = height;
        }

        stbi_image_free(pixels);
    }

    return id;
}

static SamplerId nearest_repeat;
static SamplerId linear_repeat;
static SamplerId linear_mipmap_repeat;

static ShaderId shader_font;
static ShaderId shader_halo;
static ShaderId shader_line;
static ShaderId shader_lit;
static ShaderId shader_point;
static ShaderId shader_screen_pattern;
static ShaderId shader_texture_only;
static ShaderId shader_vertex_colour;

static Backend* backend;
static Log logger;
static BufferId uniform_buffers[5];
static BufferId halo_block_buffer;
static BufferId light_block_buffer;
static BufferId per_point_buffer;
static PerPass per_pass_block;

static PipelineId pipeline_background;
static PipelineId pipeline_unselected;
static PipelineId pipeline_face_selection;
static PipelineId pipeline_pointcloud;
static PipelineId pipeline_wireframe;

static VideoObject sky;
static DenseMap objects;

static Matrix4 sky_projection;
static Matrix4 screen_projection;
static Stack scratch;
static Heap heap;

static ImageId font_textures[1];
static ImageId hatch_pattern;
static ImageId line_pattern;
static ImageId point_pattern;

static ShaderId build_shader(ShaderSpec* spec, const char* fragment_name, const char* vertex_name)
{
    char* fragment_path = get_shader_path_by_name(fragment_name, &scratch);
    char* vertex_path = get_shader_path_by_name(vertex_name, &scratch);

    ShaderId shader = {0};

    void* contents;
    uint64_t bytes;
    bool loaded_fragment = load_whole_file(fragment_path, &contents, &bytes, &scratch);
    char* fragment_source = (char*) contents;

    bool loaded_vertex = load_whole_file(vertex_path, &contents, &bytes, &scratch);
    char* vertex_source = (char*) contents;

    if(loaded_fragment && loaded_vertex)
    {
        spec->fragment.source = fragment_source;
        spec->vertex.source = vertex_source;
        shader = create_shader(backend, spec, &heap, &logger);
    }
    else if(!loaded_fragment)
    {
        log_error(&logger, "Failed to compile %s.", fragment_name);
    }
    else if(!loaded_vertex)
    {
        log_error(&logger, "Failed to compile %s.", vertex_name);
    }

    STACK_DEALLOCATE(&scratch, vertex_source);
    STACK_DEALLOCATE(&scratch, fragment_source);
    STACK_DEALLOCATE(&scratch, vertex_path);
    STACK_DEALLOCATE(&scratch, fragment_path);

    return shader;
}

bool video_system_start_up()
{
    stack_create(&scratch, (uint32_t) uptibytes(1));
    heap_create(&heap, (uint32_t) uptibytes(1));
    dense_map_create(&objects, &heap);

    backend = create_backend(&heap);

    SamplerSpec nearest_repeat_spec =
    {
        .address_mode_u = SAMPLER_ADDRESS_MODE_REPEAT,
        .address_mode_v = SAMPLER_ADDRESS_MODE_REPEAT,
        .address_mode_w = SAMPLER_ADDRESS_MODE_REPEAT,
        .magnify_filter = SAMPLER_FILTER_POINT,
        .minify_filter = SAMPLER_FILTER_POINT,
    };
    nearest_repeat = create_sampler(backend, &nearest_repeat_spec, &logger);

    SamplerSpec linear_repeat_spec =
    {
        .address_mode_u = SAMPLER_ADDRESS_MODE_REPEAT,
        .address_mode_v = SAMPLER_ADDRESS_MODE_REPEAT,
        .address_mode_w = SAMPLER_ADDRESS_MODE_REPEAT,
        .magnify_filter = SAMPLER_FILTER_LINEAR,
        .minify_filter = SAMPLER_FILTER_LINEAR,
    };
    linear_repeat = create_sampler(backend, &linear_repeat_spec, &logger);

    SamplerSpec linear_mipmap_repeat_spec =
    {
        .address_mode_u = SAMPLER_ADDRESS_MODE_REPEAT,
        .address_mode_v = SAMPLER_ADDRESS_MODE_REPEAT,
        .address_mode_w = SAMPLER_ADDRESS_MODE_REPEAT,
        .magnify_filter = SAMPLER_FILTER_LINEAR,
        .minify_filter = SAMPLER_FILTER_LINEAR,
        .mipmap_filter = SAMPLER_FILTER_LINEAR,
    };
    linear_mipmap_repeat = create_sampler(backend, &linear_mipmap_repeat_spec, &logger);

    UniformBlockSpec halo_block_spec =
    {
        .binding = 6,
        .name = "HaloBlock",
        .size = sizeof(HaloBlock),
    };

    UniformBlockSpec light_block_spec =
    {
        .binding = 5,
        .name = "LightBlock",
        .size = sizeof(LightBlock),
    };

    UniformBlockSpec per_image_block_spec =
    {
        .binding = 4,
        .name = "PerImage",
        .size = sizeof(PerImage),
    };

    UniformBlockSpec per_line_block_spec =
    {
        .binding = 1,
        .name = "PerLine",
        .size = sizeof(PerLine),
    };

    UniformBlockSpec per_object_block_spec =
    {
        .binding = 2,
        .name = "PerObject",
        .size = sizeof(PerObject),
    };

    UniformBlockSpec per_pass_block_spec =
    {
        .binding = 0,
        .name = "PerPass",
        .size = sizeof(PerPass),
    };

    UniformBlockSpec per_point_block_spec =
    {
        .binding = 7,
        .name = "PerPoint",
        .size = sizeof(PerPoint),
    };

    UniformBlockSpec per_span_block_spec =
    {
        .binding = 3,
        .name = "PerSpan",
        .size = sizeof(PerSpan),
    };

    ShaderSpec shader_font_spec =
    {
        .fragment =
        {
            .images[0] = {.name = "texture"},
            .uniform_blocks = {per_span_block_spec},
        },
        .vertex =
        {
            .uniform_blocks = {per_pass_block_spec},
        },
    };
    shader_font = build_shader(&shader_font_spec, "Font.fs", "Font.vs");

    ShaderSpec halo_spec =
    {
        .fragment =
        {
            .uniform_blocks[0] = halo_block_spec,
        },
        .vertex =
        {
            .uniform_blocks =
            {
                per_pass_block_spec,
                per_object_block_spec,
            },
        },
    };
    shader_halo = build_shader(&halo_spec, "Halo.fs", "Halo.vs");

    ShaderSpec shader_line_spec =
    {
        .fragment =
        {
            .images[0] = {.name = "texture"},
        },
        .vertex =
        {
            .uniform_blocks =
            {
                per_pass_block_spec,
                per_line_block_spec,
                per_image_block_spec,
                per_object_block_spec,
            },
        },
    };
    shader_line = build_shader(&shader_line_spec, "Line.fs", "Line.vs");

    ShaderSpec lit_spec =
    {
        .fragment =
        {
            .uniform_blocks[0] = light_block_spec,
        },
        .vertex =
        {
            .uniform_blocks =
            {
                per_pass_block_spec,
                per_object_block_spec,
            },
        },
    };
    shader_lit = build_shader(&lit_spec, "Lit.fs", "Lit.vs");

    ShaderSpec shader_point_spec =
    {
        .fragment =
        {
            .images[0] = {.name = "texture"},
        },
        .vertex =
        {
            .uniform_blocks =
            {
                per_object_block_spec,
                per_pass_block_spec,
                per_point_block_spec,
            },
        },
    };
    shader_point = build_shader(&shader_point_spec, "Point.fs", "Point.vs");

    ShaderSpec shader_screen_pattern_spec =
    {
        .fragment =
        {
            .images =
            {
                [0] = {.name = "texture"},
            },
        },
        .vertex =
        {
            .uniform_blocks =
            {
                per_pass_block_spec,
                per_image_block_spec,
            },
        },
    };
    shader_screen_pattern = build_shader(&shader_screen_pattern_spec, "Screen Pattern.fs", "Screen Pattern.vs");

    ShaderSpec texture_only_spec =
    {
        .fragment =
        {
            .images[0] =
            {
                .name = "texture",
            },
        },
        .vertex =
        {
            .uniform_blocks[0] = per_pass_block_spec,
        },
    };
    shader_texture_only = build_shader(&texture_only_spec, "Texture Only.fs", "Texture Only.vs");

    ShaderSpec shader_vertex_colour_spec =
    {
        .vertex =
        {
            .uniform_blocks =
            {
                per_pass_block_spec,
                per_object_block_spec,
            },
        },
    };
    shader_vertex_colour = build_shader(&shader_vertex_colour_spec, "Vertex Colour.fs", "Vertex Colour.vs");

    BufferSpec per_image_spec =
    {
        .binding = 4,
        .size = sizeof(PerImage),
        .format = BUFFER_FORMAT_UNIFORM,
        .usage = BUFFER_USAGE_DYNAMIC,
    };
    uniform_buffers[0] = create_buffer(backend, &per_image_spec, &logger);

    BufferSpec per_line_spec =
    {
        .binding = 1,
        .size = sizeof(PerLine),
        .format = BUFFER_FORMAT_UNIFORM,
        .usage = BUFFER_USAGE_DYNAMIC,
    };
    uniform_buffers[1] = create_buffer(backend, &per_line_spec, &logger);

    BufferSpec per_object_spec =
    {
        .binding = 2,
        .size = sizeof(PerObject),
        .format = BUFFER_FORMAT_UNIFORM,
        .usage = BUFFER_USAGE_DYNAMIC,
    };
    uniform_buffers[2] = create_buffer(backend, &per_object_spec, &logger);

    BufferSpec per_pass_spec =
    {
        .binding = 0,
        .size = sizeof(PerPass),
        .format = BUFFER_FORMAT_UNIFORM,
        .usage = BUFFER_USAGE_DYNAMIC,
    };
    uniform_buffers[3] = create_buffer(backend, &per_pass_spec, &logger);

    BufferSpec per_span_spec =
    {
        .binding = 3,
        .size = sizeof(PerSpan),
        .format = BUFFER_FORMAT_UNIFORM,
        .usage = BUFFER_USAGE_DYNAMIC,
    };
    uniform_buffers[4] = create_buffer(backend, &per_span_spec, &logger);

    BufferSpec halo_block_buffer_spec =
    {
        .binding = 6,
        .size = sizeof(HaloBlock),
        .format = BUFFER_FORMAT_UNIFORM,
        .usage = BUFFER_USAGE_DYNAMIC,
    };
    halo_block_buffer = create_buffer(backend, &halo_block_buffer_spec, &logger);

    BufferSpec per_point_spec =
    {
        .binding = 7,
        .size = sizeof(PerPoint),
        .format = BUFFER_FORMAT_UNIFORM,
        .usage = BUFFER_USAGE_DYNAMIC,
    };
    per_point_buffer = create_buffer(backend, &per_point_spec, &logger);

    BufferSpec light_block_buffer_spec =
    {
        .binding = 5,
        .size = sizeof(LightBlock),
        .format = BUFFER_FORMAT_UNIFORM,
        .usage = BUFFER_USAGE_DYNAMIC,
    };
    light_block_buffer = create_buffer(backend, &light_block_buffer_spec, &logger);

    // Sky
    video_object_create(&sky, VERTEX_LAYOUT_PC);
    video_object_generate_sky(&sky, backend, &logger, &scratch);

    hatch_pattern = build_image(backend, &logger, "polka_dot.png", &scratch, false, NULL);
    point_pattern = build_image(backend, &logger, "Point.png", &scratch, true, NULL);

    // Line pattern texture
    {
        Int2 dimensions;
        line_pattern = build_image(backend, &logger, "Line Feathering.png", &scratch, true, &dimensions);

        PerImage per_image =
        {
            .texture_dimensions = {{(float) dimensions.x, (float) dimensions.y}},
        };
        update_buffer(backend, uniform_buffers[0], &per_image, 0, sizeof(PerImage));
    }

#if 0
    StencilOpStateSpec selected_stencil_spec =
    {
        .compare_mask = 0xff,
        .compare_op = COMPARE_OP_ALWAYS,
        .depth_fail_op = STENCIL_OP_KEEP,
        .fail_op = STENCIL_OP_KEEP,
        .pass_op = STENCIL_OP_REPLACE,
        .reference = 1,
        .write_mask = 0xff,
    };

    StencilOpStateSpec halo_stencil_spec =
    {
        .compare_mask = 0xff,
        .compare_op = COMPARE_OP_NOT_EQUAL,
        .depth_fail_op = STENCIL_OP_KEEP,
        .fail_op = STENCIL_OP_KEEP,
        .pass_op = STENCIL_OP_REPLACE,
        .reference = 1,
        .write_mask = 0x00,
    };

    DepthStencilStateSpec selected_depth_stencil_spec =
    {
        .depth_compare_enabled = true,
        .depth_write_enabled = true,
        .stencil_enabled = true,
        .back_stencil = selected_stencil_spec,
        .front_stencil = selected_stencil_spec,
    };
#endif

    BlendStateSpec alpha_blend_spec =
    {
        .enabled = true,
        .alpha_op = BLEND_OP_ADD,
        .alpha_source_factor = BLEND_FACTOR_SRC_ALPHA,
        .alpha_destination_factor = BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .rgb_op = BLEND_OP_ADD,
        .rgb_source_factor = BLEND_FACTOR_SRC_ALPHA,
        .rgb_destination_factor = BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
    };

    DepthStencilStateSpec transparent_depth_stencil_spec =
    {
        .depth_compare_enabled = true,
    };

    VertexLayoutSpec vertex_layout_line_spec =
    {
        .attributes =
        {
            [0] = {.format = VERTEX_FORMAT_FLOAT3},
            [1] = {.format = VERTEX_FORMAT_FLOAT3},
            [2] = {.format = VERTEX_FORMAT_UBYTE4_NORM},
            [3] = {.format = VERTEX_FORMAT_USHORT2_NORM},
            [4] = {.format = VERTEX_FORMAT_FLOAT1},
        },
    };

    VertexLayoutSpec vertex_layout_point_spec =
    {
        .attributes =
        {
            [0] = {.format = VERTEX_FORMAT_FLOAT3},
            [1] = {.format = VERTEX_FORMAT_FLOAT2},
            [2] = {.format = VERTEX_FORMAT_UBYTE4_NORM},
            [3] = {.format = VERTEX_FORMAT_USHORT2_NORM},
        },
    };

    VertexLayoutSpec vertex_layout_spec_lit =
    {
        .attributes =
        {
            [0] = {.format = VERTEX_FORMAT_FLOAT3},
            [1] = {.format = VERTEX_FORMAT_FLOAT3},
            [2] = {.format = VERTEX_FORMAT_UBYTE4_NORM},
        },
    };

    VertexLayoutSpec vertex_layout_spec_vertex_colour =
    {
        .attributes =
        {
            [0] = {.format = VERTEX_FORMAT_FLOAT3},
            [1] = {.format = VERTEX_FORMAT_UBYTE4_NORM},
        },
    };

    PipelineSpec pipeline_background_spec =
    {
        .depth_stencil =
        {
            .depth_compare_enabled = true,
            .depth_compare_op = COMPARE_OP_EQUAL,
        },
        .shader = shader_vertex_colour,
        .vertex_layout = vertex_layout_spec_vertex_colour,
    };
    pipeline_background = create_pipeline(backend, &pipeline_background_spec, &logger);

    PipelineSpec pipeline_unselected_spec =
    {
        .depth_stencil =
        {
            .depth_compare_enabled = true,
            .depth_write_enabled = true,
        },
        .shader = shader_lit,
        .vertex_layout = vertex_layout_spec_lit,
    };
    pipeline_unselected = create_pipeline(backend, &pipeline_unselected_spec, &logger);

    PipelineSpec pipeline_face_selection_spec =
    {
        .blend = alpha_blend_spec,
        .depth_stencil =
        {
            .depth_compare_enabled = true,
            .depth_compare_op = COMPARE_OP_LESS_OR_EQUAL,
            .depth_write_enabled = false,
        },
        .shader = shader_halo,
        .vertex_layout = vertex_layout_spec_lit,
    };
    pipeline_face_selection = create_pipeline(backend, &pipeline_face_selection_spec, &logger);

    PipelineSpec pipeline_pointcloud_spec =
    {
        .blend = alpha_blend_spec,
        .depth_stencil = transparent_depth_stencil_spec,
        .shader = shader_point,
        .vertex_layout = vertex_layout_point_spec,
    };
    pipeline_pointcloud = create_pipeline(backend, &pipeline_pointcloud_spec, &logger);

    PipelineSpec pipeline_wireframe_spec =
    {
        .blend = alpha_blend_spec,
        .depth_stencil = transparent_depth_stencil_spec,
        .shader = shader_line,
        .vertex_layout = vertex_layout_line_spec,
    };
    pipeline_wireframe = create_pipeline(backend, &pipeline_wireframe_spec, &logger);

    ImmediateContextSpec immediate_spec =
    {
        .backend = backend,
        .shaders =
        {
            shader_vertex_colour,
            shader_font,
            shader_line,
        },
        .uniform_buffers =
        {
            uniform_buffers[1],
            uniform_buffers[4],
        },
    };
    immediate_context_create(&immediate_spec, &heap, &logger);

    return true;
}

void video_system_shut_down(bool functions_loaded)
{
    if(functions_loaded)
    {
        destroy_sampler(backend, nearest_repeat);
        destroy_sampler(backend, linear_repeat);
        destroy_sampler(backend, linear_mipmap_repeat);

        destroy_shader(backend, shader_font);
        destroy_shader(backend, shader_halo);
        destroy_shader(backend, shader_line);
        destroy_shader(backend, shader_lit);
        destroy_shader(backend, shader_point);
        destroy_shader(backend, shader_screen_pattern);
        destroy_shader(backend, shader_texture_only);
        destroy_shader(backend, shader_vertex_colour);

        for(int i = 0; i < 5; i += 1)
        {
            destroy_buffer(backend, uniform_buffers[i]);
        }
        destroy_buffer(backend, halo_block_buffer);
        destroy_buffer(backend, light_block_buffer);
        destroy_buffer(backend, per_point_buffer);

        destroy_image(backend, hatch_pattern);
        destroy_image(backend, line_pattern);
        destroy_image(backend, point_pattern);

        for(int i = 0; i < 1; i += 1)
        {
            destroy_image(backend, font_textures[i]);
        }

        video_object_destroy(&sky, backend);

        destroy_pipeline(backend, pipeline_background);
        destroy_pipeline(backend, pipeline_unselected);
        destroy_pipeline(backend, pipeline_face_selection);
        destroy_pipeline(backend, pipeline_pointcloud);
        destroy_pipeline(backend, pipeline_wireframe);

        immediate_context_destroy(&heap);

        destroy_backend(backend, &heap);
    }

    dense_map_destroy(&objects, &heap);

    stack_destroy(&scratch);
    heap_destroy(&heap);
}

static void set_model_matrix(Matrix4 model)
{
    PerObject per_object =
    {
        .model = matrix4_transpose(model),
    };
    update_buffer(backend, uniform_buffers[2], &per_object, 0, sizeof(per_object));
}

static void set_view_projection(Matrix4 view, Matrix4 projection)
{
    per_pass_block.view_projection = matrix4_transpose(matrix4_multiply(projection, view));
    update_buffer(backend, uniform_buffers[3], &per_pass_block, 0, sizeof(per_pass_block));

    immediate_set_matrices(view, projection);
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

#if 0

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

#endif

static void apply_object_block(VideoObject* object)
{
    PerObject per_object =
    {
        .model = matrix4_transpose(object->model),
        .normal_matrix = matrix4_transpose(object->normal),
    };
    update_buffer(backend, uniform_buffers[2], &per_object, 0, sizeof(per_object));
}

static void draw_object(VideoObject* object)
{
    DrawAction draw_action =
    {
        .index_buffer = object->buffers[1],
        .indices_count = object->indices_count,
        .vertex_buffers[0] = object->buffers[0],
    };
    draw(backend, &draw_action);
}

static void draw_selection_object(VideoObject* object, VideoObject* pointcloud, VideoObject* wireframe, Matrix4 projection)
{
    const Float4 colour = (Float4){{1.0f, 0.5f, 0.0f, 0.8f}};

    // Draw selected faces.
    if(object && object->indices_count)
    {
        set_pipeline(backend, pipeline_face_selection);

        HaloBlock halo_block =
        {
            .halo_colour = colour,
        };
        update_buffer(backend, halo_block_buffer, &halo_block, 0, sizeof(halo_block));

        apply_object_block(object);
        draw_object(object);
    }

    if(wireframe && wireframe->indices_count)
    {
        set_pipeline(backend, pipeline_wireframe);

        ImageSet image_set =
        {
            .stages[1] =
            {
                .images[0] = line_pattern,
                .samplers[0] = linear_mipmap_repeat,
            },
        };
        set_images(backend, &image_set);

        PerLine line_block =
        {
            .line_width = 4.0f,
            .projection_factor = projection.e[0],
        };
        update_buffer(backend, per_point_buffer, &line_block, 0, sizeof(line_block));

        apply_object_block(wireframe);
        draw_object(wireframe);
    }

    if(pointcloud && pointcloud->indices_count)
    {
        set_pipeline(backend, pipeline_pointcloud);

        ImageSet image_set =
        {
            .stages[1] =
            {
                .images[0] = point_pattern,
                .samplers[0] = linear_mipmap_repeat,
            },
        };
        set_images(backend, &image_set);

        PerLine point_block =
        {
            .line_width = 3.0f,
            .projection_factor = projection.e[0],
        };
        update_buffer(backend, per_point_buffer, &point_block, 0, sizeof(point_block));

        apply_object_block(pointcloud);
        draw_object(pointcloud);
    }
}

void video_system_update(VideoUpdate* update, Platform* platform)
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

    per_pass_block.viewport_dimensions = (Float2){{(float) viewport.x, (float) viewport.y}};

    Matrix4 projection;
    Matrix4 sky_view;
    Matrix4 view;

    // Update matrices.
    {
        projection = matrix4_perspective_projection(camera->field_of_view, (float) viewport.x, (float) viewport.y, camera->near_plane, camera->far_plane);

        Float3 direction = float3_normalise(float3_subtract(camera->target, camera->position));
        sky_view = matrix4_look_at(float3_zero, direction, float3_unit_z);
        video_object_set_matrices(&sky, sky_view, sky_projection);

        view = matrix4_look_at(camera->position, camera->target, float3_unit_z);

        FOR_ALL(VideoObject, objects.array)
        {
            video_object_set_matrices(it, view, projection);
        }

        set_view_projection(view, projection);

        // Update light parameters.

        Float3 light_direction = (Float3){{0.7f, 0.4f, -1.0f}};
        light_direction = float3_normalise(float3_negate(matrix4_transform_vector(view, light_direction)));

        LightBlock light_block = {light_direction};
        update_buffer(backend, light_block_buffer, &light_block, 0, sizeof(light_block));
    }

    set_pipeline(backend, pipeline_unselected);

    ClearState clear_state =
    {
        .depth = 1.0f,
        .flags =
        {
            .colour = true,
            .depth = true,
            .stencil = true,
        },
        .stencil = 0xffffffff,
    };
    clear_target(backend, &clear_state);

    // Draw all unselected models.
    for(int i = 0; i < array_count(lady->objects); i += 1)
    {
#if 0
        if(i == hovered_object_index || i == selected_object_index)
        {
            continue;
        }
#endif
        VideoObject* object = dense_map_look_up(&objects, lady->objects[i].video_object);
        apply_object_block(object);
        draw_object(object);
    }

#if 0
    // Draw the selected and hovered models.
    if(is_valid_index(selected_object_index))
    {
        draw_object_with_halo(lady, selected_object_index, float4_white);
    }
    if(is_valid_index(hovered_object_index) && hovered_object_index != selected_object_index)
    {
        draw_object_with_halo(lady, hovered_object_index, float4_yellow);
    }
#endif

    set_model_matrix(matrix4_identity);
    draw_rotate_tool(false);

    // move tool
    if(is_valid_index(selected_object_index))
    {
        Matrix4 model = matrix4_compose_transform(move_tool->position, move_tool->orientation, float3_set_all(move_tool->scale));

#if 0
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
#endif
        // draw solid parts on top of silhouette
        set_model_matrix(model);
        draw_move_tool(move_tool, false);

        set_model_matrix(matrix4_identity);
        draw_move_tool_vectors(move_tool);
    }

    // Draw the sky behind everything else.
    {
        Viewport viewport_state =
        {
            .bottom_left = {0, 0},
            .dimensions = viewport,
            .far_depth = 1.0f,
            .near_depth = 1.0f,
        };
        set_viewport(backend, &viewport_state);
        set_view_projection(sky_view, sky_projection);

        set_pipeline(backend, pipeline_background);
        apply_object_block(&sky);
        draw_object(&sky);

        Viewport reset_viewport_state =
        {
            .bottom_left = {0, 0},
            .dimensions = viewport,
            .far_depth = 1.0f,
            .near_depth = 0.0f,
        };
        set_viewport(backend, &reset_viewport_state);
        set_view_projection(view, projection);
    }

    // Draw the selection itself.
    if(selection_id || selection_pointcloud_id || selection_wireframe_id)
    {
        VideoObject* object = NULL;
        if(selection_id)
        {
            object = dense_map_look_up(&objects, selection_id);
        }

        VideoObject* pointcloud = NULL;
        if(selection_pointcloud_id)
        {
            pointcloud = dense_map_look_up(&objects, selection_pointcloud_id);
        }

        VideoObject* wireframe = NULL;
        if(selection_wireframe_id)
        {
            wireframe = dense_map_look_up(&objects, selection_wireframe_id);
        }

        draw_selection_object(object, pointcloud, wireframe, projection);
    }

    // Draw rotatory boys
    {
        ImageSet image_set =
        {
            .stages[1] =
            {
                .images[0] = line_pattern,
                .samplers[0] = linear_mipmap_repeat,
            },
        };
        set_images(backend, &image_set);

        const Float4 x_axis_colour = (Float4){{1.0f, 0.0314f, 0.0314f, 1.0f}};
        const Float4 y_axis_colour = (Float4){{0.3569f, 1.0f, 0.0f, 1.0f}};
        const Float4 z_axis_colour = (Float4){{0.0863f, 0.0314f, 1.0f, 1.0f}};

        Float3 center = rotate_tool->position;
        float radius = rotate_tool->radius;

        immediate_set_line_width(8.0f);
        immediate_add_wire_arc(center, float3_unit_x, pi, rotate_tool->angles[0], radius, x_axis_colour);
        immediate_add_wire_arc(center, float3_unit_y, pi, rotate_tool->angles[1], radius, y_axis_colour);
        immediate_add_wire_arc(center, float3_unit_z, pi, rotate_tool->angles[2], radius, z_axis_colour);
        immediate_set_blend_mode(BLEND_MODE_TRANSPARENT);
        immediate_draw();
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

        Viewport viewport_state =
        {
            .bottom_left = {corner_x, corner_y},
            .dimensions = {scale, scale},
            .far_depth = 1.0f,
            .near_depth = 0.0f,
        };
        set_viewport(backend, &viewport_state);

        ClearState clear_state =
        {
            .depth = 1.0f,
            .flags = {.depth = true},
        };
        clear_target(backend, &clear_state);

        const float across = 3.0f * sqrtf(3.0f);
        const float extent = across / 2.0f;

        Float3 direction = float3_normalise(float3_subtract(camera->target, camera->position));
        Matrix4 view = matrix4_look_at(float3_zero, direction, float3_unit_z);
        Matrix4 axis_projection = matrix4_orthographic_projection(across, across, -extent, extent);
        set_view_projection(view, axis_projection);

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
    Viewport viewport_state =
    {
        .bottom_left = {0, 0},
        .dimensions = viewport,
        .far_depth = 1.0f,
        .near_depth = 0.0f,
    };
    set_viewport(backend, &viewport_state);

    Matrix4 screen_view = matrix4_identity;
    set_view_projection(screen_view, screen_projection);

    // Draw the open file dialog.
    if(dialog_enabled)
    {
        ImageSet image_set =
        {
            .stages[1] =
            {
                .images[0] = font_textures[0],
                .samplers[0] = nearest_repeat,
            },
        };
        set_images(backend, &image_set);

        Rect space;
        space.bottom_left = (Float2){{0.0f, -250.0f}};
        space.dimensions = (Float2){{300.0f, 500.0f}};
        ui_lay_out(dialog_panel, space, ui_context);
        ui_draw(dialog_panel, ui_context);

        ui_draw_focus_indicator(dialog_panel, ui_context);
    }

    // Draw the main menu.
    {
        ImageSet image_set =
        {
            .stages[1] =
            {
                .images[0] = font_textures[0],
                .samplers[0] = nearest_repeat,
            },
        };
        set_images(backend, &image_set);

        Rect space;
        space.bottom_left = (Float2){{-viewport.x / 2.0f, viewport.y / 2.0f}};
        space.dimensions.x = (float) viewport.x;
        space.dimensions.y = 60.0f;
        space.bottom_left.y -= space.dimensions.y;
        ui_lay_out(main_menu, space, ui_context);
        ui_draw(main_menu, ui_context);

        ui_draw_focus_indicator(main_menu, ui_context);
    }
}

void video_resize_viewport(Int2 dimensions, double dots_per_millimeter, float fov)
{
    float width = (float) dimensions.x;
    float height = (float) dimensions.y;
    sky_projection = matrix4_perspective_projection(fov, width, height, 0.001f, 1.0f);
    screen_projection = matrix4_orthographic_projection(width, height, -1.0f, 1.0f);
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
    video_object_destroy(object, backend);
    dense_map_remove(&objects, id, &heap);
}

void video_set_up_font(BmfFont* font)
{
    for(int i = 0; i < font->pages_count; i += 1)
    {
        char* filename = font->pages[i].bitmap_filename;
        font_textures[i] = build_image(backend, &logger, filename, &scratch, false, NULL);
    }
}

void video_set_model(DenseMapId id, Matrix4 model)
{
    VideoObject* object = dense_map_look_up(&objects, id);
    video_object_set_model(object, model);
}

void video_update_mesh(DenseMapId id, JanMesh* mesh, Heap* heap)
{
    VideoObject* object = dense_map_look_up(&objects, id);
    video_object_update_mesh(object, mesh, backend, &logger, heap);
}

void video_update_wireframe(DenseMapId id, JanMesh* mesh, Heap* heap)
{
    VideoObject* object = dense_map_look_up(&objects, id);
    video_object_update_wireframe(object, mesh, backend, &logger, heap);
}

void video_update_selection(DenseMapId id, JanMesh* mesh, JanSelection* selection, Heap* heap)
{
    VideoObject* object = dense_map_look_up(&objects, id);
    video_object_update_selection(object, mesh, selection, backend, &logger, heap);
}

void video_update_pointcloud_selection(DenseMapId id, JanMesh* mesh, JanSelection* selection, JanVertex* hovered, Heap* heap)
{
    VideoObject* object = dense_map_look_up(&objects, id);
    video_object_update_pointcloud_selection(object, mesh, selection, hovered, backend, &logger, heap);
}

void video_update_wireframe_selection(DenseMapId id, JanMesh* mesh, JanSelection* selection, JanEdge* hovered, Heap* heap)
{
    VideoObject* object = dense_map_look_up(&objects, id);
    video_object_update_wireframe_selection(object, mesh, selection, hovered, backend, &logger, heap);
}

int get_vertex_layout_size(VertexLayout vertex_layout)
{
    switch(vertex_layout)
    {
        case VERTEX_LAYOUT_PC:    return sizeof(VertexPC);
        case VERTEX_LAYOUT_PNC:   return sizeof(VertexPNC);
        case VERTEX_LAYOUT_LINE:  return sizeof(LineVertex);
        case VERTEX_LAYOUT_POINT: return sizeof(PointVertex);
        default:                  return 0;
    }
}
