#include "video_internal.h"

#include "int_utilities.h"

#include <math.h>

int count_mip_levels(int width, int height)
{
    return (int) floor(log2(imax(width, height))) + 1;
}

int get_vertex_format_component_count(VertexFormat format)
{
    switch(format)
    {
        case VERTEX_FORMAT_FLOAT2:  return 2;
        case VERTEX_FORMAT_FLOAT3:  return 3;
        case VERTEX_FORMAT_FLOAT4:  return 4;
        default:                    return 0;
    }
}

int get_vertex_format_size(VertexFormat format)
{
    switch(format)
    {
        case VERTEX_FORMAT_FLOAT2:  return sizeof(float) * 2;
        case VERTEX_FORMAT_FLOAT3:  return sizeof(float) * 3;
        case VERTEX_FORMAT_FLOAT4:  return sizeof(float) * 4;
        default:                    return 0;
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
        case PIXEL_FORMAT_RGB16F:
        case PIXEL_FORMAT_RGB32F:
        case PIXEL_FORMAT_RG8:
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
