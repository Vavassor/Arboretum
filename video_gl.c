#include "video_internal.h"

#if defined(PROFILE_ES_3)
#include <GLES3/gl3.h>
#elif defined(PROFILE_CORE_3_3)
#include "gl_core_3_3.h"
#endif

#include "assert.h"
#include "id_pool.h"
#include "int_utilities.h"
#include "float_utilities.h"
#include "string_utilities.h"

#ifndef GL_COMPRESSED_RGB8_ETC2
#define GL_COMPRESSED_RGB8_ETC2 0x9274
#endif

#ifndef GL_COMPRESSED_RGBA_S3TC_DXT1_EXT
#define GL_COMPRESSED_RGBA_S3TC_DXT1_EXT 0x83f1
#endif

#ifndef GL_COMPRESSED_RGBA_S3TC_DXT3_EXT
#define GL_COMPRESSED_RGBA_S3TC_DXT3_EXT 0x83f2
#endif

#ifndef GL_COMPRESSED_RGBA_S3TC_DXT5_EXT
#define GL_COMPRESSED_RGBA_S3TC_DXT5_EXT 0x83f3
#endif

#ifndef GL_COMPRESSED_SRGB8_ETC2
#define GL_COMPRESSED_SRGB8_ETC2 0x9275
#endif

#ifndef GL_TEXTURE_MAX_ANISOTROPY_EXT
#define GL_TEXTURE_MAX_ANISOTROPY_EXT 0x84fe
#endif

#ifndef GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT
#define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT 0x84ff
#endif

typedef enum ResourceStatus
{
    RESOURCE_STATUS_INVALID,
    RESOURCE_STATUS_FAILURE,
    RESOURCE_STATUS_VALID,
} ResourceStatus;

typedef struct ResourceBase
{
    ResourceStatus status;
} ResourceBase;

typedef struct Buffer
{
    ResourceBase resource;
    GLuint id;
    GLenum type;
} Buffer;

typedef struct Image
{
    ResourceBase resource;
    ImageType type;
    PixelFormat pixel_format;
    GLuint texture;
    GLenum target;
    int width;
    int height;
    int depth;
    int mipmap_count;
    bool render_target;
} Image;

typedef struct Attachment
{
    ImageId image;
    int mip_level;
    int slice;
} Attachment;

typedef struct Pass
{
    ResourceBase resource;
    Attachment colour_attachments[PASS_COLOUR_ATTACHMENT_CAP];
    Attachment depth_stencil_attachment;
    GLuint framebuffer;
} Pass;

typedef struct BlendState
{
    float constant_colour[4];
    BlendOp alpha_op;
    BlendOp rgb_op;
    BlendFactor alpha_destination_factor;
    BlendFactor alpha_source_factor;
    BlendFactor rgb_destination_factor;
    BlendFactor rgb_source_factor;
    bool enabled;
    ColourComponentFlags colour_write_flags;
} BlendState;

typedef struct VertexAttribute
{
    GLenum type;
    int buffer_index;
    int offset;
    int size;
    int stride;
    bool normalised;
} VertexAttribute;

typedef struct InputAssembly
{
    PrimitiveTopology primitive_topology;
    IndexType index_type;
} InputAssembly;

typedef struct StencilOpState
{
    CompareOp compare_op;
    StencilOp depth_fail_op;
    StencilOp fail_op;
    StencilOp pass_op;
    uint32_t compare_mask;
    uint32_t reference;
    uint32_t write_mask;
} StencilOpState;

typedef struct DepthStencilState
{
    StencilOpState front_stencil;
    StencilOpState back_stencil;
    CompareOp depth_compare_op;
    bool depth_compare_enabled;
    bool depth_write_enabled;
    bool stencil_enabled;
} DepthStencilState;

typedef struct RasterizerState
{
    CullMode cull_mode;
    FaceWinding face_winding;
    float depth_bias_clamp;
    float depth_bias_constant;
    float depth_bias_slope;
    bool depth_bias_enabled;
} RasterizerState;

typedef struct Pipeline
{
    ResourceBase resource;
    VertexAttribute attributes[VERTEX_ATTRIBUTE_CAP];
    BlendState blend;
    DepthStencilState depth_stencil;
    InputAssembly input_assembly;
    RasterizerState rasterizer;
    ShaderId shader;
    GLuint vertex_array;
} Pipeline;

typedef struct Sampler
{
    ResourceBase resource;
    GLuint handle;
} Sampler;

typedef struct ShaderImage
{
    int texture_slot;
} ShaderImage;

typedef struct ShaderStage
{
    ShaderImage images[SHADER_STAGE_IMAGE_CAP];
} ShaderStage;

typedef struct Shader
{
    ResourceBase resource;
    ShaderStage stages[2];
    GLuint program;
} Shader;

typedef struct Capabilities
{
    int max_anisotropy;
} Capabilities;

typedef struct Features
{
    bool sampler_filter_anisotropic : 1;
    bool texture_compression_etc2 : 1;
    bool texture_compression_s3tc : 1;
} Features;

struct Backend
{
    Capabilities capabilities;
    Features features;
    IdPool buffer_id_pool;
    IdPool image_id_pool;
    IdPool pass_id_pool;
    IdPool pipeline_id_pool;
    IdPool sampler_id_pool;
    IdPool shader_id_pool;
    Buffer* buffers;
    Image* images;
    Pass* passes;
    Pipeline* pipelines;
    Sampler* samplers;
    Shader* shaders;
    Pipeline* current_pipeline;
};

static BlendFactor default_blend_factor(BlendFactor factor, BlendFactor default_factor)
{
    if(factor == BLEND_FACTOR_INVALID)
    {
        return default_factor;
    }
    return factor;
}

static BlendOp default_blend_op(BlendOp op, BlendOp default_op)
{
    if(op == BLEND_OP_INVALID)
    {
        return default_op;
    }
    return op;
}

static ColourComponentFlags default_colour_component_flags(ColourComponentFlags flags, ColourComponentFlags default_flags)
{
    if(!flags.r && !flags.g && !flags.b && !flags.a)
    {
        return default_flags;
    }
    return flags;
}

static CompareOp default_compare_op(CompareOp op, CompareOp default_op)
{
    if(op == COMPARE_OP_INVALID)
    {
        return default_op;
    }
    return op;
}

static CullMode default_cull_mode(CullMode mode, CullMode default_mode)
{
    if(mode == CULL_MODE_INVALID)
    {
        return default_mode;
    }
    return mode;
}

static FaceWinding default_face_winding(FaceWinding winding, FaceWinding default_winding)
{
    if(winding == FACE_WINDING_INVALID)
    {
        return default_winding;
    }
    return winding;
}

static ImageType default_image_type(ImageType type, ImageType default_type)
{
    if(type == IMAGE_TYPE_INVALID)
    {
        return default_type;
    }
    return type;
}

static IndexType default_index_type(IndexType type, IndexType default_type)
{
    if(type == INDEX_TYPE_INVALID)
    {
        return default_type;
    }
    return type;
}

static PrimitiveTopology default_primitive_topology(PrimitiveTopology topology, PrimitiveTopology default_topology)
{
    if(topology == PRIMITIVE_TOPOLOGY_INVALID)
    {
        return default_topology;
    }
    return topology;
}

static StencilOp default_stencil_op(StencilOp op, StencilOp default_op)
{
    if(op == STENCIL_OP_INVALID)
    {
        return default_op;
    }
    return op;
}

static GLenum get_blend_factor(BlendFactor blend_factor)
{
    switch(blend_factor)
    {
        case BLEND_FACTOR_CONSTANT_ALPHA:            return GL_CONSTANT_ALPHA;
        case BLEND_FACTOR_CONSTANT_COLOUR:           return GL_CONSTANT_COLOR;
        case BLEND_FACTOR_DST_ALPHA:                 return GL_DST_ALPHA;
        case BLEND_FACTOR_DST_COLOUR:                return GL_DST_COLOR;
        case BLEND_FACTOR_ONE:                       return GL_ONE;
        case BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA:  return GL_ONE_MINUS_CONSTANT_ALPHA;
        case BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOUR: return GL_ONE_MINUS_CONSTANT_COLOR;
        case BLEND_FACTOR_ONE_MINUS_DST_ALPHA:       return GL_ONE_MINUS_DST_ALPHA;
        case BLEND_FACTOR_ONE_MINUS_DST_COLOUR:      return GL_ONE_MINUS_DST_COLOR;
        case BLEND_FACTOR_ONE_MINUS_SRC_ALPHA:       return GL_ONE_MINUS_SRC_ALPHA;
        case BLEND_FACTOR_ONE_MINUS_SRC_COLOUR:      return GL_ONE_MINUS_SRC_COLOR;
        case BLEND_FACTOR_SRC_ALPHA:                 return GL_SRC_ALPHA;
        case BLEND_FACTOR_SRC_ALPHA_SATURATED:       return GL_SRC_ALPHA_SATURATE;
        case BLEND_FACTOR_SRC_COLOUR:                return GL_SRC_COLOR;
        case BLEND_FACTOR_ZERO:                      return GL_ZERO;
        default:                                     return 0;
    }
}

