#include "int2.h"

const Int2 int2_zero = {0, 0};

Int2 int2_add(Int2 a, Int2 b)
{
    return (Int2){a.x + b.x, a.y + b.y};
}

Int2 int2_subtract(Int2 a, Int2 b)
{
    return (Int2){a.x - b.x, a.y - b.y};
}

bool int2_equals(Int2 a, Int2 b)
{
    return a.x == b.x && a.y == b.y;
}

bool int2_not_equals(Int2 a, Int2 b)
{
    return a.x != b.x || a.y != b.y;
}
