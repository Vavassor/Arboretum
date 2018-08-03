#include "video.h"

#include "assert.h"
#include "asset_paths.h"
#include "array2.h"
#include "bitmap.h"
#include "bmfont.h"
#include "bmp.h"
#include "closest_point_of_approach.h"
#include "colours.h"
#include "debug_draw.h"
#include "debug_readout.h"
#include "filesystem.h"
#include "float_utilities.h"
#include "history.h"
#include "immediate.h"
#include "input.h"
#include "int_utilities.h"
#include "intersection.h"
#include "jan.h"
#include "log.h"
#include "map.h"
#include "math_basics.h"
#include "obj.h"
#include "object_lady.h"
#include "sorting.h"
#include "string_utilities.h"
#include "string_build.h"
#include "uniform_blocks.h"
#include "vector_extras.h"
#include "vector_math.h"
#include "vertex_layout.h"
#include "video_object.h"
#include "video_ui.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

typedef struct Buffers
{
    BufferId frame_triangle;
} Buffers;

typedef struct Images
{
    ImageId frame_colour;
    ImageId frame_depth;
    ImageId frame_velocity;
    ImageId font_textures[1];
    ImageId hatch_pattern;
    ImageId line_pattern;
    ImageId point_pattern;
    Int2 hatch_pattern_dimensions;
    Int2 line_pattern_dimensions;
} Images;

typedef struct Matrices
{
    Matrix4 view;
    Matrix4 projection;
    Matrix4 sky_view;
    Matrix4 sky_projection;
    Matrix4 screen_projection;
} Matrices;

typedef struct Passes
{
    PassId base;
} Passes;

typedef struct Pipelines
{
    PipelineId background;
    PipelineId face_selection;
    PipelineId halo;
    PipelineId hidden;
    PipelineId pointcloud;
    PipelineId resolve;
    PipelineId selected;
    PipelineId unselected;
    PipelineId wireframe;
} Pipelines;

typedef struct Samplers
{
    SamplerId nearest_repeat;
    SamplerId linear_repeat;
    SamplerId linear_mipmap_repeat;
} Samplers;

typedef struct Shaders
{
    ShaderId face_selection;
    ShaderId font;
    ShaderId halo;
    ShaderId line;
    ShaderId lit;
    ShaderId point;
    ShaderId screen_pattern;
    ShaderId texture_only;
    ShaderId vertex_colour;
} Shaders;

typedef struct Uniforms
{
    BufferId buffers[5];
    BufferId halo_block;
    BufferId light_block;
    BufferId per_point;
} Uniforms;

struct VideoContext
{
    Stack scratch;
    Heap heap;
    DenseMap objects;
    Buffers buffers;
    Images images;
    Passes passes;
    Pipelines pipelines;
    Samplers samplers;
    Shaders shaders;
    Uniforms uniforms;
    VideoObject sky;
    Backend* backend;
    Log* logger;
};

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

static ImageId build_image(VideoContext* context, const char* name, bool with_mipmaps, Int2* dimensions)
{
    char* path = get_image_path_by_name(name, &context->scratch);
    Bitmap bitmap;
    bitmap.pixels = stbi_load(path, &bitmap.width, &bitmap.height, &bitmap.bytes_per_pixel, STBI_default);
    STACK_DEALLOCATE(&context->scratch, path);

    ImageId id = {0};

    if(bitmap.pixels)
    {
        ImageContent content;
        Subimage* subimage = &content.subimages[0][0];
        subimage->content = bitmap.pixels;
        subimage->size = bitmap_get_size(&bitmap);

        Bitmap* mipmaps = NULL;
        int mip_levels = 1;
        if(with_mipmaps)
        {
            mip_levels = count_mip_levels(bitmap.width, bitmap.height);

            mipmaps = generate_mipmap_array(&bitmap, &context->heap);
            for(int i = 0; i < array_count(mipmaps); i += 1)
            {
                Bitmap* mipmap = &mipmaps[i];
                subimage = &content.subimages[0][i + 1];
                subimage->content = mipmap->pixels;
                subimage->size = bitmap_get_size(mipmap);
            }
        }

        ImageSpec spec =
        {
            .content = content,
            .height = bitmap.height,
            .mipmap_count = mip_levels,
            .pixel_format = get_pixel_format(bitmap.bytes_per_pixel),
            .type = IMAGE_TYPE_2D,
            .width = bitmap.width,
        };
        id = create_image(context->backend, &spec, context->logger);

        if(dimensions)
        {
            dimensions->x = bitmap.width;
            dimensions->y = bitmap.height;
        }

        stbi_image_free(bitmap.pixels);

        if(mipmaps)
        {
            bitmap_destroy_array(mipmaps, &context->heap);
        }
    }

    return id;
}

static ShaderId build_shader(VideoContext* context, ShaderSpec* spec, const char* fragment_name, const char* vertex_name)
{
    char* fragment_path = get_shader_path_by_name(fragment_name, &context->scratch);
    char* vertex_path = get_shader_path_by_name(vertex_name, &context->scratch);

    ShaderId shader = {0};

    void* contents;
    uint64_t bytes;
    bool loaded_fragment = load_whole_file(fragment_path, &contents, &bytes, &context->scratch);
    char* fragment_source = (char*) contents;

    bool loaded_vertex = load_whole_file(vertex_path, &contents, &bytes, &context->scratch);
    char* vertex_source = (char*) contents;

    if(loaded_fragment && loaded_vertex)
    {
        spec->fragment.source = fragment_source;
        spec->vertex.source = vertex_source;
        shader = create_shader(context->backend, spec, &context->heap, context->logger);
    }
    else if(!loaded_fragment)
    {
        log_error(context->logger, "Failed to compile %s.", fragment_name);
    }
    else if(!loaded_vertex)
    {
        log_error(context->logger, "Failed to compile %s.", vertex_name);
    }

    STACK_DEALLOCATE(&context->scratch, vertex_source);
    STACK_DEALLOCATE(&context->scratch, fragment_source);
    STACK_DEALLOCATE(&context->scratch, vertex_path);
    STACK_DEALLOCATE(&context->scratch, fragment_path);

    return shader;
}

typedef struct VertexFrameTriangle
{
    Float3 position;
    Float2 texcoord;
} VertexFrameTriangle;

