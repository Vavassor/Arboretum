#ifndef VIDEO_INTERNAL_H_
#define VIDEO_INTERNAL_H_

#include <stdbool.h>
#include <stdint.h>

#include "memory.h"
#include "int2.h"
#include "log.h"

#define PROFILE_CORE_3_3
// #define PROFILE_ES_3

#define MIPMAP_CAP 16
#define PASS_COLOUR_ATTACHMENT_CAP 4
#define SHADER_STAGE_BUFFER_CAP 4
#define SHADER_STAGE_UNIFORM_BLOCK_CAP 4
#define SHADER_STAGE_IMAGE_CAP 4
#define VERTEX_ATTRIBUTE_CAP 8

typedef enum BlendFactor
{
    BLEND_FACTOR_INVALID,
    BLEND_FACTOR_CONSTANT_ALPHA,
    BLEND_FACTOR_CONSTANT_COLOUR,
    BLEND_FACTOR_DST_ALPHA,
    BLEND_FACTOR_DST_COLOUR,
    BLEND_FACTOR_ONE,
    BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA,
    BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOUR,
    BLEND_FACTOR_ONE_MINUS_DST_ALPHA,
    BLEND_FACTOR_ONE_MINUS_DST_COLOUR,
    BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
    BLEND_FACTOR_ONE_MINUS_SRC_COLOUR,
    BLEND_FACTOR_SRC_ALPHA,
    BLEND_FACTOR_SRC_ALPHA_SATURATED,
    BLEND_FACTOR_SRC_COLOUR,
    BLEND_FACTOR_ZERO,
} BlendFactor;

typedef enum BlendOp
{
    BLEND_OP_INVALID,
    BLEND_OP_ADD,
    BLEND_OP_REVERSE_SUBTRACT,
    BLEND_OP_SUBTRACT,
} BlendOp;

typedef enum BufferFormat
{
    BUFFER_FORMAT_INVALID,
    BUFFER_FORMAT_INDEX,
    BUFFER_FORMAT_UNIFORM,
    BUFFER_FORMAT_VERTEX,
} BufferFormat;

typedef enum BufferUsage
{
    BUFFER_USAGE_INVALID,
    BUFFER_USAGE_STATIC,
    BUFFER_USAGE_DYNAMIC,
} BufferUsage;

typedef enum CompareOp
{
    COMPARE_OP_INVALID,
    COMPARE_OP_ALWAYS,
    COMPARE_OP_EQUAL,
    COMPARE_OP_GREATER,
    COMPARE_OP_GREATER_OR_EQUAL,
    COMPARE_OP_LESS,
    COMPARE_OP_LESS_OR_EQUAL,
    COMPARE_OP_NEVER,
    COMPARE_OP_NOT_EQUAL,
} CompareOp;

typedef enum CubeFace
{
    CUBE_FACE_X_NEGATIVE,
    CUBE_FACE_X_POSITIVE,
    CUBE_FACE_Y_NEGATIVE,
    CUBE_FACE_Y_POSITIVE,
    CUBE_FACE_Z_NEGATIVE,
    CUBE_FACE_Z_POSITIVE,
    CUBE_FACE_COUNT,
} CubeFace;

typedef enum CullMode
{
    CULL_MODE_INVALID,
    CULL_MODE_NONE,
    CULL_MODE_BACK,
    CULL_MODE_FRONT,
    CULL_MODE_FRONT_AND_BACK,
} CullMode;

typedef enum FaceWinding
{
    FACE_WINDING_INVALID,
    FACE_WINDING_CLOCKWISE,
    FACE_WINDING_COUNTERCLOCKWISE,
} FaceWinding;

typedef enum ImageType
{
    IMAGE_TYPE_INVALID,
    IMAGE_TYPE_2D,
    IMAGE_TYPE_3D,
    IMAGE_TYPE_ARRAY,
    IMAGE_TYPE_CUBE,
} ImageType;

typedef enum IndexType
{
    INDEX_TYPE_INVALID,
    INDEX_TYPE_NONE,
    INDEX_TYPE_UINT16,
    INDEX_TYPE_UINT32,
} IndexType;

typedef enum PixelFormat
{
    PIXEL_FORMAT_INVALID,
    PIXEL_FORMAT_RGBA8,
    PIXEL_FORMAT_SRGB8_ALPHA8,
    PIXEL_FORMAT_RGBA16F,
    PIXEL_FORMAT_RGBA32F,
    PIXEL_FORMAT_RGB8,
    PIXEL_FORMAT_RGB8_SNORM,
    PIXEL_FORMAT_SRGB8,
    PIXEL_FORMAT_RGB16F,
    PIXEL_FORMAT_RGB32F,
    PIXEL_FORMAT_RG8,
    PIXEL_FORMAT_RG8_SNORM,
    PIXEL_FORMAT_RG16F,
    PIXEL_FORMAT_RG32F,
    PIXEL_FORMAT_R8,
    PIXEL_FORMAT_R16F,
    PIXEL_FORMAT_R32F,
    PIXEL_FORMAT_DEPTH16,
    PIXEL_FORMAT_DEPTH24,
    PIXEL_FORMAT_DEPTH32F,
    PIXEL_FORMAT_DEPTH24_STENCIL8,
    PIXEL_FORMAT_ETC2_RGB8,
    PIXEL_FORMAT_ETC2_SRGB8,
    PIXEL_FORMAT_S3TC_DXT1,
    PIXEL_FORMAT_S3TC_DXT3,
    PIXEL_FORMAT_S3TC_DXT5,
} PixelFormat;

