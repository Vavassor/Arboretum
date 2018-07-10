#ifndef UNIFORM_BLOCKS_H_
#define UNIFORM_BLOCKS_H_

#include "vector_math.h"

typedef struct HaloBlock
{
    Float4 halo_colour;
} HaloBlock;

typedef struct LightBlock
{
    Float3 light_direction;
} LightBlock;

typedef struct PerImage
{
    Float2 texture_dimensions;
} PerImage;

typedef struct PerLine
{
    float line_width;
    float projection_factor;
} PerLine;

typedef struct PerObject
{
    Matrix4 model;
    Matrix4 normal_matrix;
} PerObject;

typedef struct PerPoint
{
    float point_radius;
    float projection_factor;
} PerPoint;

typedef struct PerSpan
{
    Float3 tint;
    float padding;
} PerSpan;

typedef struct PerView
{
    Matrix4 view_projection;
    Float2 viewport_dimensions;
} PerView;

#endif // UNIFORM_BLOCKS_H_