static void create_buffers(VideoContext* context, Buffers* buffers)
{
    Backend* backend = context->backend;
    Log* logger = context->logger;

    VertexFrameTriangle vertices[3] =
    {
        {(Float3){{-1.0f, -1.0f, 0.0f}}, (Float2){{0.0f, 0.0f}}},
        {(Float3){{+3.0f, -1.0f, 0.0f}}, (Float2){{2.0f, 0.0f}}},
        {(Float3){{-1.0f, +3.0f, 0.0f}}, (Float2){{0.0f, 2.0f}}},
    };

    BufferSpec frame_triangle_spec =
    {
        .format = BUFFER_FORMAT_VERTEX,
        .usage = BUFFER_USAGE_STATIC,
        .content = vertices,
        .size = sizeof(VertexFrameTriangle) * 3,
    };
    buffers->frame_triangle = create_buffer(backend, &frame_triangle_spec, logger);
}

static void destroy_buffers(VideoContext* context, Buffers* buffers)
{
    Backend* backend = context->backend;

    destroy_buffer(backend, buffers->frame_triangle);
}

static void create_images(VideoContext* context, Images* images)
{
    Backend* backend = context->backend;
    Uniforms* uniforms = &context->uniforms;

    Int2 dimensions;
    images->hatch_pattern = build_image(context, "polka_dot.png", false, &dimensions);
    images->hatch_pattern_dimensions = dimensions;

    images->line_pattern = build_image(context, "Line Feathering.png", true, &dimensions);
    images->line_pattern_dimensions = dimensions;

    images->point_pattern = build_image(context, "Point.png", true, NULL);

    PerImage per_image =
    {
        .texture_dimensions = int2_to_float2(dimensions),
    };
    update_buffer(backend, uniforms->buffers[0], &per_image, 0, sizeof(PerImage));
}

static void destroy_images(VideoContext* context, Images* images)
{
    Backend* backend = context->backend;

    destroy_image(backend, images->hatch_pattern);
    destroy_image(backend, images->line_pattern);
    destroy_image(backend, images->point_pattern);

    for(int i = 0; i < 1; i += 1)
    {
        destroy_image(backend, images->font_textures[i]);
    }
}

static void create_samplers(VideoContext* context, Samplers* samplers)
{
    Backend* backend = context->backend;
    Log* logger = context->logger;

    SamplerSpec nearest_repeat_spec =
    {
        .address_mode_u = SAMPLER_ADDRESS_MODE_REPEAT,
        .address_mode_v = SAMPLER_ADDRESS_MODE_REPEAT,
        .address_mode_w = SAMPLER_ADDRESS_MODE_REPEAT,
        .magnify_filter = SAMPLER_FILTER_POINT,
        .minify_filter = SAMPLER_FILTER_POINT,
    };
    samplers->nearest_repeat = create_sampler(backend, &nearest_repeat_spec, logger);

    SamplerSpec linear_repeat_spec =
    {
        .address_mode_u = SAMPLER_ADDRESS_MODE_REPEAT,
        .address_mode_v = SAMPLER_ADDRESS_MODE_REPEAT,
        .address_mode_w = SAMPLER_ADDRESS_MODE_REPEAT,
        .magnify_filter = SAMPLER_FILTER_LINEAR,
        .minify_filter = SAMPLER_FILTER_LINEAR,
    };
    samplers->linear_repeat = create_sampler(backend, &linear_repeat_spec, logger);

    SamplerSpec linear_mipmap_repeat_spec =
    {
        .address_mode_u = SAMPLER_ADDRESS_MODE_REPEAT,
        .address_mode_v = SAMPLER_ADDRESS_MODE_REPEAT,
        .address_mode_w = SAMPLER_ADDRESS_MODE_REPEAT,
        .magnify_filter = SAMPLER_FILTER_LINEAR,
        .minify_filter = SAMPLER_FILTER_LINEAR,
        .mipmap_filter = SAMPLER_FILTER_LINEAR,
    };
    samplers->linear_mipmap_repeat = create_sampler(backend, &linear_mipmap_repeat_spec, logger);
}

static void destroy_samplers(VideoContext* context, Samplers* samplers)
{
    destroy_sampler(context->backend, samplers->nearest_repeat);
    destroy_sampler(context->backend, samplers->linear_repeat);
    destroy_sampler(context->backend, samplers->linear_mipmap_repeat);
}

static void create_shaders(VideoContext* context, Shaders* shaders)
{
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

    UniformBlockSpec per_view_spec =
    {
        .binding = 0,
        .name = "PerView",
        .size = sizeof(PerView),
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
            .uniform_blocks = {per_view_spec},
        },
    };
    shaders->font = build_shader(context, &shader_font_spec, "Font.fs", "Font.vs");

    ShaderSpec face_selection_spec =
    {
        .fragment =
        {
            .uniform_blocks[0] = halo_block_spec,
        },
        .vertex =
        {
            .uniform_blocks =
            {
                per_view_spec,
                per_object_block_spec,
            },
        },
    };
    shaders->face_selection = build_shader(context, &face_selection_spec, "Face Selection.fs", "Face Selection.vs");

    ShaderSpec shader_halo_spec =
    {
        .fragment =
        {
            .uniform_blocks[0] = halo_block_spec,
        },
        .vertex =
        {
            .uniform_blocks =
            {
                per_view_spec,
                per_line_block_spec,
                per_object_block_spec,
            },
        },
    };
    shaders->halo = build_shader(context, &shader_halo_spec, "Halo.fs", "Halo.vs");

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
                per_view_spec,
                per_line_block_spec,
                per_image_block_spec,
                per_object_block_spec,
            },
        },
    };
    shaders->line = build_shader(context, &shader_line_spec, "Line.fs", "Line.vs");

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
                per_view_spec,
                per_object_block_spec,
            },
        },
    };
    shaders->lit = build_shader(context, &lit_spec, "Lit.fs", "Lit.vs");

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
                per_view_spec,
                per_point_block_spec,
            },
        },
    };
    shaders->point = build_shader(context, &shader_point_spec, "Point.fs", "Point.vs");

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
                per_image_block_spec,
                per_object_block_spec,
                per_view_spec,
            },
        },
    };
    shaders->screen_pattern = build_shader(context, &shader_screen_pattern_spec, "Screen Pattern.fs", "Screen Pattern.vs");

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
            .uniform_blocks[0] = per_view_spec,
        },
    };
    shaders->texture_only = build_shader(context, &texture_only_spec, "Texture Only.fs", "Texture Only.vs");

    ShaderSpec shader_vertex_colour_spec =
    {
        .vertex =
        {
            .uniform_blocks =
            {
                per_view_spec,
                per_object_block_spec,
            },
        },
    };
    shaders->vertex_colour = build_shader(context, &shader_vertex_colour_spec, "Vertex Colour.fs", "Vertex Colour.vs");
}

