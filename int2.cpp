#include "int2.h"

extern const Int2 int2_zero = {0, 0};

Int2 operator + (Int2 i0, Int2 i1)
{
    Int2 result;
    result.x = i0.x + i1.x;
    result.y = i0.y + i1.y;
    return result;
}

Int2& operator += (Int2& i0, Int2 i1)
{
    i0.x += i1.x;
    i0.y += i1.y;
    return i0;
}

Int2 operator - (Int2 i0, Int2 i1)
{
    Int2 result;
    result.x = i0.x - i1.x;
    result.y = i0.y - i1.y;
    return result;
}

Int2& operator -= (Int2& i0, Int2 i1)
{
    i0.x -= i1.x;
    i0.y -= i1.y;
    return i0;
}

bool operator == (const Int2& a, const Int2& b)
{
    return a.x == b.x && a.y == b.y;
}

bool operator != (const Int2& a, const Int2& b)
{
    return a.x != b.x || a.y != b.y;
}