static GLenum get_blend_op(BlendOp blend_op)
{
    switch(blend_op)
    {
        case BLEND_OP_ADD:              return GL_FUNC_ADD;
        case BLEND_OP_REVERSE_SUBTRACT: return GL_FUNC_REVERSE_SUBTRACT;
        case BLEND_OP_SUBTRACT:         return GL_FUNC_SUBTRACT;
        default:                        return 0;
    }
}

static GLenum get_compare_op(CompareOp compare_op)
{
    switch(compare_op)
    {
        case COMPARE_OP_ALWAYS:           return GL_ALWAYS;
        case COMPARE_OP_EQUAL:            return GL_EQUAL;
        case COMPARE_OP_GREATER:          return GL_GREATER;
        case COMPARE_OP_GREATER_OR_EQUAL: return GL_GEQUAL;
        case COMPARE_OP_LESS:             return GL_LESS;
        case COMPARE_OP_LESS_OR_EQUAL:    return GL_LEQUAL;
        case COMPARE_OP_NEVER:            return GL_NEVER;
        case COMPARE_OP_NOT_EQUAL:        return GL_NOTEQUAL;
        default:                          return 0;
    }
}

static GLenum get_cube_face_target(int face)
{
    switch(face)
    {
        default:
        case 0: return GL_TEXTURE_CUBE_MAP_POSITIVE_X;
        case 1: return GL_TEXTURE_CUBE_MAP_NEGATIVE_X;
        case 2: return GL_TEXTURE_CUBE_MAP_POSITIVE_Y;
        case 3: return GL_TEXTURE_CUBE_MAP_NEGATIVE_Y;
        case 4: return GL_TEXTURE_CUBE_MAP_POSITIVE_Z;
        case 5: return GL_TEXTURE_CUBE_MAP_NEGATIVE_Z;
    }
}

static GLenum get_cull_mode(CullMode cull_mode)
{
    switch(cull_mode)
    {
        case CULL_MODE_BACK:           return GL_BACK;
        case CULL_MODE_FRONT:          return GL_FRONT;
        case CULL_MODE_FRONT_AND_BACK: return GL_FRONT_AND_BACK;
        default:                       return 0;
    }
}

static GLenum get_depth_stencil_attachment_target(PixelFormat pixel_format)
{
    switch(pixel_format)
    {
        case PIXEL_FORMAT_DEPTH16:
        case PIXEL_FORMAT_DEPTH24:
        case PIXEL_FORMAT_DEPTH32F:
            return GL_DEPTH_ATTACHMENT;
        case PIXEL_FORMAT_DEPTH24_STENCIL8:
            return GL_DEPTH_STENCIL_ATTACHMENT;
        default:
            return 0;
    }
}

static GLenum get_face_winding(FaceWinding face_winding)
{
    switch(face_winding)
    {
        case FACE_WINDING_CLOCKWISE:        return GL_CW;
        case FACE_WINDING_COUNTERCLOCKWISE: return GL_CCW;
        default:                            return 0;
    }
}

static GLenum get_generic_pixel_format(PixelFormat pixel_format)
{
    switch(pixel_format)
    {
        case PIXEL_FORMAT_RGBA8:
        case PIXEL_FORMAT_SRGB8_ALPHA8:
        case PIXEL_FORMAT_RGBA16F:
        case PIXEL_FORMAT_RGBA32F:
            return GL_RGBA;
        case PIXEL_FORMAT_RGB8:
        case PIXEL_FORMAT_SRGB8:
        case PIXEL_FORMAT_RGB16F:
        case PIXEL_FORMAT_RGB32F:
            return GL_RGB;
        case PIXEL_FORMAT_RG8:
        case PIXEL_FORMAT_RG16F:
        case PIXEL_FORMAT_RG32F:
            return GL_RG;
        case PIXEL_FORMAT_R8:
        case PIXEL_FORMAT_R16F:
        case PIXEL_FORMAT_R32F:
            return GL_RED;
        case PIXEL_FORMAT_DEPTH16:
        case PIXEL_FORMAT_DEPTH24:
        case PIXEL_FORMAT_DEPTH32F:
            return GL_DEPTH_COMPONENT;
        case PIXEL_FORMAT_DEPTH24_STENCIL8:
            return GL_DEPTH_STENCIL;
        case PIXEL_FORMAT_ETC2_RGB8:
            return GL_COMPRESSED_RGB8_ETC2;
        case PIXEL_FORMAT_ETC2_SRGB8:
            return GL_COMPRESSED_SRGB8_ETC2;
        case PIXEL_FORMAT_S3TC_DXT1:
            return GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
        case PIXEL_FORMAT_S3TC_DXT3:
            return GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
        case PIXEL_FORMAT_S3TC_DXT5:
            return GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
        default:
            return 0;
    }
}

static GLenum get_internal_pixel_format(PixelFormat pixel_format)
{
    switch(pixel_format)
    {
        case PIXEL_FORMAT_RGBA8:            return GL_RGBA8;
        case PIXEL_FORMAT_SRGB8_ALPHA8:     return GL_SRGB8_ALPHA8;
        case PIXEL_FORMAT_RGBA16F:          return GL_RGBA16F;
        case PIXEL_FORMAT_RGBA32F:          return GL_RGBA32F;
        case PIXEL_FORMAT_RGB8:             return GL_RGB8;
        case PIXEL_FORMAT_SRGB8:            return GL_SRGB8;
        case PIXEL_FORMAT_RGB16F:           return GL_RGB16F;
        case PIXEL_FORMAT_RGB32F:           return GL_RGB32F;
        case PIXEL_FORMAT_RG8:              return GL_RG8;
        case PIXEL_FORMAT_RG16F:            return GL_RG16F;
        case PIXEL_FORMAT_RG32F:            return GL_RG32F;
        case PIXEL_FORMAT_R8:               return GL_R8;
        case PIXEL_FORMAT_R16F:             return GL_R16F;
        case PIXEL_FORMAT_R32F:             return GL_R32F;
        case PIXEL_FORMAT_DEPTH16:          return GL_DEPTH_COMPONENT16;
        case PIXEL_FORMAT_DEPTH24:          return GL_DEPTH_COMPONENT24;
        case PIXEL_FORMAT_DEPTH32F:         return GL_DEPTH_COMPONENT32F;
        case PIXEL_FORMAT_DEPTH24_STENCIL8: return GL_DEPTH24_STENCIL8;
        case PIXEL_FORMAT_ETC2_RGB8:        return GL_COMPRESSED_RGB8_ETC2;
        case PIXEL_FORMAT_ETC2_SRGB8:       return GL_COMPRESSED_SRGB8_ETC2;
        case PIXEL_FORMAT_S3TC_DXT1:        return GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
        case PIXEL_FORMAT_S3TC_DXT3:        return GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
        case PIXEL_FORMAT_S3TC_DXT5:        return GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
        default:                            return 0;
    }
}

static GLenum get_magnify_filter(SamplerFilter filter)
{
    switch(filter)
    {
        default:
        case SAMPLER_FILTER_POINT:  return GL_NEAREST;
        case SAMPLER_FILTER_LINEAR: return GL_LINEAR;
    }
}

static GLenum get_minify_filter(SamplerFilter minify, SamplerFilter mipmap)
{
    switch(minify)
    {
        default:
        case SAMPLER_FILTER_POINT:
        {
            switch(mipmap)
            {
                default:
                    return GL_NEAREST;
                case SAMPLER_FILTER_POINT:
                    return GL_NEAREST_MIPMAP_NEAREST;
                case SAMPLER_FILTER_LINEAR:
                    return GL_NEAREST_MIPMAP_LINEAR;
            }
            break;
        }
        case SAMPLER_FILTER_LINEAR:
        {
            switch(mipmap)
            {
                default:
                    return GL_LINEAR;
                case SAMPLER_FILTER_POINT:
                    return GL_LINEAR_MIPMAP_NEAREST;
                case SAMPLER_FILTER_LINEAR:
                    return GL_LINEAR_MIPMAP_LINEAR;
            }
            break;
        }
    }
}