static void destroy_shaders(VideoContext* context, Shaders* shaders)
{
    Backend* backend = context->backend;

    destroy_shader(backend, shaders->face_selection);
    destroy_shader(backend, shaders->font);
    destroy_shader(backend, shaders->halo);
    destroy_shader(backend, shaders->line);
    destroy_shader(backend, shaders->lit);
    destroy_shader(backend, shaders->point);
    destroy_shader(backend, shaders->screen_pattern);
    destroy_shader(backend, shaders->texture_only);
    destroy_shader(backend, shaders->vertex_colour);
}

static void create_pipelines(VideoContext* context, Pipelines* pipelines)
{
    Backend* backend = context->backend;
    Log* logger = context->logger;
    Shaders* shaders = &context->shaders;

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

    DepthStencilStateSpec halo_depth_stencil_spec =
    {
        .depth_compare_enabled = true,
        .depth_write_enabled = true,
        .stencil_enabled = true,
        .back_stencil = halo_stencil_spec,
        .front_stencil = halo_stencil_spec,
    };

    VertexLayoutSpec frame_triangle_vertex_spec =
    {
        .attributes =
        {
            {.format = VERTEX_FORMAT_FLOAT3},
            {.format = VERTEX_FORMAT_FLOAT2},
        },
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
        .shader = shaders->vertex_colour,
        .vertex_layout = vertex_layout_spec_vertex_colour,
    };
    pipelines->background = create_pipeline(backend, &pipeline_background_spec, logger);

    PipelineSpec pipeline_unselected_spec =
    {
        .depth_stencil =
        {
            .depth_compare_enabled = true,
            .depth_write_enabled = true,
        },
        .shader = shaders->lit,
        .vertex_layout = vertex_layout_spec_lit,
    };
    pipelines->unselected = create_pipeline(backend, &pipeline_unselected_spec, logger);

    PipelineSpec pipeline_face_selection_spec =
    {
        .blend = alpha_blend_spec,
        .depth_stencil =
        {
            .depth_compare_enabled = true,
            .depth_compare_op = COMPARE_OP_LESS_OR_EQUAL,
            .depth_write_enabled = false,
        },
        .shader = shaders->face_selection,
        .vertex_layout = vertex_layout_spec_lit,
    };
    pipelines->face_selection = create_pipeline(backend, &pipeline_face_selection_spec, logger);

    PipelineSpec pipeline_pointcloud_spec =
    {
        .blend = alpha_blend_spec,
        .depth_stencil = transparent_depth_stencil_spec,
        .shader = shaders->point,
        .vertex_layout = vertex_layout_point_spec,
    };
    pipelines->pointcloud = create_pipeline(backend, &pipeline_pointcloud_spec, logger);

    PipelineSpec resolve_pipeline_spec =
    {
        .input_assembly =
        {
            .index_type = INDEX_TYPE_NONE,
        },
        .shader = shaders->texture_only,
        .vertex_layout = frame_triangle_vertex_spec,
    };
    pipelines->resolve = create_pipeline(backend, &resolve_pipeline_spec, logger);

    PipelineSpec pipeline_wireframe_spec =
    {
        .blend = alpha_blend_spec,
        .depth_stencil = transparent_depth_stencil_spec,
        .shader = shaders->line,
        .vertex_layout = vertex_layout_line_spec,
    };
    pipelines->wireframe = create_pipeline(backend, &pipeline_wireframe_spec, logger);

    PipelineSpec pipeline_halo_spec =
    {
        .depth_stencil = halo_depth_stencil_spec,
        .shader = shaders->halo,
        .vertex_layout = vertex_layout_line_spec,
    };
    pipelines->halo = create_pipeline(backend, &pipeline_halo_spec, logger);

    PipelineSpec pipeline_selected_spec =
    {
        .depth_stencil = selected_depth_stencil_spec,
        .shader = shaders->lit,
        .vertex_layout = vertex_layout_spec_lit,
    };
    pipelines->selected = create_pipeline(backend, &pipeline_selected_spec, logger);

    PipelineSpec pipeline_spec_hidden =
    {
        .depth_stencil =
        {
            .depth_compare_enabled = false,
            .depth_write_enabled = true,
        },
        .input_assembly =
        {
            .index_type = INDEX_TYPE_NONE,
        },
        .shader = shaders->screen_pattern,
        .vertex_layout = vertex_layout_spec_vertex_colour,
    };
    pipelines->hidden = create_pipeline(backend, &pipeline_spec_hidden, logger);
}

static void destroy_pipelines(VideoContext* context, Pipelines* pipelines)
{
    Backend* backend = context->backend;

    destroy_pipeline(backend, pipelines->background);
    destroy_pipeline(backend, pipelines->face_selection);
    destroy_pipeline(backend, pipelines->halo);
    destroy_pipeline(backend, pipelines->hidden);
    destroy_pipeline(backend, pipelines->pointcloud);
    destroy_pipeline(backend, pipelines->resolve);
    destroy_pipeline(backend, pipelines->selected);
    destroy_pipeline(backend, pipelines->unselected);
    destroy_pipeline(backend, pipelines->wireframe);
}

