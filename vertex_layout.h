#ifndef VERTEX_LAYOUT_H_
#define VERTEX_LAYOUT_H_

#include "vector_math.h"
#include "sized_types.h"

struct VertexPNC
{
    Vector3 position;
    Vector3 normal;
    u32 colour;
};

struct VertexPC
{
    Vector3 position;
    u32 colour;
};

struct VertexPT
{
    Vector3 position;
    u32 texcoord;
};

struct PointVertex
{
    Vector3 position;
    Vector2 direction;
    u32 colour;
    u32 texcoord;
};

struct LineVertex
{
    Vector3 position;
    Vector3 direction;
    u32 colour;
    u32 texcoord;
    float side;
};

u32 rgb_to_u32(Vector3 c);
u32 rgba_to_u32(Vector4 c);
Vector4 u32_to_rgba(u32 u);
Vector3 u32_to_rgb(u32 u);

u32 texcoord_to_u32(Vector2 v);

#endif // VERTEX_LAYOUT_H_
