#ifndef VERTEX_LAYOUT_H_
#define VERTEX_LAYOUT_H_

#if defined(__cplusplus)
extern "C" {
#endif

#include "vector_math.h"

#include <stdint.h>

typedef struct VertexPNC
{
    Float3 position;
    Float3 normal;
    uint32_t colour;
} VertexPNC;

typedef struct VertexPC
{
    Float3 position;
    uint32_t colour;
} VertexPC;

typedef struct VertexPT
{
    Float3 position;
    uint32_t texcoord;
} VertexPT;

typedef struct PointVertex
{
    Float3 position;
    Float2 direction;
    uint32_t colour;
    uint32_t texcoord;
} PointVertex;

typedef struct LineVertex
{
    Float3 position;
    Float3 direction;
    uint32_t colour;
    uint32_t texcoord;
    float side;
} LineVertex;

uint32_t rgb_to_u32(Float3 c);
uint32_t rgba_to_u32(Float4 c);
Float4 u32_to_rgba(uint32_t u);
Float3 u32_to_rgb(uint32_t u);

uint32_t texcoord_to_u32(Float2 v);

#if defined(__cplusplus)
} // extern "C"
#endif

#endif // VERTEX_LAYOUT_H_