static GLenum get_pixel_format_type(PixelFormat pixel_format)
{
    switch(pixel_format)
    {
        case PIXEL_FORMAT_RGBA32F:
        case PIXEL_FORMAT_RGB32F:
        case PIXEL_FORMAT_RG32F:
        case PIXEL_FORMAT_R32F:
        case PIXEL_FORMAT_DEPTH32F:
            return GL_FLOAT;
        case PIXEL_FORMAT_RGBA16F:
        case PIXEL_FORMAT_RGB16F:
        case PIXEL_FORMAT_RG16F:
        case PIXEL_FORMAT_R16F:
            return GL_HALF_FLOAT;
        case PIXEL_FORMAT_RGBA8:
        case PIXEL_FORMAT_SRGB8_ALPHA8:
        case PIXEL_FORMAT_RGB8:
        case PIXEL_FORMAT_SRGB8:
        case PIXEL_FORMAT_RG8:
        case PIXEL_FORMAT_R8:
            return GL_UNSIGNED_BYTE;
        case PIXEL_FORMAT_DEPTH16:
            return GL_UNSIGNED_SHORT;
        case PIXEL_FORMAT_DEPTH24:
            return GL_UNSIGNED_INT;
        case PIXEL_FORMAT_DEPTH24_STENCIL8:
            return GL_UNSIGNED_INT_24_8;
        default:
            return 0;
    }
}

static GLenum get_stencil_op(StencilOp stencil_op)
{
    switch(stencil_op)
    {
        case STENCIL_OP_DECREMENT_AND_CLAMP: return GL_DECR;
        case STENCIL_OP_DECREMENT_AND_WRAP:  return GL_DECR_WRAP;
        case STENCIL_OP_INCREMENT_AND_CLAMP: return GL_INCR;
        case STENCIL_OP_INCREMENT_AND_WRAP:  return GL_INCR_WRAP;
        case STENCIL_OP_INVERT:              return GL_INVERT;
        case STENCIL_OP_KEEP:                return GL_KEEP;
        case STENCIL_OP_REPLACE:             return GL_REPLACE;
        case STENCIL_OP_ZERO:                return GL_ZERO;
        default:                             return 0;
    }
}

static GLenum get_texture_target(ImageType type)
{
    switch(type)
    {
        case IMAGE_TYPE_2D:      return GL_TEXTURE_2D;
        case IMAGE_TYPE_3D:      return GL_TEXTURE_3D;
        case IMAGE_TYPE_ARRAY:   return GL_TEXTURE_2D_ARRAY;
        case IMAGE_TYPE_CUBE:    return GL_TEXTURE_CUBE_MAP;
        default:                 return 0;
    }
}

static GLenum get_vertex_format_type(VertexFormat format)
{
    switch(format)
    {
        case VERTEX_FORMAT_FLOAT1:
        case VERTEX_FORMAT_FLOAT2:
        case VERTEX_FORMAT_FLOAT3:
        case VERTEX_FORMAT_FLOAT4:
            return GL_FLOAT;
        case VERTEX_FORMAT_UBYTE4_NORM:
            return GL_UNSIGNED_BYTE;
        case VERTEX_FORMAT_USHORT2_NORM:
            return GL_UNSIGNED_SHORT;
        default:
            return 0;
    }
}

static GLboolean get_vertex_format_normalised(VertexFormat format)
{
    switch(format)
    {
        default:
        case VERTEX_FORMAT_FLOAT1:
        case VERTEX_FORMAT_FLOAT2:
        case VERTEX_FORMAT_FLOAT3:
        case VERTEX_FORMAT_FLOAT4:
            return GL_FALSE;
        case VERTEX_FORMAT_UBYTE4_NORM:
        case VERTEX_FORMAT_USHORT2_NORM:
            return GL_TRUE;
    }
}

static GLenum get_wrap_parameter(SamplerAddressMode mode)
{
    switch(mode)
    {
        default:
        case SAMPLER_ADDRESS_MODE_REPEAT:          return GL_REPEAT;
        case SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT: return GL_MIRRORED_REPEAT;
        case SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE:   return GL_CLAMP_TO_EDGE;
    }
}

static GLenum translate_buffer_format(BufferFormat format)
{
    switch(format)
    {
        case BUFFER_FORMAT_VERTEX:  return GL_ARRAY_BUFFER;
        case BUFFER_FORMAT_INDEX:   return GL_ELEMENT_ARRAY_BUFFER;
        case BUFFER_FORMAT_UNIFORM: return GL_UNIFORM_BUFFER;
        default:                    return 0;
    }
}

static GLenum translate_buffer_usage(BufferUsage usage)
{
    switch(usage)
    {
        case BUFFER_USAGE_STATIC:  return GL_STATIC_DRAW;
        case BUFFER_USAGE_DYNAMIC: return GL_DYNAMIC_DRAW;
        default:                   return 0;
    }
}

static GLenum translate_primitive_topology(PrimitiveTopology primitive_topology)
{
    switch(primitive_topology)
    {
        case PRIMITIVE_TOPOLOGY_TRIANGLE_LIST: return GL_TRIANGLES;
        default:                               return 0;
    }
}

static GLenum translate_index_type(IndexType index_type)
{
    switch(index_type)
    {
        default:
        case INDEX_TYPE_NONE:   return 0;
        case INDEX_TYPE_UINT16: return GL_UNSIGNED_SHORT;
        case INDEX_TYPE_UINT32: return GL_UNSIGNED_INT;
    }
}

static bool is_pixel_format_supported(Backend* backend, PixelFormat pixel_format)
{
    switch(pixel_format)
    {
        case PIXEL_FORMAT_INVALID:
            return false;
        case PIXEL_FORMAT_ETC2_RGB8:
        case PIXEL_FORMAT_ETC2_SRGB8:
            return backend->features.texture_compression_etc2;
        case PIXEL_FORMAT_S3TC_DXT1:
        case PIXEL_FORMAT_S3TC_DXT3:
        case PIXEL_FORMAT_S3TC_DXT5:
            return backend->features.texture_compression_s3tc;
        default:
            return true;
    }
}

static Buffer* fetch_buffer(Backend* backend, BufferId id)
{
    if(id.value != invalid_id)
    {
        uint32_t slot = get_id_slot(id.value);
        ASSERT(slot >= 0 && slot < (uint32_t) backend->buffer_id_pool.cap);
        return &backend->buffers[slot];
    }
    return NULL;
}

static Image* fetch_image(Backend* backend, ImageId id)
{
    if(id.value != invalid_id)
    {
        uint32_t slot = get_id_slot(id.value);
        ASSERT(slot >= 0 && slot < (uint32_t) backend->image_id_pool.cap);
        return &backend->images[slot];
    }
    return NULL;
}

static Pass* fetch_pass(Backend* backend, PassId id)
{
    if(id.value != invalid_id)
    {
        uint32_t slot = get_id_slot(id.value);
        ASSERT(slot >= 0 && slot < (uint32_t) backend->pass_id_pool.cap);
        return &backend->passes[slot];
    }
    return NULL;
}

static Pipeline* fetch_pipeline(Backend* backend, PipelineId id)
{
    if(id.value != invalid_id)
    {
        uint32_t slot = get_id_slot(id.value);
        ASSERT(slot >= 0 && slot < (uint32_t) backend->pipeline_id_pool.cap);
        return &backend->pipelines[slot];
    }
    return NULL;
}

static Sampler* fetch_sampler(Backend* backend, SamplerId id)
{
    if(id.value != invalid_id)
    {
        uint32_t slot = get_id_slot(id.value);
        ASSERT(slot >= 0 && slot < (uint32_t) backend->sampler_id_pool.cap);
        return &backend->samplers[slot];
    }
    return NULL;
}

static Shader* fetch_shader(Backend* backend, ShaderId id)
{
    if(id.value != invalid_id)
    {
        uint32_t slot = get_id_slot(id.value);
        ASSERT(slot >= 0 && slot < (uint32_t) backend->shader_id_pool.cap);
        return &backend->shaders[slot];
    }
    return NULL;
}

static void load_buffer(Buffer* buffer, BufferSpec* spec)
{
    ASSERT(spec->format != BUFFER_FORMAT_INVALID);
    ASSERT(spec->usage != BUFFER_USAGE_INVALID);
    ASSERT(spec->size > 0);

    glGenBuffers(1, &buffer->id);

    buffer->type = translate_buffer_format(spec->format);
    GLenum usage = translate_buffer_usage(spec->usage);

    glBindBuffer(buffer->type, buffer->id);
    glBufferData(buffer->type, spec->size, spec->content, usage);

    if(spec->format == BUFFER_FORMAT_UNIFORM)
    {
        glBindBufferRange(buffer->type, spec->binding, buffer->id, 0, spec->size);
    }

    buffer->resource.status = RESOURCE_STATUS_VALID;
}

