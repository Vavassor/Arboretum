#include "vector_extras.h"

Float2 int2_to_float2(Int2 i)
{
    return (Float2){{(float) i.x, (float) i.y}};
}
