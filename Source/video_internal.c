#include "video_internal.h"

#include "int_utilities.h"

#include <math.h>

const PassId default_pass = {0};

int count_mip_levels(int width, int height)
{
    return (int) floor(log2(imax(width, height))) + 1;
}

int get_vertex_format_component_count(VertexFormat format)
{
    switch(format)
    {
        case VERTEX_FORMAT_FLOAT1:
            return 1;
        case VERTEX_FORMAT_FLOAT2:
        case VERTEX_FORMAT_USHORT2_NORM:
            return 2;
        case VERTEX_FORMAT_FLOAT3:
            return 3;
        case VERTEX_FORMAT_FLOAT4:
        case VERTEX_FORMAT_UBYTE4_NORM:
            return 4;
        default:
            return 0;
    }
}

int get_vertex_format_size(VertexFormat format)
{
    switch(format)
    {
        case VERTEX_FORMAT_FLOAT1:       return sizeof(float);
        case VERTEX_FORMAT_FLOAT2:       return sizeof(float) * 2;
        case VERTEX_FORMAT_FLOAT3:       return sizeof(float) * 3;
        case VERTEX_FORMAT_FLOAT4:       return sizeof(float) * 4;
        case VERTEX_FORMAT_UBYTE4_NORM:  return sizeof(uint8_t) * 4;
        case VERTEX_FORMAT_USHORT2_NORM: return sizeof(uint16_t) * 2;
        default:                         return 0;
    }
}

bool is_pixel_format_compressed(PixelFormat pixel_format)
{
    switch(pixel_format)
    {
        default:
        case PIXEL_FORMAT_RGBA8:
        case PIXEL_FORMAT_RGBA16F:
        case PIXEL_FORMAT_RGBA32F:
        case PIXEL_FORMAT_RGB8:
        case PIXEL_FORMAT_RGB8_SNORM:
        case PIXEL_FORMAT_RGB16F:
        case PIXEL_FORMAT_RGB32F:
        case PIXEL_FORMAT_RG8:
        case PIXEL_FORMAT_RG8_SNORM:
        case PIXEL_FORMAT_RG16F:
        case PIXEL_FORMAT_RG32F:
        case PIXEL_FORMAT_R8:
        case PIXEL_FORMAT_R16F:
        case PIXEL_FORMAT_R32F:
        case PIXEL_FORMAT_DEPTH16:
        case PIXEL_FORMAT_DEPTH24:
        case PIXEL_FORMAT_DEPTH32F:
        case PIXEL_FORMAT_DEPTH24_STENCIL8:
            return false;
        case PIXEL_FORMAT_ETC2_RGB8:
        case PIXEL_FORMAT_ETC2_SRGB8:
        case PIXEL_FORMAT_S3TC_DXT1:
        case PIXEL_FORMAT_S3TC_DXT3:
        case PIXEL_FORMAT_S3TC_DXT5:
            return true;
    }
}

bool is_pixel_format_depth_only(PixelFormat pixel_format)
{
    switch(pixel_format)
    {
        case PIXEL_FORMAT_DEPTH16:
        case PIXEL_FORMAT_DEPTH24:
        case PIXEL_FORMAT_DEPTH32F:
            return true;
        default:
            return false;
    }
}

void create_backend(Backend* backend, PlatformVideo* platform, Heap* heap)
{
    backend->create_backend(backend, platform, heap);
}

void destroy_backend(Backend* backend, Heap* heap)
{
    backend->destroy_backend(backend, heap);
}

BufferId create_buffer(Backend* backend, BufferSpec* spec, Log* log)
{
    return backend->create_buffer(backend, spec, log);
}

ImageId create_image(Backend* backend, ImageSpec* spec, Log* log)
{
    return backend->create_image(backend, spec, log);
}

PassId create_pass(Backend* backend, PassSpec* spec, Log* log)
{
    return backend->create_pass(backend, spec, log);
}

PipelineId create_pipeline(Backend* backend, PipelineSpec* spec, Log* log)
{
    return backend->create_pipeline(backend, spec, log);
}

SamplerId create_sampler(Backend* backend, SamplerSpec* spec, Log* log)
{
    return backend->create_sampler(backend, spec, log);
}

ShaderId create_shader(Backend* backend, ShaderSpec* spec, Heap* heap, Log* log)
{
    return backend->create_shader(backend, spec, heap, log);
}

void destroy_buffer(Backend* backend, BufferId id)
{
    backend->destroy_buffer(backend, id);
}

void destroy_image(Backend* backend, ImageId id)
{
    backend->destroy_image(backend, id);
}

void destroy_pass(Backend* backend, PassId id)
{
    backend->destroy_pass(backend, id);
}

void destroy_pipeline(Backend* backend, PipelineId id)
{
    backend->destroy_pipeline(backend, id);
}

void destroy_sampler(Backend* backend, SamplerId id)
{
    backend->destroy_sampler(backend, id);
}

void destroy_shader(Backend* backend, ShaderId id)
{
    backend->destroy_shader(backend, id);
}

void blit_pass_colour(Backend* backend, PassId source_id, PassId target_id)
{
    backend->blit_pass_colour(backend, source_id, target_id);
}

void clear_target(Backend* backend, ClearState* clear_state)
{
    backend->clear_target(backend, clear_state);
}

void draw(Backend* backend, DrawAction* draw_action)
{
    backend->draw(backend, draw_action);
}

void resize_swap_buffers(Backend* backend, Int2 dimensions)
{
    backend->resize_swap_buffers(backend, dimensions);
}

void set_images(Backend* backend, ImageSet* image_set)
{
    backend->set_images(backend, image_set);
}

void set_pass(Backend* backend, PassId id)
{
    backend->set_pass(backend, id);
}

void set_pipeline(Backend* backend, PipelineId id)
{
    backend->set_pipeline(backend, id);
}

void set_scissor_rect(Backend* backend, ScissorRect* scissor_rect)
{
    backend->set_scissor_rect(backend, scissor_rect);
}

void set_viewport(Backend* backend, Viewport* viewport)
{
    backend->set_viewport(backend, viewport);
}

void swap_buffers(Backend* backend, PlatformVideo* video)
{
    backend->swap_buffers(backend, video);
}

void update_buffer(Backend* backend, BufferId id, const void* memory, int base, int size)
{
    backend->update_buffer(backend, id, memory, base, size);
}