static void unload_buffer(Buffer* buffer)
{
    glDeleteBuffers(1, &buffer->id);

    buffer->resource.status = RESOURCE_STATUS_INVALID;
}

static void copy_to_buffer(Buffer* buffer, const void* memory, int base, int size)
{
    glBindBuffer(buffer->type, buffer->id);
    glBufferSubData(buffer->type, base, size, memory);
}

static void load_image(Image* image, ImageSpec* spec, Backend* backend)
{
    ASSERT(spec->width > 0);
    ASSERT(spec->height > 0);

    if(!is_pixel_format_supported(backend, spec->pixel_format))
    {
        image->resource.status = RESOURCE_STATUS_FAILURE;
        return;
    }

    image->type = default_image_type(spec->type, IMAGE_TYPE_2D);
    image->pixel_format = spec->pixel_format;
    image->target = get_texture_target(image->type);
    image->width = spec->width;
    image->height = spec->height;
    image->depth = imax(spec->depth, 1);
    image->mipmap_count = imax(spec->mipmap_count, 1);
    image->render_target = spec->render_target;

    int face_count = 1;
    if(image->type == IMAGE_TYPE_CUBE)
    {
        face_count = 6;
    }

    bool compressed = is_pixel_format_compressed(image->pixel_format);
    GLenum internal_format = get_internal_pixel_format(image->pixel_format);
    GLenum format = get_generic_pixel_format(image->pixel_format);
    GLenum type = get_pixel_format_type(image->pixel_format);

    glGenTextures(1, &image->texture);
    glBindTexture(image->target, image->texture);

    for(int face = 0; face < face_count; face += 1)
    {
        GLenum target = image->target;
        if(image->type == IMAGE_TYPE_CUBE)
        {
            target = get_cube_face_target(face);
        }

        for(int mip_level = 0; mip_level < image->mipmap_count; mip_level += 1)
        {
            Subimage* subimage = &spec->content.subimages[face][mip_level];

            int width = imax(spec->width >> mip_level, 1);
            int height = imax(spec->height >> mip_level, 1);

            switch(image->type)
            {
                case IMAGE_TYPE_2D:
                case IMAGE_TYPE_CUBE:
                {
                    if(compressed)
                    {
                        glCompressedTexImage2D(target, mip_level, internal_format, width, height, 0, subimage->size, subimage->content);
                    }
                    else
                    {
                        glTexImage2D(target, mip_level, internal_format, width, height, 0, format, type, subimage->content);
                    }
                    break;
                }
                case IMAGE_TYPE_3D:
                case IMAGE_TYPE_ARRAY:
                {
                    int depth = imax(spec->depth >> mip_level, 1);
                    if(compressed)
                    {
                        glCompressedTexImage3D(target, mip_level, internal_format, width, height, depth, 0, subimage->size, subimage->content);
                    }
                    else
                    {
                        glTexImage3D(target, mip_level, internal_format, width, height, depth, 0, format, type, subimage->content);
                    }
                    break;
                }
                case IMAGE_TYPE_INVALID:
                {
                    break;
                }
            }
        }
    }

    image->resource.status = RESOURCE_STATUS_VALID;
}

static void unload_image(Image* image)
{
    if(image->texture)
    {
        glDeleteTextures(1, &image->texture);
    }

    image->resource.status = RESOURCE_STATUS_INVALID;
}

static void update_image(Image* image, ImageContent* content)
{
    int face_count = 1;
    if(image->type == IMAGE_TYPE_CUBE)
    {
        face_count = 6;
    }

    GLenum format = get_generic_pixel_format(image->pixel_format);
    GLenum type = get_pixel_format_type(image->pixel_format);

    glBindTexture(image->target, image->texture);

    for(int i = 0; i < face_count; i += 1)
    {
        GLenum target = image->target;
        if(image->type == IMAGE_TYPE_CUBE)
        {
            target = get_cube_face_target(i);
        }

        for(int j = 0; j < image->mipmap_count; j += 1)
        {
            Subimage* subimage = &content->subimages[i][j];

            int width = imax(image->width >> j, 1);
            int height = imax(image->height >> j, 1);

            switch(image->type)
            {
                case IMAGE_TYPE_2D:
                case IMAGE_TYPE_CUBE:
                {
                    glTexSubImage2D(target, j, 0, 0, width, height, format, type, subimage->content);
                    break;
                }
                case IMAGE_TYPE_3D:
                case IMAGE_TYPE_ARRAY:
                {
                    int depth = imax(image->depth >> j, 1);
                    glTexSubImage3D(target, j, 0, 0, 0, width, height, depth, format, type, subimage->content);
                    break;
                }
                case IMAGE_TYPE_INVALID:
                {
                    break;
                }
            }
        }
    }
}

static void set_up_attachment_image(Attachment* attachment, Image* image, GLuint point)
{
    int mip_level = attachment->mip_level;
    int slice = attachment->slice;

    switch(image->type)
    {
        default:
        case IMAGE_TYPE_2D:
        {
            glFramebufferTexture2D(GL_FRAMEBUFFER, point, GL_TEXTURE_2D, image->texture, mip_level);
            break;
        }
        case IMAGE_TYPE_3D:
        case IMAGE_TYPE_ARRAY:
        {
            glFramebufferTextureLayer(GL_FRAMEBUFFER, point, image->texture, mip_level, slice);
            break;
        }
        case IMAGE_TYPE_CUBE:
        {
            GLenum target = get_cube_face_target(slice);
            glFramebufferTexture2D(GL_FRAMEBUFFER, point, target, image->texture, mip_level);
            break;
        }
    }
}

static void load_pass(Pass* pass, PassSpec* spec, Backend* backend)
{
    Attachment* attachment;
    AttachmentSpec* attachment_spec;
    for(int i = 0; i < PASS_COLOUR_ATTACHMENT_CAP; i += 1)
    {
        attachment_spec = &spec->colour_attachments[i];
        if(attachment_spec->image.value == invalid_id)
        {
            break;
        }

        attachment = &pass->colour_attachments[i];
        attachment->image = attachment_spec->image;
        attachment->mip_level = attachment_spec->mip_level;
        attachment->slice = attachment_spec->slice;
    }

    attachment_spec = &spec->depth_stencil_attachment;
    if(attachment_spec->image.value != invalid_id)
    {
        attachment = &pass->depth_stencil_attachment;
        attachment->image = attachment_spec->image;
        attachment->mip_level = attachment_spec->mip_level;
        attachment->slice = attachment_spec->slice;
    }

    glGenFramebuffers(1, &pass->framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, pass->framebuffer);

    Image* image;
    for(int i = 0; i < PASS_COLOUR_ATTACHMENT_CAP; i += 1)
    {
        attachment = &pass->colour_attachments[i];
        image = fetch_image(backend, attachment->image);
        if(!image)
        {
            break;
        }

        GLuint point = GL_COLOR_ATTACHMENT0 + i;
        set_up_attachment_image(attachment, image, point);
    }

    image = fetch_image(backend, pass->depth_stencil_attachment.image);
    if(image)
    {
        GLenum point = get_depth_stencil_attachment_target(image->pixel_format);
        set_up_attachment_image(attachment, image, point);
    }

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if(status != GL_FRAMEBUFFER_COMPLETE)
    {
        pass->resource.status = RESOURCE_STATUS_FAILURE;
        return;
    }

    pass->resource.status = RESOURCE_STATUS_VALID;
}

static void unload_pass(Pass* pass)
{
    if(pass->framebuffer)
    {
        glDeleteFramebuffers(1, &pass->framebuffer);
    }

    pass->resource.status = RESOURCE_STATUS_INVALID;
}

static void load_blend_state(BlendState* state, BlendStateSpec* spec)
{
    for(int i = 0; i < 4; i += 1)
    {
        state->constant_colour[i] = spec->constant_colour[i];
    }

    state->alpha_op = default_blend_op(spec->alpha_op, BLEND_OP_ADD);
    state->rgb_op = default_blend_op(spec->rgb_op, BLEND_OP_ADD);
    state->alpha_destination_factor = default_blend_factor(spec->alpha_destination_factor, BLEND_FACTOR_ZERO);
    state->alpha_source_factor = default_blend_factor(spec->alpha_source_factor, BLEND_FACTOR_ONE);
    state->rgb_destination_factor = default_blend_factor(spec->rgb_destination_factor, BLEND_FACTOR_ZERO);
    state->rgb_source_factor = default_blend_factor(spec->rgb_source_factor, BLEND_FACTOR_ONE);
    state->enabled = spec->enabled;

    ColourComponentFlags default_flags =
    {
        .r = true,
        .g = true,
        .b = true,
        .a = true,
        .disable_all = false,
    };
    state->colour_write_flags = default_colour_component_flags(spec->colour_write_flags, default_flags);
}

