#ifndef VERTEX_LAYOUT_H_
#define VERTEX_LAYOUT_H_

#include "vector_math.h"
#include "sized_types.h"

struct VertexPNC
{
    Float3 position;
    Float3 normal;
    u32 colour;
};

struct VertexPC
{
    Float3 position;
    u32 colour;
};

struct VertexPT
{
    Float3 position;
    u32 texcoord;
};

struct PointVertex
{
    Float3 position;
    Float2 direction;
    u32 colour;
    u32 texcoord;
};

struct LineVertex
{
    Float3 position;
    Float3 direction;
    u32 colour;
    u32 texcoord;
    float side;
};

u32 rgb_to_u32(Float3 c);
u32 rgba_to_u32(Float4 c);
Float4 u32_to_rgba(u32 u);
Float3 u32_to_rgb(u32 u);

u32 texcoord_to_u32(Float2 v);

#endif // VERTEX_LAYOUT_H_