typedef enum PrimitiveTopology
{
    PRIMITIVE_TOPOLOGY_INVALID,
    PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
} PrimitiveTopology;

typedef enum SamplerAddressMode
{
    SAMPLER_ADDRESS_MODE_INVALID,
    SAMPLER_ADDRESS_MODE_REPEAT,
    SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
    SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
} SamplerAddressMode;

typedef enum SamplerFilter
{
    SAMPLER_FILTER_INVALID,
    SAMPLER_FILTER_POINT,
    SAMPLER_FILTER_LINEAR,
} SamplerFilter;

typedef enum StencilOp
{
    STENCIL_OP_INVALID,
    STENCIL_OP_DECREMENT_AND_CLAMP,
    STENCIL_OP_DECREMENT_AND_WRAP,
    STENCIL_OP_INCREMENT_AND_CLAMP,
    STENCIL_OP_INCREMENT_AND_WRAP,
    STENCIL_OP_INVERT,
    STENCIL_OP_KEEP,
    STENCIL_OP_REPLACE,
    STENCIL_OP_ZERO,
} StencilOp;

typedef enum VertexFormat
{
    VERTEX_FORMAT_INVALID,
    VERTEX_FORMAT_FLOAT1,
    VERTEX_FORMAT_FLOAT2,
    VERTEX_FORMAT_FLOAT3,
    VERTEX_FORMAT_FLOAT4,
    VERTEX_FORMAT_UBYTE4_NORM,
    VERTEX_FORMAT_USHORT2_NORM,
} VertexFormat;

// Id Types.....................................................................

typedef struct BufferId {uint32_t value;} BufferId;
typedef struct ImageId {uint32_t value;} ImageId;
typedef struct PassId {uint32_t value;} PassId;
typedef struct PipelineId {uint32_t value;} PipelineId;
typedef struct SamplerId {uint32_t value;} SamplerId;
typedef struct ShaderId {uint32_t value;} ShaderId;

// BufferSpec...................................................................

typedef struct BufferSpec
{
    void* content;
    BufferFormat format;
    BufferUsage usage;
    int size;
    int binding;
} BufferSpec;

// ImageSpec....................................................................

typedef struct Subimage
{
    void* content;
    int size;
} Subimage;

typedef struct ImageContent
{
    Subimage subimages[CUBE_FACE_COUNT][MIPMAP_CAP];
} ImageContent;

typedef struct ImageSpec
{
    ImageContent content;
    ImageType type;
    PixelFormat pixel_format;
    int width;
    int height;
    int depth;
    int mipmap_count;
    bool render_target;
} ImageSpec;

// PassDesc.....................................................................

typedef struct AttachmentSpec
{
    ImageId image;
    int mip_level;
    int slice;
} AttachmentSpec;

typedef struct PassSpec
{
    AttachmentSpec colour_attachments[PASS_COLOUR_ATTACHMENT_CAP];
    AttachmentSpec depth_stencil_attachment;
} PassSpec;

// PipelineSpec.................................................................

typedef struct VertexAttributeSpec
{
    VertexFormat format;
    int buffer_index;
} VertexAttributeSpec;

typedef struct VertexLayoutSpec
{
    VertexAttributeSpec attributes[VERTEX_ATTRIBUTE_CAP];
} VertexLayoutSpec;

typedef struct ColourComponentFlags
{
    bool r : 1;
    bool g : 1;
    bool b : 1;
    bool a : 1;
    bool disable_all: 1;
} ColourComponentFlags;

typedef struct BlendStateSpec
{
    float constant_colour[4];
    BlendOp alpha_op;
    BlendOp rgb_op;
    BlendFactor alpha_destination_factor;
    BlendFactor alpha_source_factor;
    BlendFactor rgb_destination_factor;
    BlendFactor rgb_source_factor;
    PixelFormat colour_format;
    PixelFormat depth_format;
    int colour_attachment_count;
    bool enabled;
    ColourComponentFlags colour_write_flags;
} BlendStateSpec;

typedef struct StencilOpStateSpec
{
    CompareOp compare_op;
    StencilOp depth_fail_op;
    StencilOp fail_op;
    StencilOp pass_op;
    uint32_t compare_mask;
    uint32_t reference;
    uint32_t write_mask;
} StencilOpStateSpec;