static void load_stencil_op_state(StencilOpState* state, StencilOpStateSpec* spec)
{
    state->compare_op = default_compare_op(spec->compare_op, COMPARE_OP_ALWAYS);
    state->depth_fail_op = default_stencil_op(spec->depth_fail_op, STENCIL_OP_KEEP);
    state->fail_op = default_stencil_op(spec->fail_op, STENCIL_OP_KEEP);
    state->pass_op = default_stencil_op(spec->pass_op, STENCIL_OP_KEEP);
    state->compare_mask = spec->compare_mask;
    state->reference = spec->reference;
    state->write_mask = spec->write_mask;
}

static void load_depth_stencil_state(DepthStencilState* state, DepthStencilStateSpec* spec)
{
    load_stencil_op_state(&state->back_stencil, &spec->back_stencil);
    load_stencil_op_state(&state->front_stencil, &spec->front_stencil);

    state->depth_compare_op = default_compare_op(spec->depth_compare_op, COMPARE_OP_LESS);
    state->depth_compare_enabled = spec->depth_compare_enabled;
    state->depth_write_enabled = spec->depth_write_enabled;
    state->stencil_enabled = spec->stencil_enabled;
}

static void load_input_assembly(InputAssembly* input_assembly, InputAssemblySpec* spec)
{
    input_assembly->primitive_topology = default_primitive_topology(spec->primitive_topology, PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    input_assembly->index_type = default_index_type(spec->index_type, INDEX_TYPE_UINT16);
}

static void load_rasterizer_state(RasterizerState* state, RasterizerStateSpec* spec)
{
    state->cull_mode = default_cull_mode(spec->cull_mode, CULL_MODE_BACK);
    state->face_winding = default_face_winding(spec->face_winding, FACE_WINDING_COUNTERCLOCKWISE);
    state->depth_bias_clamp = spec->depth_bias_clamp;
    state->depth_bias_constant = spec->depth_bias_constant;
    state->depth_bias_slope = spec->depth_bias_slope;
    state->depth_bias_enabled = spec->depth_bias_enabled;
}

static void load_vertex_layout(Pipeline* pipeline, VertexLayoutSpec* spec)
{
    for(int i = 0; i < VERTEX_ATTRIBUTE_CAP; i += 1)
    {
        pipeline->attributes[i].buffer_index = invalid_index;
    }

    int auto_offset[SHADER_STAGE_BUFFER_CAP] = {0};

    for(int i = 0; i < VERTEX_ATTRIBUTE_CAP; i += 1)
    {
        VertexAttributeSpec* attribute_spec = &spec->attributes[i];
        if(attribute_spec->format == VERTEX_FORMAT_INVALID)
        {
            break;
        }

        int buffer_index = attribute_spec->buffer_index;
        GLenum format = attribute_spec->format;

        VertexAttribute* attribute = &pipeline->attributes[i];
        attribute->type = get_vertex_format_type(format);
        attribute->buffer_index = buffer_index;
        attribute->offset = auto_offset[buffer_index];
        attribute->size = get_vertex_format_component_count(format);
        attribute->normalised = get_vertex_format_normalised(format);

        auto_offset[attribute->buffer_index] += get_vertex_format_size(format);
    }

    for(int i = 0; i < VERTEX_ATTRIBUTE_CAP; i += 1)
    {
        VertexAttribute* attribute = &pipeline->attributes[i];
        if(attribute->buffer_index != invalid_index)
        {
            attribute->stride = auto_offset[attribute->buffer_index];
        }
    }
}

static void load_pipeline(Pipeline* pipeline, PipelineSpec* spec)
{
    ASSERT(spec->shader.value != invalid_id);
    pipeline->shader = spec->shader;

    load_blend_state(&pipeline->blend, &spec->blend);
    load_depth_stencil_state(&pipeline->depth_stencil, &spec->depth_stencil);
    load_input_assembly(&pipeline->input_assembly, &spec->input_assembly);
    load_rasterizer_state(&pipeline->rasterizer, &spec->rasterizer);
    load_vertex_layout(pipeline, &spec->vertex_layout);

    glGenVertexArrays(1, &pipeline->vertex_array);

    pipeline->resource.status = RESOURCE_STATUS_VALID;
}

static void unload_pipeline(Pipeline* pipeline)
{
    glDeleteVertexArrays(1, &pipeline->vertex_array);

    pipeline->resource.status = RESOURCE_STATUS_INVALID;
}

static void load_sampler(Sampler* sampler, SamplerSpec* spec, Backend* backend)
{
    glGenSamplers(1, &sampler->handle);

    GLenum minify = get_minify_filter(spec->minify_filter, spec->mipmap_filter);
    GLenum magnify = get_magnify_filter(spec->magnify_filter);
    glSamplerParameteri(sampler->handle, GL_TEXTURE_MIN_FILTER, minify);
    glSamplerParameteri(sampler->handle, GL_TEXTURE_MAG_FILTER, magnify);

    float min_lod = clamp(spec->min_lod, 0.0f, 1000.0f);
    float max_lod = spec->max_lod;
    if(max_lod == 0.0f)
    {
        max_lod = 1000.0f;
    }
    max_lod = clamp(max_lod, 0.0f, 1000.0f);
    glSamplerParameterf(sampler->handle, GL_TEXTURE_MIN_LOD, min_lod);
    glSamplerParameterf(sampler->handle, GL_TEXTURE_MAX_LOD, max_lod);

    GLenum wrap_u = get_wrap_parameter(spec->address_mode_u);
    GLenum wrap_v = get_wrap_parameter(spec->address_mode_v);
    GLenum wrap_w = get_wrap_parameter(spec->address_mode_w);
    glSamplerParameteri(sampler->handle, GL_TEXTURE_WRAP_S, wrap_u);
    glSamplerParameteri(sampler->handle, GL_TEXTURE_WRAP_T, wrap_v);
    glSamplerParameteri(sampler->handle, GL_TEXTURE_WRAP_R, wrap_w);

    if(backend->features.sampler_filter_anisotropic && spec->max_anisotropy > 1)
    {
        int max_anisotropy = imin(backend->capabilities.max_anisotropy, spec->max_anisotropy);
        glSamplerParameteri(sampler->handle, GL_TEXTURE_MAX_ANISOTROPY_EXT, max_anisotropy);
    }

    sampler->resource.status = RESOURCE_STATUS_VALID;
}

static void unload_sampler(Sampler* sampler)
{
    glDeleteSamplers(1, &sampler->handle);

    sampler->resource.status = RESOURCE_STATUS_INVALID;
}

static GLuint compile_shader(GLenum type, const char* source, Heap* heap, Log* log)
{
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);

    GLint compile_status;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compile_status);
    if(compile_status == GL_FALSE)
    {
        GLint info_log_size = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &info_log_size);
        if(info_log_size > 0)
        {
            GLchar* info_log = HEAP_ALLOCATE(heap, GLchar, info_log_size);
            GLsizei bytes_written = 0;
            glGetShaderInfoLog(shader, info_log_size, &bytes_written, info_log);
            log_error(log, "%s", info_log);
            HEAP_DEALLOCATE(heap, info_log);
        }

        glDeleteShader(shader);

        return 0;
    }

    return shader;
}

static bool check_link_status(GLuint program, Heap* heap, Log* log)
{
    GLint link_status;
    glGetProgramiv(program, GL_LINK_STATUS, &link_status);
    if(link_status == GL_FALSE)
    {
        GLint info_log_size = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &info_log_size);
        if(info_log_size > 0)
        {
            GLchar* info_log = HEAP_ALLOCATE(heap, GLchar, info_log_size);
            GLsizei bytes_written = 0;
            glGetProgramInfoLog(program, info_log_size, &bytes_written, info_log);
            log_error(log, "%s", info_log);
            HEAP_DEALLOCATE(heap, info_log);
        }
        glDeleteProgram(program);
        return false;
    }
    return true;
}

