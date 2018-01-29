#include "vertex_layout.h"

#include "assert.h"

union Pack4x8
{
	struct
	{
		u8 r, g, b, a;
	};
	u32 packed;
};

static u32 pack_unorm3x8(Vector3 v)
{
	Pack4x8 u;
	u.r = 0xff * v.x;
	u.g = 0xff * v.y;
	u.b = 0xff * v.z;
	u.a = 0xff;
	return u.packed;
}

static u32 pack_unorm4x8(Vector4 v)
{
	Pack4x8 u;
	u.r = 0xff * v.x;
	u.g = 0xff * v.y;
	u.b = 0xff * v.z;
	u.a = 0xff * v.w;
	return u.packed;
}

static u32 pack_unorm16x2(Vector2 v)
{
	union
	{
		struct
		{
			u16 x;
			u16 y;
		};
		u32 packed;
	} u;
	u.x = 0xffff * v.x;
	u.y = 0xffff * v.y;
	return u.packed;
}

static bool is_unorm(float x)
{
	return x >= 0.0f && x <= 1.0f;
}

u32 rgb_to_u32(Vector3 c)
{
	ASSERT(is_unorm(c.x));
	ASSERT(is_unorm(c.y));
	ASSERT(is_unorm(c.z));
	return pack_unorm3x8(c);
}

u32 rgba_to_u32(Vector4 c)
{
	ASSERT(is_unorm(c.x));
	ASSERT(is_unorm(c.y));
	ASSERT(is_unorm(c.z));
	ASSERT(is_unorm(c.w));
	return pack_unorm4x8(c);
}

u32 texcoord_to_u32(Vector2 v)
{
	ASSERT(is_unorm(v.x));
	ASSERT(is_unorm(v.y));
	return pack_unorm16x2(v);
}