typedef struct DepthStencilStateSpec
{
    StencilOpStateSpec back_stencil;
    StencilOpStateSpec front_stencil;
    CompareOp depth_compare_op;
    bool depth_compare_enabled;
    bool depth_write_enabled;
    bool stencil_enabled;
} DepthStencilStateSpec;

typedef struct InputAssemblySpec
{
    PrimitiveTopology primitive_topology;
    IndexType index_type;
} InputAssemblySpec;

typedef struct RasterizerStateSpec
{
    CullMode cull_mode;
    FaceWinding face_winding;
    float depth_bias_clamp;
    float depth_bias_constant;
    float depth_bias_slope;
    bool depth_bias_enabled;
} RasterizerStateSpec;

typedef struct PipelineSpec
{
    VertexLayoutSpec vertex_layout;
    BlendStateSpec blend;
    DepthStencilStateSpec depth_stencil;
    InputAssemblySpec input_assembly;
    RasterizerStateSpec rasterizer;
    ShaderId shader;
} PipelineSpec;

// SamplerSpec..................................................................

typedef struct SamplerSpec
{
    float min_lod;
    float max_lod;
    SamplerFilter magnify_filter;
    SamplerFilter minify_filter;
    SamplerFilter mipmap_filter;
    SamplerAddressMode address_mode_u;
    SamplerAddressMode address_mode_v;
    SamplerAddressMode address_mode_w;
    int max_anisotropy;
} SamplerSpec;

// ShaderSpec...................................................................

typedef struct UniformBlockSpec
{
    const char* name;
    int size;
    int binding;
} UniformBlockSpec;

typedef struct ShaderImageSpec
{
    const char* name;
    ImageType type;
} ShaderImageSpec;

typedef struct ShaderStageSpec
{
    const char* source;
    UniformBlockSpec uniform_blocks[SHADER_STAGE_UNIFORM_BLOCK_CAP];
    ShaderImageSpec images[SHADER_STAGE_IMAGE_CAP];
} ShaderStageSpec;

typedef struct ShaderSpec
{
    ShaderStageSpec vertex;
    ShaderStageSpec fragment;
} ShaderSpec;

// Viewport.....................................................................

typedef struct Viewport
{
    Int2 bottom_left;
    Int2 dimensions;
    float near_depth;
    float far_depth;
} Viewport;

typedef struct ScissorRect
{
    Int2 bottom_left;
    Int2 dimensions;
} ScissorRect;

// Actions......................................................................

typedef struct ShaderStageImageSet
{
    ImageId images[SHADER_STAGE_IMAGE_CAP];
    SamplerId samplers[SHADER_STAGE_IMAGE_CAP];
} ShaderStageImageSet;

typedef struct ImageSet
{
    ShaderStageImageSet stages[2];
} ImageSet;

typedef struct DrawAction
{
    BufferId vertex_buffers[SHADER_STAGE_BUFFER_CAP];
    BufferId index_buffer;
    int indices_count;
} DrawAction;

typedef struct ClearFlags
{
    bool colour : 1;
    bool depth : 1;
    bool stencil : 1;
} ClearFlags;

typedef struct ClearState
{
    float colour[4];
    float depth;
    uint32_t stencil;
    ClearFlags flags;
} ClearState;

// Functions....................................................................

int count_mip_levels(int width, int height);
int get_vertex_format_component_count(VertexFormat format);
int get_vertex_format_size(VertexFormat format);
bool is_pixel_format_compressed(PixelFormat pixel_format);

typedef struct Backend Backend;
Backend* create_backend(Heap* heap);
void destroy_backend(Backend* backend, Heap* heap);

BufferId create_buffer(Backend* backend, BufferSpec* spec, Log* log);
ImageId create_image(Backend* backend, ImageSpec* spec, Log* log);
PassId create_pass(Backend* backend, PassSpec* spec, Log* log);
PipelineId create_pipeline(Backend* backend, PipelineSpec* spec, Log* log);
SamplerId create_sampler(Backend* backend, SamplerSpec* spec, Log* log);
ShaderId create_shader(Backend* backend, ShaderSpec* spec, Heap* heap, Log* log);

void destroy_buffer(Backend* backend, BufferId id);
void destroy_image(Backend* backend, ImageId id);
void destroy_pass(Backend* backend, PassId id);
void destroy_pipeline(Backend* backend, PipelineId id);
void destroy_sampler(Backend* backend, SamplerId id);
void destroy_shader(Backend* backend, ShaderId id);

void clear_target(Backend* backend, ClearState* clear_state);
void draw(Backend* backend, DrawAction* draw_action);
void set_images(Backend* backend, ImageSet* image_set);
void set_pass(Backend* backend, PassId id);
void set_pipeline(Backend* backend, PipelineId id);
void set_scissor_rect(Backend* backend, ScissorRect* scissor_rect);
void set_viewport(Backend* backend, Viewport* viewport);
void update_buffer(Backend* backend, BufferId id, const void* memory, int base, int size);

extern const PassId default_pass;

#endif // VIDEO_INTERNAL_H_