static void set_up_uniform_blocks(Shader* shader, ShaderSpec* shader_spec)
{
    for(int i = 0; i < 2; i += 1)
    {
        ShaderStageSpec* shader_stage_spec;
        if(i == 0)
        {
            shader_stage_spec = &shader_spec->vertex;
        }
        else
        {
            shader_stage_spec = &shader_spec->fragment;
        }
        for(int j = 0; j < SHADER_STAGE_UNIFORM_BLOCK_CAP; j += 1)
        {
            UniformBlockSpec* uniform_block_spec = &shader_stage_spec->uniform_blocks[j];
            if(uniform_block_spec->size == 0)
            {
                break;
            }

            ASSERT(uniform_block_spec->name);

            GLuint index = glGetUniformBlockIndex(shader->program, uniform_block_spec->name);
            glUniformBlockBinding(shader->program, index, uniform_block_spec->binding);
        }
    }
}

static void set_up_shader_images(Shader* shader, ShaderSpec* shader_spec)
{
    int texture_slot = 0;
    glUseProgram(shader->program);

    for(int i = 0; i < 2; i += 1)
    {
        ShaderStageSpec* shader_stage_spec;
        if(i == 0)
        {
            shader_stage_spec = &shader_spec->vertex;
        }
        else
        {
            shader_stage_spec = &shader_spec->fragment;
        }
        ShaderStage* stage = &shader->stages[i];

        for(int j = 0; j < SHADER_STAGE_IMAGE_CAP; j += 1)
        {
            ShaderImageSpec* image_spec = &shader_stage_spec->images[j];
            if(image_spec->type == IMAGE_TYPE_INVALID)
            {
                break;
            }

            ASSERT(image_spec->name);

            ShaderImage* image = &stage->images[j];
            image->texture_slot = texture_slot;
            texture_slot += 1;

            GLint location = glGetUniformLocation(shader->program, image_spec->name);
            glUniform1i(location, image->texture_slot);
        }
    }
}

static void load_shader(Shader* shader, ShaderSpec* shader_spec, Heap* heap, Log* log)
{
    ASSERT(shader_spec->fragment.source);
    ASSERT(shader_spec->vertex.source);

    GLuint fragment_shader = compile_shader(GL_FRAGMENT_SHADER, shader_spec->fragment.source, heap, log);
    if(fragment_shader == 0)
    {
        shader->resource.status = RESOURCE_STATUS_FAILURE;
        return;
    }

    GLuint vertex_shader = compile_shader(GL_VERTEX_SHADER, shader_spec->vertex.source, heap, log);
    if(vertex_shader == 0)
    {
        glDeleteShader(fragment_shader);
        shader->resource.status = RESOURCE_STATUS_FAILURE;
        return;
    }

    GLuint program = glCreateProgram();
    glAttachShader(program, fragment_shader);
    glAttachShader(program, vertex_shader);
    glLinkProgram(program);
    glDeleteShader(fragment_shader);
    glDeleteShader(vertex_shader);

    bool success = check_link_status(program, heap, log);
    if(!success)
    {
        shader->resource.status = RESOURCE_STATUS_FAILURE;
        return;
    }
    shader->program = program;

    set_up_uniform_blocks(shader, shader_spec);
    set_up_shader_images(shader, shader_spec);

    shader->resource.status = RESOURCE_STATUS_VALID;
}

static void unload_shader(Shader* shader)
{
    glDeleteProgram(shader->program);

    shader->resource.status = RESOURCE_STATUS_INVALID;
}

static void set_up_features(Features* features)
{
#if defined(PROFILE_ES_3)
    features->texture_compression_etc2 = true;
#endif

    int extension_count;
    glGetIntegerv(GL_NUM_EXTENSIONS, &extension_count);

    for(int i = 0; i < extension_count; i += 1)
    {
        const char* name = (const char*) glGetStringi(GL_EXTENSIONS, i);

        if(string_ends_with(name, "texture_filter_anisotropic"))
        {
            features->sampler_filter_anisotropic = true;
        }
        else if(string_ends_with(name, "ES3_compatibility"))
        {
            features->texture_compression_etc2 = true;
        }
        else if(string_ends_with(name, "texture_compression_s3tc")
                || string_ends_with(name, "compressed_texture_s3tc")
                || string_ends_with(name, "texture_compression_dxt1"))
        {
            features->texture_compression_s3tc = true;
        }
    }
}

static void set_up_capabilities(Capabilities* capabilities, Features* features)
{
    capabilities->max_anisotropy = 1;
    if(features->sampler_filter_anisotropic)
    {
        glGetIntegerv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &capabilities->max_anisotropy);
    }
}

Backend* create_backend(Heap* heap)
{
    Backend* backend = HEAP_ALLOCATE(heap, Backend, 1);

    create_id_pool(&backend->buffer_id_pool, heap, 32);
    create_id_pool(&backend->image_id_pool, heap, 8);
    create_id_pool(&backend->pass_id_pool, heap, 1);
    create_id_pool(&backend->pipeline_id_pool, heap, 16);
    create_id_pool(&backend->sampler_id_pool, heap, 4);
    create_id_pool(&backend->shader_id_pool, heap, 16);

    backend->buffers = HEAP_ALLOCATE(heap, Buffer, backend->buffer_id_pool.cap);
    backend->images = HEAP_ALLOCATE(heap, Image, backend->image_id_pool.cap);
    backend->passes = HEAP_ALLOCATE(heap, Pass, backend->pass_id_pool.cap);
    backend->pipelines = HEAP_ALLOCATE(heap, Pipeline, backend->pipeline_id_pool.cap);
    backend->samplers = HEAP_ALLOCATE(heap, Sampler, backend->sampler_id_pool.cap);
    backend->shaders = HEAP_ALLOCATE(heap, Shader, backend->shader_id_pool.cap);

    set_up_features(&backend->features);
    set_up_capabilities(&backend->capabilities, &backend->features);

#if defined(PROFILE_CORE_3_3)
    glEnable(GL_FRAMEBUFFER_SRGB);
#endif

    return backend;
}

void destroy_backend(Backend* backend, Heap* heap)
{
    for(int i = 0; i < backend->buffer_id_pool.cap; i += 1)
    {
        Buffer* buffer = &backend->buffers[i];
        if(buffer->resource.status != RESOURCE_STATUS_INVALID)
        {
            unload_buffer(buffer);
        }
    }

    for(int i = 0; i < backend->image_id_pool.cap; i += 1)
    {
        Image* image = &backend->images[i];
        if(image->resource.status != RESOURCE_STATUS_INVALID)
        {
            unload_image(image);
        }
    }

    for(int i = 0; i < backend->pass_id_pool.cap; i += 1)
    {
        Pass* pass = &backend->passes[i];
        if(pass->resource.status != RESOURCE_STATUS_INVALID)
        {
            unload_pass(pass);
        }
    }

    for(int i = 0; i < backend->pipeline_id_pool.cap; i += 1)
    {
        Pipeline* pipeline = &backend->pipelines[i];
        if(pipeline->resource.status != RESOURCE_STATUS_INVALID)
        {
            unload_pipeline(pipeline);
        }
    }

    for(int i = 0; i < backend->sampler_id_pool.cap; i += 1)
    {
        Sampler* sampler = &backend->samplers[i];
        if(sampler->resource.status != RESOURCE_STATUS_INVALID)
        {
            unload_sampler(sampler);
        }
    }

    for(int i = 0; i < backend->shader_id_pool.cap; i += 1)
    {
        Shader* shader = &backend->shaders[i];
        if(shader->resource.status != RESOURCE_STATUS_INVALID)
        {
            unload_shader(shader);
        }
    }

    destroy_id_pool(&backend->buffer_id_pool, heap);
    destroy_id_pool(&backend->image_id_pool, heap);
    destroy_id_pool(&backend->pass_id_pool, heap);
    destroy_id_pool(&backend->pipeline_id_pool, heap);
    destroy_id_pool(&backend->sampler_id_pool, heap);
    destroy_id_pool(&backend->shader_id_pool, heap);

    HEAP_DEALLOCATE(heap, backend->buffers);
    HEAP_DEALLOCATE(heap, backend->images);
    HEAP_DEALLOCATE(heap, backend->passes);
    HEAP_DEALLOCATE(heap, backend->pipelines);
    HEAP_DEALLOCATE(heap, backend->samplers);
    HEAP_DEALLOCATE(heap, backend->shaders);
}

BufferId create_buffer(Backend* backend, BufferSpec* spec, Log* log)
{
    IdPool* pool = &backend->buffer_id_pool;
    BufferId id = (BufferId){allocate_id(pool)};
    if(id.value != invalid_id)
    {
        Buffer* buffer = fetch_buffer(backend, id);
        load_buffer(buffer, spec);
        ASSERT(buffer->resource.status == RESOURCE_STATUS_VALID);
    }
    else
    {
        log_error(log, "The buffer pool is out of memory.");
    }
    return id;
}

