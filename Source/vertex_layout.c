#include "vertex_layout.h"

#include "assert.h"

typedef union Pack4x8
{
    struct
    {
        uint8_t r, g, b, a;
    };
    uint32_t packed;
} Pack4x8;

static uint32_t pack_unorm3x8(Float3 v)
{
    Pack4x8 u;
    u.r = (uint8_t) (0xff * v.x);
    u.g = (uint8_t) (0xff * v.y);
    u.b = (uint8_t) (0xff * v.z);
    u.a = 0xff;
    return u.packed;
}

static Float3 unpack_unorm3x8(uint32_t x)
{
    Pack4x8 u;
    u.packed = x;
    Float3 v;
    v.x = u.r / 255.0f;
    v.y = u.g / 255.0f;
    v.z = u.b / 255.0f;
    return v;
}

static uint32_t pack_unorm4x8(Float4 v)
{
    Pack4x8 u;
    u.r = (uint8_t) (0xff * v.x);
    u.g = (uint8_t) (0xff * v.y);
    u.b = (uint8_t) (0xff * v.z);
    u.a = (uint8_t) (0xff * v.w);
    return u.packed;
}

static Float4 unpack_unorm4x8(uint32_t x)
{
    Pack4x8 u;
    u.packed = x;
    Float4 v;
    v.x = u.r / 255.0f;
    v.y = u.g / 255.0f;
    v.z = u.b / 255.0f;
    v.w = u.a / 255.0f;
    return v;
}

static uint32_t pack_unorm16x2(Float2 v)
{
    union
    {
        struct
        {
            uint16_t x;
            uint16_t y;
        };
        uint32_t packed;
    } u;
    u.x = (uint16_t) (0xffff * v.x);
    u.y = (uint16_t) (0xffff * v.y);
    return u.packed;
}

static bool is_unorm(float x)
{
    return x >= 0.0f && x <= 1.0f;
}

uint32_t rgb_to_u32(Float3 c)
{
    ASSERT(is_unorm(c.x));
    ASSERT(is_unorm(c.y));
    ASSERT(is_unorm(c.z));
    return pack_unorm3x8(c);
}

uint32_t rgba_to_u32(Float4 c)
{
    ASSERT(is_unorm(c.x));
    ASSERT(is_unorm(c.y));
    ASSERT(is_unorm(c.z));
    ASSERT(is_unorm(c.w));
    return pack_unorm4x8(c);
}

Float3 u32_to_rgb(uint32_t u)
{
    return unpack_unorm3x8(u);
}

Float4 u32_to_rgba(uint32_t u)
{
    return unpack_unorm4x8(u);
}

uint32_t texcoord_to_u32(Float2 v)
{
    ASSERT(is_unorm(v.x));
    ASSERT(is_unorm(v.y));
    return pack_unorm16x2(v);
}