static void create_uniforms(VideoContext* context, Uniforms* uniforms)
{
    Backend* backend = context->backend;
    Log* logger = context->logger;

    BufferSpec per_image_spec =
    {
        .binding = 4,
        .size = sizeof(PerImage),
        .format = BUFFER_FORMAT_UNIFORM,
        .usage = BUFFER_USAGE_DYNAMIC,
    };
    uniforms->buffers[0] = create_buffer(backend, &per_image_spec, logger);

    BufferSpec per_line_spec =
    {
        .binding = 1,
        .size = sizeof(PerLine),
        .format = BUFFER_FORMAT_UNIFORM,
        .usage = BUFFER_USAGE_DYNAMIC,
    };
    uniforms->buffers[1] = create_buffer(backend, &per_line_spec, logger);

    BufferSpec per_object_spec =
    {
        .binding = 2,
        .size = sizeof(PerObject),
        .format = BUFFER_FORMAT_UNIFORM,
        .usage = BUFFER_USAGE_DYNAMIC,
    };
    uniforms->buffers[2] = create_buffer(backend, &per_object_spec, logger);

    BufferSpec per_view_spec =
    {
        .binding = 0,
        .size = sizeof(PerView),
        .format = BUFFER_FORMAT_UNIFORM,
        .usage = BUFFER_USAGE_DYNAMIC,
    };
    uniforms->buffers[3] = create_buffer(backend, &per_view_spec, logger);

    BufferSpec per_span_spec =
    {
        .binding = 3,
        .size = sizeof(PerSpan),
        .format = BUFFER_FORMAT_UNIFORM,
        .usage = BUFFER_USAGE_DYNAMIC,
    };
    uniforms->buffers[4] = create_buffer(backend, &per_span_spec, logger);

    BufferSpec halo_block_buffer_spec =
    {
        .binding = 6,
        .size = sizeof(HaloBlock),
        .format = BUFFER_FORMAT_UNIFORM,
        .usage = BUFFER_USAGE_DYNAMIC,
    };
    uniforms->halo_block = create_buffer(backend, &halo_block_buffer_spec, logger);

    BufferSpec per_point_spec =
    {
        .binding = 7,
        .size = sizeof(PerPoint),
        .format = BUFFER_FORMAT_UNIFORM,
        .usage = BUFFER_USAGE_DYNAMIC,
    };
    uniforms->per_point = create_buffer(backend, &per_point_spec, logger);

    BufferSpec light_block_buffer_spec =
    {
        .binding = 5,
        .size = sizeof(LightBlock),
        .format = BUFFER_FORMAT_UNIFORM,
        .usage = BUFFER_USAGE_DYNAMIC,
    };
    uniforms->light_block = create_buffer(backend, &light_block_buffer_spec, logger);
}

static void destroy_uniforms(VideoContext* context, Uniforms* uniforms)
{
    Backend* backend = context->backend;

    for(int i = 0; i < 5; i += 1)
    {
        destroy_buffer(backend, uniforms->buffers[i]);
    }
    destroy_buffer(backend, uniforms->halo_block);
    destroy_buffer(backend, uniforms->light_block);
    destroy_buffer(backend, uniforms->per_point);
}

static void create_context(VideoContext* context, Log* logger)
{
    stack_create(&context->scratch, (uint32_t) uptibytes(1));
    heap_create(&context->heap, (uint32_t) uptibytes(1));
    dense_map_create(&context->objects, &context->heap);

    context->backend = create_backend(&context->heap);
    context->logger = logger;

    create_buffers(context, &context->buffers);
    create_samplers(context, &context->samplers);
    create_shaders(context, &context->shaders);
    create_pipelines(context, &context->pipelines);
    create_uniforms(context, &context->uniforms);
    create_images(context, &context->images);

    Backend* backend = context->backend;
    Shaders* shaders = &context->shaders;
    Uniforms* uniforms = &context->uniforms;

    video_object_create(&context->sky, VERTEX_LAYOUT_PC);
    video_object_generate_sky(&context->sky, backend, logger, &context->scratch);

    ImmediateContextSpec immediate_spec =
    {
        .backend = backend,
        .shaders =
        {
            shaders->vertex_colour,
            shaders->font,
            shaders->line,
        },
        .uniform_buffers =
        {
            uniforms->buffers[1],
            uniforms->buffers[4],
        },
    };
    immediate_context_create(&immediate_spec, &context->heap, logger);
}

static void destroy_context(VideoContext* context, bool functions_loaded)
{
    if(functions_loaded)
    {
        destroy_buffers(context, &context->buffers);
        destroy_images(context, &context->images);
        destroy_samplers(context, &context->samplers);
        destroy_shaders(context, &context->shaders);
        destroy_pipelines(context, &context->pipelines);
        destroy_uniforms(context, &context->uniforms);

        video_object_destroy(&context->sky, context->backend);

        immediate_context_destroy(&context->heap);

        destroy_backend(context->backend, &context->heap);
    }

    dense_map_destroy(&context->objects, &context->heap);

    stack_destroy(&context->scratch);
    heap_destroy(&context->heap);
}

static void apply_object_block(VideoContext* context, VideoObject* object)
{
    Backend* backend = context->backend;
    Uniforms* uniforms = &context->uniforms;

    PerObject per_object =
    {
        .model = matrix4_transpose(object->model),
        .normal_matrix = matrix4_transpose(object->normal),
    };
    update_buffer(backend, uniforms->buffers[2], &per_object, 0, sizeof(per_object));
}

static void draw_object(VideoContext* context, VideoObject* object)
{
    DrawAction draw_action =
    {
        .index_buffer = object->buffers[1],
        .indices_count = object->indices_count,
        .vertex_buffers[0] = object->buffers[0],
    };
    draw(context->backend, &draw_action);
}

static void set_model_matrix(VideoContext* context, Matrix4 model)
{
    Backend* backend = context->backend;
    Uniforms* uniforms = &context->uniforms;

    PerObject per_object =
    {
        .model = matrix4_transpose(model),
    };
    update_buffer(backend, uniforms->buffers[2], &per_object, 0, sizeof(per_object));
}