ImageId create_image(Backend* backend, ImageSpec* spec, Log* log)
{
    IdPool* pool = &backend->image_id_pool;
    ImageId id = (ImageId){allocate_id(pool)};
    if(id.value != invalid_id)
    {
        Image* image = fetch_image(backend, id);
        load_image(image, spec, backend);
        ASSERT(image->resource.status == RESOURCE_STATUS_VALID);
    }
    else
    {
        log_error(log, "The image pool is out of memory.");
    }
    return id;
}

PassId create_pass(Backend* backend, PassSpec* spec, Log* log)
{
    IdPool* pool = &backend->pass_id_pool;
    PassId id = (PassId){allocate_id(pool)};
    if(id.value != invalid_id)
    {
        Pass* pass = fetch_pass(backend, id);
        load_pass(pass, spec, backend);
        ASSERT(pass->resource.status == RESOURCE_STATUS_VALID);
    }
    else
    {
        log_error(log, "The pass pool is out of memory.");
    }
    return id;
}

PipelineId create_pipeline(Backend* backend, PipelineSpec* spec, Log* log)
{
    IdPool* pool = &backend->pipeline_id_pool;
    PipelineId id = (PipelineId){allocate_id(pool)};
    if(id.value != invalid_id)
    {
        Pipeline* pipeline = fetch_pipeline(backend, id);
        load_pipeline(pipeline, spec);
        ASSERT(pipeline->resource.status == RESOURCE_STATUS_VALID);
    }
    else
    {
        log_error(log, "The pipeline pool is out of memory.");
    }
    return id;
}

SamplerId create_sampler(Backend* backend, SamplerSpec* spec, Log* log)
{
    IdPool* pool = &backend->sampler_id_pool;
    SamplerId id = (SamplerId){allocate_id(pool)};
    if(id.value != invalid_id)
    {
        Sampler* sampler = fetch_sampler(backend, id);
        load_sampler(sampler, spec, backend);
        ASSERT(sampler->resource.status == RESOURCE_STATUS_VALID);
    }
    else
    {
        log_error(log, "The sampler pool is out of memory.");
    }
    return id;
}

ShaderId create_shader(Backend* backend, ShaderSpec* spec, Heap* heap, Log* log)
{
    IdPool* pool = &backend->shader_id_pool;
    ShaderId id = (ShaderId){allocate_id(pool)};
    if(id.value != invalid_id)
    {
        Shader* shader = fetch_shader(backend, id);
        load_shader(shader, spec, heap, log);
        ASSERT(shader->resource.status == RESOURCE_STATUS_VALID);
    }
    else
    {
        log_error(log, "The shader pool is out of memory!");
    }
    return id;
}

void destroy_buffer(Backend* backend, BufferId id)
{
    Buffer* buffer = fetch_buffer(backend, id);
    if(buffer)
    {
        unload_buffer(buffer);
        deallocate_id(&backend->buffer_id_pool, id.value);
        ASSERT(buffer->resource.status == RESOURCE_STATUS_INVALID);
    }
}

void destroy_image(Backend* backend, ImageId id)
{
    Image* image = fetch_image(backend, id);
    if(image)
    {
        unload_image(image);
        deallocate_id(&backend->image_id_pool, id.value);
        ASSERT(image->resource.status == RESOURCE_STATUS_INVALID);
    }
}

void destroy_pass(Backend* backend, PassId id)
{
    Pass* pass = fetch_pass(backend, id);
    if(pass)
    {
        unload_pass(pass);
        deallocate_id(&backend->pass_id_pool, id.value);
        ASSERT(pass->resource.status == RESOURCE_STATUS_INVALID);
    }
}

void destroy_pipeline(Backend* backend, PipelineId id)
{
    Pipeline* pipeline = fetch_pipeline(backend, id);
    if(pipeline)
    {
        unload_pipeline(pipeline);
        deallocate_id(&backend->pipeline_id_pool, id.value);
        ASSERT(pipeline->resource.status == RESOURCE_STATUS_INVALID);
    }
}

void destroy_sampler(Backend* backend, SamplerId id)
{
    Sampler* sampler = fetch_sampler(backend, id);
    if(sampler)
    {
        unload_sampler(sampler);
        deallocate_id(&backend->sampler_id_pool, id.value);
        ASSERT(sampler->resource.status == RESOURCE_STATUS_INVALID);
    }
}

void destroy_shader(Backend* backend, ShaderId id)
{
    Shader* shader = fetch_shader(backend, id);
    if(shader)
    {
        unload_shader(shader);
        deallocate_id(&backend->shader_id_pool, id.value);
        ASSERT(shader->resource.status == RESOURCE_STATUS_INVALID);
    }
}

static bool get_boolean(GLenum name)
{
    GLboolean value;
    glGetBooleanv(name, &value);
    return value;
}

static bool check_all_set(GLenum name)
{
    GLboolean b[4];
    glGetBooleanv(name, b);
    return b[0] && b[1] && b[2] && b[3];
}

void clear_target(Backend* backend, ClearState* clear_state)
{
    (void) backend;

    GLbitfield mask = 0;

    if(clear_state->flags.colour)
    {
        ASSERT(check_all_set(GL_COLOR_WRITEMASK));
        mask |= GL_COLOR_BUFFER_BIT;
        glClearColor(clear_state->colour[0], clear_state->colour[1], clear_state->colour[2], clear_state->colour[3]);
    }
    if(clear_state->flags.depth)
    {
        ASSERT(get_boolean(GL_DEPTH_WRITEMASK));
        mask |= GL_DEPTH_BUFFER_BIT;
#if defined(PROFILE_CORE_3_3)
        glClearDepth(clear_state->depth);
#elif defined(PROFILE_ES_3)
        glClearDepthf(clear_state->depth);
#endif
    }
    if(clear_state->flags.stencil)
    {
        ASSERT(get_boolean(GL_STENCIL_WRITEMASK));
        mask |= GL_STENCIL_BUFFER_BIT;
        glClearStencil(clear_state->stencil);
    }

    ASSERT(mask);
    glClear(mask);
}

void draw(Backend* backend, DrawAction* draw_action)
{
    Pipeline* pipeline = backend->current_pipeline;

    glBindVertexArray(pipeline->vertex_array);

    for(int i = 0; i < VERTEX_ATTRIBUTE_CAP; i += 1)
    {
        VertexAttribute* attribute = &pipeline->attributes[i];
        if(attribute->buffer_index == invalid_index)
        {
            break;
        }

        Buffer* buffer = fetch_buffer(backend, draw_action->vertex_buffers[attribute->buffer_index]);
        ASSERT(buffer);

        glBindBuffer(GL_ARRAY_BUFFER, buffer->id);
        glEnableVertexAttribArray(i);
        glVertexAttribPointer(i, attribute->size, attribute->type, attribute->normalised, attribute->stride, (const GLvoid*) (uintptr_t) attribute->offset);
    }

    GLenum mode = translate_primitive_topology(pipeline->input_assembly.primitive_topology);
    if(pipeline->input_assembly.index_type == INDEX_TYPE_NONE)
    {
        glDrawArrays(mode, 0, draw_action->indices_count);
    }
    else
    {
        Buffer* index_buffer = fetch_buffer(backend, draw_action->index_buffer);
        ASSERT(index_buffer);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer->id);

        GLenum index_type = translate_index_type(pipeline->input_assembly.index_type);
        glDrawElements(mode, draw_action->indices_count, index_type, NULL);
    }
}

void set_images(Backend* backend, ImageSet* image_set)
{
    Pipeline* pipeline = backend->current_pipeline;
    Shader* shader = fetch_shader(backend, pipeline->shader);

    for(int i = 0; i < 2; i += 1)
    {
        ShaderStage* stage = &shader->stages[i];
        ShaderStageImageSet* stage_set = &image_set->stages[i];

        for(int j = 0; j < SHADER_STAGE_IMAGE_CAP; j += 1)
        {
            ImageId image_id = stage_set->images[j];
            SamplerId sampler_id = stage_set->samplers[j];
            if(image_id.value == invalid_id)
            {
                break;
            }

            ShaderImage* shader_image = &stage->images[j];
            Image* image = fetch_image(backend, image_id);
            Sampler* sampler = fetch_sampler(backend, sampler_id);

            glActiveTexture(GL_TEXTURE0 + shader_image->texture_slot);
            glBindTexture(image->target, image->texture);
            glBindSampler(shader_image->texture_slot, sampler->handle);
        }
    }
}

void set_pass(Backend* backend, PassId id)
{
    Pass* pass = fetch_pass(backend, id);

    if(pass)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, pass->framebuffer);

        GLenum attachments[PASS_COLOUR_ATTACHMENT_CAP] =
        {
            GL_COLOR_ATTACHMENT0,
            GL_COLOR_ATTACHMENT1,
            GL_COLOR_ATTACHMENT2,
            GL_COLOR_ATTACHMENT3
        };
        int total_attachments = 0;
        for(int i = 0; i < PASS_COLOUR_ATTACHMENT_CAP; i += 1)
        {
            Attachment* attachment = &pass->colour_attachments[i];
            Image* image = fetch_image(backend, attachment->image);
            if(!image)
            {
                break;
            }
            total_attachments += 1;
        }
        glDrawBuffers(total_attachments, attachments);
    }
    else
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
}

static void set_blend_state(BlendState* state, BlendState* prior)
{
    if(!prior || state->enabled != prior->enabled)
    {
        if(state->enabled)
        {
            glEnable(GL_BLEND);
        }
        else
        {
            glDisable(GL_BLEND);
        }
    }

    if(!prior
            || state->rgb_op != prior->rgb_op
            || state->alpha_op != prior->alpha_op)
    {
        GLenum rgb_mode = get_blend_op(state->rgb_op);
        GLenum alpha_mode = get_blend_op(state->alpha_op);
        glBlendEquationSeparate(rgb_mode, alpha_mode);
    }

    if(!prior
            || state->alpha_destination_factor != prior->alpha_destination_factor
            || state->alpha_source_factor != prior->alpha_source_factor
            || state->rgb_destination_factor != prior->rgb_destination_factor
            || state->rgb_source_factor != prior->rgb_source_factor)
    {
        GLenum alpha_destination = get_blend_factor(state->alpha_destination_factor);
        GLenum alpha_source = get_blend_factor(state->alpha_source_factor);
        GLenum rgb_destination = get_blend_factor(state->rgb_destination_factor);
        GLenum rgb_source = get_blend_factor(state->rgb_source_factor);
        glBlendFuncSeparate(rgb_source, rgb_destination, alpha_source, alpha_destination);
    }

    float colour[4] =
    {
        state->constant_colour[0],
        state->constant_colour[1],
        state->constant_colour[2],
        state->constant_colour[3],
    };
    if(!prior
            || colour[0] != prior->constant_colour[0]
            || colour[1] != prior->constant_colour[1]
            || colour[2] != prior->constant_colour[2]
            || colour[3] != prior->constant_colour[3])
    {
        glBlendColor(colour[0], colour[1], colour[2], colour[3]);
    }

    ColourComponentFlags flags = state->colour_write_flags;
    if(!prior
            || flags.r != prior->colour_write_flags.r
            || flags.g != prior->colour_write_flags.g
            || flags.b != prior->colour_write_flags.b
            || flags.a != prior->colour_write_flags.a
            || flags.disable_all != prior->colour_write_flags.disable_all)
    {
        if(flags.disable_all)
        {
            glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
        }
        else
        {
            glColorMask(flags.r, flags.g, flags.b, flags.a);
        }
    }
}

static void set_stencil_state(StencilOpState* state, StencilOpState* prior, GLenum face)
{
    if(!prior
            || state->compare_op != prior->compare_op
            || state->reference != prior->reference
            || state->compare_mask != prior->compare_mask)
    {
        GLenum compare = get_compare_op(state->compare_op);
        glStencilFuncSeparate(face, compare, state->reference, state->compare_mask);
    }

    if(!prior
            || state->fail_op != prior->fail_op
            || state->depth_fail_op != prior->depth_fail_op
            || state->pass_op != prior->pass_op)
    {
        GLenum fail = get_stencil_op(state->fail_op);
        GLenum depth_fail = get_stencil_op(state->depth_fail_op);
        GLenum pass = get_stencil_op(state->pass_op);
        glStencilOpSeparate(face, fail, depth_fail, pass);
    }

    if(!prior || state->write_mask != prior->write_mask)
    {
        glStencilMaskSeparate(face, state->write_mask);
    }
}

static void set_depth_stencil_state(DepthStencilState* state, DepthStencilState* prior)
{
    if(!prior || state->depth_compare_enabled != prior->depth_compare_enabled)
    {
        if(state->depth_compare_enabled)
        {
            glEnable(GL_DEPTH_TEST);
        }
        else
        {
            glDisable(GL_DEPTH_TEST);
        }
    }

    if(!prior || state->depth_write_enabled != prior->depth_write_enabled)
    {
        glDepthMask(state->depth_write_enabled);
    }

    if(!prior || state->depth_compare_op != prior->depth_compare_op)
    {
        GLenum depth_op = get_compare_op(state->depth_compare_op);
        glDepthFunc(depth_op);
    }

    if(!prior || state->stencil_enabled != prior->stencil_enabled)
    {
        if(state->stencil_enabled)
        {
            glEnable(GL_STENCIL_TEST);
        }
        else
        {
            glDisable(GL_STENCIL_TEST);
        }
    }

    StencilOpState* back = NULL;
    StencilOpState* front = NULL;
    if(prior)
    {
        back = &prior->back_stencil;
        front = &prior->front_stencil;
    }
    set_stencil_state(&state->back_stencil, back, GL_BACK);
    set_stencil_state(&state->front_stencil, front, GL_FRONT);
}

static void set_rasterizer_state(RasterizerState* state, RasterizerState* prior, Backend* backend)
{
    if(!prior || state->cull_mode != prior->cull_mode)
    {
        bool was_enabled = prior && prior->cull_mode != CULL_MODE_NONE;
        bool enabled = state->cull_mode != CULL_MODE_NONE;

        if(was_enabled != enabled)
        {
            if(enabled)
            {
                glDisable(GL_CULL_FACE);
            }
            else
            {
                glEnable(GL_CULL_FACE);
            }
        }

        if(enabled)
        {
            GLenum mode = get_cull_mode(state->cull_mode);
            glCullFace(mode);
        }
    }

    if(!prior || state->face_winding != prior->face_winding)
    {
        GLenum winding = get_face_winding(state->face_winding);
        glFrontFace(winding);
    }

    if(!prior || state->depth_bias_enabled != prior->depth_bias_enabled)
    {
        if(state->depth_bias_enabled)
        {
            glEnable(GL_POLYGON_OFFSET_FILL);
        }
        else
        {
            glDisable(GL_POLYGON_OFFSET_FILL);
        }
    }

    if(!prior
            || state->depth_bias_constant != prior->depth_bias_constant
            || state->depth_bias_slope != prior->depth_bias_slope)
    {
        glPolygonOffset(state->depth_bias_slope, state->depth_bias_constant);
    }
}

void set_pipeline(Backend* backend, PipelineId id)
{
    Pipeline* pipeline = fetch_pipeline(backend, id);
    Pipeline* prior = backend->current_pipeline;
    if(pipeline != prior)
    {
        Shader* shader = fetch_shader(backend, pipeline->shader);
        glUseProgram(shader->program);

        BlendState* blend = NULL;
        DepthStencilState* depth_stencil = NULL;
        RasterizerState* rasterizer = NULL;
        if(prior)
        {
            blend = &prior->blend;
            depth_stencil = &prior->depth_stencil;
            rasterizer = &prior->rasterizer;
        }
        set_blend_state(&pipeline->blend, blend);
        set_depth_stencil_state(&pipeline->depth_stencil, depth_stencil);
        set_rasterizer_state(&pipeline->rasterizer, rasterizer, backend);

        backend->current_pipeline = pipeline;
    }
}

void set_scissor_rect(Backend* backend, ScissorRect* rect)
{
    (void) backend;

    if(rect)
    {
        glEnable(GL_SCISSOR_TEST);

        int x = rect->bottom_left.x;
        int y = rect->bottom_left.y;
        int width = rect->dimensions.x;
        int height = rect->dimensions.y;
        glScissor(x, y, width, height);
    }
    else
    {
        glDisable(GL_SCISSOR_TEST);
    }
}

void set_viewport(Backend* backend, Viewport* viewport)
{
    (void) backend;

    glViewport(viewport->bottom_left.x, viewport->bottom_left.y, viewport->dimensions.x, viewport->dimensions.y);

#if defined(PROFILE_CORE_3_3)
    glDepthRange(viewport->near_depth, viewport->far_depth);
#elif defined(PROFILE_ES_3)
    glDepthRangef(viewport->near_depth, viewport->far_depth);
#endif
}

void update_buffer(Backend* backend, BufferId id, const void* memory, int base, int size)
{
    if(!size)
    {
        return;
    }
    Buffer* buffer = fetch_buffer(backend, id);
    ASSERT(buffer);
    copy_to_buffer(buffer, memory, base, size);
}