static void set_view_projection(VideoContext* context, Matrix4 view, Matrix4 projection, Float2 viewport_dimensions)
{
    Backend* backend = context->backend;
    Uniforms* uniforms = &context->uniforms;

    PerView per_view =
    {
        .view_projection = matrix4_transpose(matrix4_multiply(projection, view)),
        .viewport_dimensions = viewport_dimensions,
    };
    update_buffer(backend, uniforms->buffers[3], &per_view, 0, sizeof(per_view));

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

static void draw_move_tool_and_vectors(VideoContext* context, MoveTool* move_tool)
{
    Backend* backend = context->backend;
    Images* images = &context->images;
    Pipelines* pipelines = &context->pipelines;
    Samplers* samplers = &context->samplers;
    Uniforms* uniforms = &context->uniforms;

    Matrix4 model = matrix4_compose_transform(move_tool->position, move_tool->orientation, float3_set_all(move_tool->scale));

    ImageSet image_set =
    {
        .stages[1] =
        {
            .images[0] = images->hatch_pattern,
            .samplers[0] = samplers->linear_repeat,
        },
    };

    PerImage per_image =
    {
        .texture_dimensions = int2_to_float2(images->hatch_pattern_dimensions),
    };
    update_buffer(backend, uniforms->buffers[0], &per_image, 0, sizeof(PerImage));

    set_model_matrix(context, model);
    immediate_set_override_pipeline(context->pipelines.hidden);
    set_pipeline(backend, pipelines->hidden);
    set_images(backend, &image_set);
    draw_move_tool(move_tool, true);

    set_model_matrix(context, matrix4_identity);
    immediate_set_override_pipeline(pipelines->hidden);
    draw_move_tool_vectors(move_tool);

    // draw solid parts on top of silhouette
    set_model_matrix(context, model);
    draw_move_tool(move_tool, false);

    set_model_matrix(context, matrix4_identity);
    draw_move_tool_vectors(move_tool);

    per_image.texture_dimensions = int2_to_float2(images->line_pattern_dimensions);
    update_buffer(backend, uniforms->buffers[0], &per_image, 0, sizeof(PerImage));
}

static void draw_rotate_tool(VideoContext* context, RotateTool* rotate_tool)
{
    Backend* backend = context->backend;
    Images* images = &context->images;
    Samplers* samplers = &context->samplers;

    ImageSet image_set =
    {
        .stages[1] =
        {
            .images[0] = images->line_pattern,
            .samplers[0] = samplers->linear_repeat,
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

static void draw_object_with_halo(VideoContext* context, ObjectLady* lady, int index, DenseMapId halo_id, Float4 colour, Matrix4 projection)
{
    ASSERT(index != invalid_index);
    ASSERT(index >= 0 && index < array_count(lady->objects));

    VideoObject* object = dense_map_look_up(&context->objects, lady->objects[index].video_object);
    VideoObject* halo = dense_map_look_up(&context->objects, halo_id);
    ASSERT(object->indices_count > 0);
    ASSERT(halo->indices_count > 0);

    Backend* backend = context->backend;
    Images* images = &context->images;
    Pipelines* pipelines = &context->pipelines;
    Samplers* samplers = &context->samplers;
    Uniforms* uniforms = &context->uniforms;

    // Draw the object.
    set_pipeline(backend, pipelines->selected);

    ClearState clear =
    {
        .flags = {.stencil = true},
        .stencil = 0xffffffff,
    };
    clear_target(backend, &clear);

    apply_object_block(context, object);
    draw_object(context, object);

    // Draw the halo.
    set_pipeline(backend, pipelines->halo);

    ImageSet image_set =
    {
        .stages[1] =
        {
            .images[0] = images->line_pattern,
            .samplers[0] = samplers->linear_mipmap_repeat,
        },
    };
    set_images(backend, &image_set);

    HaloBlock halo_block = {.halo_colour = colour};
    update_buffer(backend, uniforms->halo_block, &halo_block, 0, sizeof(halo_block));

    PerLine line_block =
    {
        .line_width = 3.0f,
        .projection_factor = projection.e[0],
    };
    update_buffer(backend, uniforms->per_point, &line_block, 0, sizeof(line_block));

    draw_object(context, halo);
}

static void draw_face_selection(VideoContext* context, VideoObject* object)
{
    if(!object || !object->indices_count)
    {
        return;
    }

    Backend* backend = context->backend;
    Pipelines* pipelines = &context->pipelines;
    Uniforms* uniforms = &context->uniforms;

    set_pipeline(backend, pipelines->face_selection);

    HaloBlock halo_block =
    {
        .halo_colour = (Float4){{1.0f, 0.5f, 0.0f, 0.8f}},
    };
    update_buffer(backend, uniforms->halo_block, &halo_block, 0, sizeof(halo_block));

    apply_object_block(context, object);
    draw_object(context, object);
}

static void draw_pointcloud(VideoContext* context, VideoObject* object, Matrix4 projection)
{
    if(!object || !object->indices_count)
    {
        return;
    }

    Backend* backend = context->backend;
    Images* images = &context->images;
    Pipelines* pipelines = &context->pipelines;
    Samplers* samplers = &context->samplers;
    Uniforms* uniforms = &context->uniforms;

    set_pipeline(backend, pipelines->pointcloud);

    ImageSet image_set =
    {
        .stages[1] =
        {
            .images[0] = images->point_pattern,
            .samplers[0] = samplers->linear_mipmap_repeat,
        },
    };
    set_images(backend, &image_set);

    PerLine point_block =
    {
        .line_width = 3.0f,
        .projection_factor = projection.e[0],
    };
    update_buffer(backend, uniforms->per_point, &point_block, 0, sizeof(point_block));

    apply_object_block(context, object);
    draw_object(context, object);
}

static void draw_wireframe(VideoContext* context, VideoObject* object, Matrix4 projection)
{
    if(!object || !object->indices_count)
    {
        return;
    }

    Backend* backend = context->backend;
    Images* images = &context->images;
    Pipelines* pipelines = &context->pipelines;
    Samplers* samplers = &context->samplers;
    Uniforms* uniforms = &context->uniforms;

    set_pipeline(backend, pipelines->wireframe);

    ImageSet image_set =
    {
        .stages[1] =
        {
            .images[0] = images->line_pattern,
            .samplers[0] = samplers->linear_mipmap_repeat,
        },
    };
    set_images(backend, &image_set);

    PerLine line_block =
    {
        .line_width = 4.0f,
        .projection_factor = projection.e[0],
    };
    update_buffer(backend, uniforms->per_point, &line_block, 0, sizeof(line_block));

    apply_object_block(context, object);
    draw_object(context, object);
}

static void draw_selection(VideoContext* context, DenseMapId faces_id, DenseMapId pointcloud_id, DenseMapId wireframe_id, Matrix4 projection)
{
    VideoObject* faces = NULL;
    if(faces_id)
    {
        faces = dense_map_look_up(&context->objects, faces_id);
    }

    VideoObject* pointcloud = NULL;
    if(pointcloud_id)
    {
        pointcloud = dense_map_look_up(&context->objects, pointcloud_id);
    }

    VideoObject* wireframe = NULL;
    if(wireframe_id)
    {
        wireframe = dense_map_look_up(&context->objects, wireframe_id);
    }

    draw_face_selection(context, faces);
    draw_pointcloud(context, pointcloud, projection);
    draw_wireframe(context, wireframe, projection);
}

static void set_regular_view(VideoContext* context, Int2 viewport, Matrix4 view, Matrix4 projection)
{
    Viewport viewport_state =
    {
        .bottom_left = {0, 0},
        .dimensions = viewport,
        .far_depth = 1.0f,
        .near_depth = 0.0f,
    };
    set_viewport(context->backend, &viewport_state);
    set_view_projection(context, view, projection, int2_to_float2(viewport));
}

static void draw_sky(VideoContext* context, Int2 viewport, Matrix4 view, Matrix4 projection)
{
    Backend* backend = context->backend;
    Pipelines* pipelines = &context->pipelines;

    Float2 viewport_dimensions = int2_to_float2(viewport);

    Viewport viewport_state =
    {
        .bottom_left = {0, 0},
        .dimensions = viewport,
        .far_depth = 1.0f,
        .near_depth = 1.0f,
    };
    set_viewport(backend, &viewport_state);
    set_view_projection(context, view, projection, viewport_dimensions);

    set_pipeline(backend, pipelines->background);
    apply_object_block(context, &context->sky);
    draw_object(context, &context->sky);
}

static void draw_compass(VideoContext* context, Camera* camera, Int2 viewport)
{
    Backend* backend = context->backend;

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

    const float across = 3.0f * sqrtf(3.0f);
    const float extent = across / 2.0f;

    Float3 direction = float3_normalise(float3_subtract(camera->target, camera->position));
    Matrix4 view = matrix4_look_at(float3_zero, direction, float3_unit_z);
    Matrix4 axis_projection = matrix4_orthographic_projection(across, across, -extent, extent);
    set_view_projection(context, view, axis_projection, int2_to_float2(viewport));
    set_model_matrix(context, matrix4_identity);

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

static void draw_file_dialog(VideoContext* context, UiItem* dialog_panel, UiContext* ui_context)
{
    Backend* backend = context->backend;
    Images* images = &context->images;
    Samplers* samplers = &context->samplers;

    ImageSet image_set =
    {
        .stages[1] =
        {
            .images[0] = images->font_textures[0],
            .samplers[0] = samplers->nearest_repeat,
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

static void draw_main_menu(VideoContext* context, UiContext* ui_context, UiItem* main_menu, Float2 viewport)
{
    Backend* backend = context->backend;
    Images* images = &context->images;
    Samplers* samplers = &context->samplers;

    ImageSet image_set =
    {
        .stages[1] =
        {
            .images[0] = images->font_textures[0],
            .samplers[0] = samplers->nearest_repeat,
        },
    };
    set_images(backend, &image_set);

    Rect space;
    space.bottom_left = (Float2){{-viewport.x / 2.0f, viewport.y / 2.0f}};
    space.dimensions.x = viewport.x;
    space.dimensions.y = 60.0f;
    space.bottom_left.y -= space.dimensions.y;
    ui_lay_out(main_menu, space, ui_context);
    ui_draw(main_menu, ui_context);

    ui_draw_focus_indicator(main_menu, ui_context);
}

static void draw_debug_readout_waves(UiItem* readout)
{
    for(int i = 0; i < DEBUG_CHANNEL_CAP; i += 1)
    {
        DebugChannel* channel = &debug_readout.channels[i];
        UiItem* item = &readout->container.items[i];

        Float2 channel_start = item->bounds.bottom_left;
        float2_add_assign(&channel_start, (Float2){{60.0f, 0.0f}});

        switch(channel->type)
        {
            case DEBUG_CHANNEL_TYPE_FLOAT:
            {
                for(int j = 0; j < DEBUG_CHANNEL_VALUE_CAP; j += 1)
                {
                    float value = channel->floats[j];
                    float t;
                    if(channel->float_min == channel->float_max)
                    {
                        t = 0.0f;
                    }
                    else
                    {
                        t = unlerp(channel->float_min, channel->float_max, value);
                    }
                    float x = 3.0f * j;
                    float y = 16.0f * t;
                    Float2 bottom_left = (Float2){{x, 2.0f}};
                    float2_add_assign(&bottom_left, channel_start);
                    Rect rect =
                    {
                        .bottom_left = bottom_left,
                        .dimensions = (Float2){{3.0f, y}},
                    };
                    immediate_add_rect(rect, float4_white);
                }
                break;
            }
            case DEBUG_CHANNEL_TYPE_INVALID:
            {
                break;
            }
        }
    }
    immediate_draw();
}

static void draw_debug_readout(VideoContext* context, UiContext* ui_context, UiItem* readout, Float2 viewport)
{
    Backend* backend = context->backend;
    Images* images = &context->images;
    Samplers* samplers = &context->samplers;

    ImageSet image_set =
    {
        .stages[1] =
        {
            .images[0] = images->font_textures[0],
            .samplers[0] = samplers->nearest_repeat,
        },
    };
    set_images(backend, &image_set);

    Rect space =
    {
        .bottom_left = {{-viewport.x / 2.0f, -viewport.y / 2.0f}},
        .dimensions = {{viewport.x, 80.0f}},
    };
    ui_lay_out(readout, space, ui_context);
    ui_draw(readout, ui_context);

    ui_draw_focus_indicator(readout, ui_context);

    draw_debug_readout_waves(readout);
}

static void draw_debug_images(VideoContext* context, Int2 viewport)
{
    Backend* backend = context->backend;
    Images* images = &context->images;
    Samplers* samplers = &context->samplers;

    ImageSet image_set =
    {
        .stages[1] =
        {
            .images[0] = images->frame_colour,
            .samplers[0] = samplers->nearest_repeat,
        },
    };
    set_images(backend, &image_set);

    Rect rect =
    {
        (Float2){{viewport.x / 2.0f - 80.0f, viewport.y / 2.0f - 80.0f}},
        (Float2){{60.0f, 60.0f}},
    };
    Quad quad = rect_to_quad(rect);
    Rect texture_rect = (Rect){float2_zero, float2_one};
    immediate_add_quad_textured(&quad, texture_rect);
    immediate_draw();
}

static void update_matrices(VideoUpdate* update, Matrices* matrices)
{
    Camera* camera = update->camera;
    Int2 viewport = update->viewport;

    matrices->view = matrix4_look_at(camera->position, camera->target, float3_unit_z);

    float fov = camera->field_of_view;
    float width = (float) viewport.x;
    float height = (float) viewport.y;
    matrices->projection = matrix4_perspective_projection(fov, width, height, camera->near_plane, camera->far_plane);

    Float3 direction = float3_normalise(float3_subtract(camera->target, camera->position));
    matrices->sky_view = matrix4_look_at(float3_zero, direction, float3_unit_z);

    matrices->sky_projection = matrix4_perspective_projection(fov, width, height, 0.001f, 1.0f);

    matrices->screen_projection = matrix4_orthographic_projection(width, height, -1.0f, 1.0f);
}

static void draw_debug_draw_shapes(VideoContext* context)
{
    set_model_matrix(context, matrix4_identity);

    for(int i = 0; i < debug_draw.shapes_count; i += 1)
    {
        DebugDrawShape shape = debug_draw.shapes[i];

        if(shape.colour.w < 1.0f)
        {
            immediate_set_blend_mode(BLEND_MODE_TRANSPARENT);
        }

        switch(shape.type)
        {
            case DEBUG_DRAW_SHAPE_TYPE_LINE_SEGMENT:
            {
                LineSegment line_segment = shape.line_segment;
                immediate_add_line(line_segment.start, line_segment.end, shape.colour);
                break;
            }
            case DEBUG_DRAW_SHAPE_TYPE_SPHERE:
            {
                Sphere sphere = shape.sphere;
                immediate_add_sphere(sphere.center, sphere.radius, shape.colour);
                break;
            }
        }
    }

    immediate_draw();
}

static void draw_opaque_phase(VideoContext* context, VideoUpdate* update, Matrices* matrices)
{
    ObjectLady* lady = update->lady;
    MoveTool* move_tool = update->move_tool;
    RotateTool* rotate_tool = update->rotate_tool;
    int selected_object_index = update->selected_object_index;
    int hovered_object_index = update->hovered_object_index;
    DenseMapId hover_halo = update->hover_halo;
    DenseMapId selection_halo = update->selection_halo;
    Int2 viewport = update->viewport;

    Backend* backend = context->backend;
    Passes* passes = &context->passes;
    Pipelines* pipelines = &context->pipelines;
    Uniforms* uniforms = &context->uniforms;

    FOR_ALL(VideoObject, context->objects.array)
    {
        video_object_set_matrices(it, matrices->view, matrices->projection);
    }

    Float3 light_direction = (Float3){{0.7f, 0.4f, -1.0f}};
    light_direction = float3_normalise(float3_negate(matrix4_transform_vector(matrices->view, light_direction)));

    LightBlock light_block = {light_direction};
    update_buffer(backend, uniforms->light_block, &light_block, 0, sizeof(light_block));

    set_pass(context->backend, passes->base);
    set_regular_view(context, viewport, matrices->view, matrices->projection);

    set_pipeline(backend, pipelines->unselected);

    ClearState clear_state =
    {
        .depth = 1.0f,
        .flags =
        {
            .colour = true,
            .depth = true,
        },
    };
    clear_target(backend, &clear_state);

    // Draw all unselected models.
    for(int i = 0; i < array_count(lady->objects); i += 1)
    {
        if(i == hovered_object_index || i == selected_object_index)
        {
            continue;
        }
        VideoObject* object = dense_map_look_up(&context->objects, lady->objects[i].video_object);
        apply_object_block(context, object);
        draw_object(context, object);
    }

    draw_debug_draw_shapes(context);

    if(is_valid_index(selected_object_index))
    {
        draw_object_with_halo(context, lady, selected_object_index, selection_halo, float4_white, matrices->projection);
    }
    if(is_valid_index(hovered_object_index) && hovered_object_index != selected_object_index)
    {
        draw_object_with_halo(context, lady, hovered_object_index, hover_halo, float4_yellow, matrices->projection);
    }

    if(is_valid_index(selected_object_index))
    {
        draw_move_tool_and_vectors(context, move_tool);
    }

    draw_rotate_tool(context, rotate_tool);

    video_object_set_matrices(&context->sky, matrices->sky_view, matrices->sky_projection);
    draw_sky(context, viewport, matrices->sky_view, matrices->sky_projection);
}

static void draw_transparent_phase(VideoContext* context, VideoUpdate* update, Matrices* matrices)
{
    Int2 viewport = update->viewport;

    set_regular_view(context, viewport, matrices->view, matrices->projection);

    DenseMapId selection_id = update->selection_id;
    DenseMapId selection_pointcloud_id = update->selection_pointcloud_id;
    DenseMapId selection_wireframe_id = update->selection_wireframe_id;

    draw_selection(context, selection_id, selection_pointcloud_id, selection_wireframe_id, matrices->projection);
}

static void draw_resolved_base_pass(VideoContext* context, VideoUpdate* update)
{
    Backend* backend = context->backend;
    Buffers* buffers = &context->buffers;
    Images* images = &context->images;
    Pipelines* pipelines = &context->pipelines;
    Samplers* samplers = &context->samplers;

    ImageSet image_set =
    {
        .stages[1] =
        {
            .images[0] = images->frame_colour,
            .samplers[0] = samplers->nearest_repeat,
        },
    };
    set_images(backend, &image_set);
    set_pipeline(backend, pipelines->resolve);
    set_regular_view(context, update->viewport, matrix4_identity, matrix4_identity);
    DrawAction draw_action =
    {
        .indices_count = 3,
        .vertex_buffers[0] = buffers->frame_triangle,
    };
    draw(backend, &draw_action);
}

static void draw_screen_phase(VideoContext* context, VideoUpdate* update, Matrices* matrices)
{
    Camera* camera = update->camera;
    bool dialog_enabled = update->dialog_enabled;
    UiItem* dialog_panel = update->dialog_panel;
    UiItem* main_menu = update->main_menu;
    UiItem* readout = update->debug_readout;
    UiContext* ui_context = update->ui_context;
    Int2 viewport = update->viewport;

    Backend* backend = context->backend;

    set_pass(backend, default_pass);
    set_pipeline(backend, context->pipelines.unselected);
    ClearState clear_state =
    {
        .depth = 1.0f,
        .flags =
        {
            .colour = true,
            .depth = true,
        },
    };
    clear_target(backend, &clear_state);

    draw_resolved_base_pass(context, update);

    draw_compass(context, camera, viewport);

    Viewport viewport_state =
    {
        .bottom_left = {0, 0},
        .dimensions = viewport,
        .far_depth = 1.0f,
        .near_depth = 0.0f,
    };
    set_viewport(backend, &viewport_state);

    Float2 viewport_dimensions = int2_to_float2(viewport);
    Matrix4 view = matrix4_identity;
    Matrix4 projection = matrices->screen_projection;
    set_view_projection(context, view, projection, viewport_dimensions);
    set_model_matrix(context, matrix4_identity);

    if(dialog_enabled)
    {
        draw_file_dialog(context, dialog_panel, ui_context);
    }

    draw_main_menu(context, ui_context, main_menu, viewport_dimensions);
    draw_debug_readout(context, ui_context, readout, viewport_dimensions);
    draw_debug_images(context, viewport);
}

VideoContext* video_create_context(Heap* heap, Log* logger)
{
    VideoContext* context = HEAP_ALLOCATE(heap, VideoContext, 1);
    create_context(context, logger);
    return context;
}

void video_destroy_context(VideoContext* context, Heap* heap, bool functions_loaded)
{
    destroy_context(context, functions_loaded);
    SAFE_HEAP_DEALLOCATE(heap, context);
}

void video_update_context(VideoContext* context, VideoUpdate* update, Platform* platform)
{
    Matrices matrices = {0};
    update_matrices(update, &matrices);

    draw_opaque_phase(context, update, &matrices);
    draw_transparent_phase(context, update, &matrices);
    draw_screen_phase(context, update, &matrices);
}

static void clean_up_base_pass(VideoContext* context)
{
    Backend* backend = context->backend;
    Images* images = &context->images;
    Passes* passes = &context->passes;

    destroy_pass(backend, passes->base);

    destroy_image(backend, images->frame_colour);
    destroy_image(backend, images->frame_depth);
    destroy_image(backend, images->frame_velocity);
}

static void recreate_base_pass(VideoContext* context, Int2 dimensions)
{
    Backend* backend = context->backend;
    Images* images = &context->images;
    Log* logger = context->logger;
    Passes* passes = &context->passes;

    ImageSpec spec_frame_colour =
    {
        .pixel_format = PIXEL_FORMAT_SRGB8_ALPHA8,
        .render_target = true,
        .width = dimensions.x,
        .height = dimensions.y,
    };
    images->frame_colour = create_image(backend, &spec_frame_colour, logger);

    ImageSpec spec_frame_depth =
    {
        .pixel_format = PIXEL_FORMAT_DEPTH24_STENCIL8,
        .render_target = true,
        .width = dimensions.x,
        .height = dimensions.y,
    };
    images->frame_depth = create_image(backend, &spec_frame_depth, logger);

    ImageSpec spec_frame_velocity =
    {
        .pixel_format = PIXEL_FORMAT_RG8,
        .render_target = true,
        .width = dimensions.x,
        .height = dimensions.y,
    };
    images->frame_velocity = create_image(backend, &spec_frame_velocity, logger);

    PassSpec spec_base =
    {
        .colour_attachments =
        {
            [0] = {.image = images->frame_colour},
            [1] = {.image = images->frame_velocity},
        },
        .depth_stencil_attachment = {.image = images->frame_depth},
    };
    passes->base = create_pass(backend, &spec_base, logger);
}

void video_resize_viewport(VideoContext* context, Int2 dimensions, double dots_per_millimeter, float fov)
{
    clean_up_base_pass(context);
    recreate_base_pass(context, dimensions);
}

DenseMapId video_add_object(VideoContext* context, VertexLayout vertex_layout)
{
    DenseMap* objects = &context->objects;
    DenseMapId id = dense_map_add(objects, &context->heap);
    VideoObject* object = dense_map_look_up(objects, id);
    video_object_create(object, vertex_layout);
    return id;
}

void video_remove_object(VideoContext* context, DenseMapId id)
{
    DenseMap* objects = &context->objects;
    VideoObject* object = dense_map_look_up(objects, id);
    video_object_destroy(object, context->backend);
    dense_map_remove(objects, id, &context->heap);
}

void video_set_up_font(VideoContext* context, BmfFont* font)
{
    Images* images = &context->images;

    for(int i = 0; i < font->pages_count; i += 1)
    {
        char* filename = font->pages[i].bitmap_filename;
        images->font_textures[i] = build_image(context, filename, false, NULL);
    }
}

void video_set_model(VideoContext* context, DenseMapId id, Matrix4 model)
{
    VideoObject* object = dense_map_look_up(&context->objects, id);
    video_object_set_model(object, model);
}

void video_update_mesh(VideoContext* context, DenseMapId id, JanMesh* mesh, Heap* heap)
{
    VideoObject* object = dense_map_look_up(&context->objects, id);
    video_object_update_mesh(object, mesh, context->backend, context->logger, heap);
}

void video_update_wireframe(VideoContext* context, DenseMapId id, JanMesh* mesh, Heap* heap)
{
    VideoObject* object = dense_map_look_up(&context->objects, id);
    video_object_update_wireframe(object, mesh, context->backend, context->logger, heap);
}

void video_update_selection(VideoContext* context, DenseMapId id, JanMesh* mesh, JanSelection* selection, Heap* heap)
{
    VideoObject* object = dense_map_look_up(&context->objects, id);
    video_object_update_selection(object, mesh, selection, context->backend, context->logger, heap);
}

void video_update_pointcloud_selection(VideoContext* context, DenseMapId id, JanMesh* mesh, JanSelection* selection, JanVertex* hovered, Heap* heap)
{
    VideoObject* object = dense_map_look_up(&context->objects, id);
    video_object_update_pointcloud_selection(object, mesh, selection, hovered, context->backend, context->logger, heap);
}

void video_update_wireframe_selection(VideoContext* context, DenseMapId id, JanMesh* mesh, JanSelection* selection, JanEdge* hovered, Heap* heap)
{
    VideoObject* object = dense_map_look_up(&context->objects, id);
    video_object_update_wireframe_selection(object, mesh, selection, hovered, context->backend, context->logger, heap);
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
