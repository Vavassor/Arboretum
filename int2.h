#ifndef INT2_H_
#define INT2_H_

#include <stdbool.h>

// A pair of integers. This name could be confused with sized integer types like
// s8 where the 8 signifies 8 bits, not 8 integers.
typedef struct Int2
{
    int x, y;
} Int2;

Int2 int2_add(Int2 a, Int2 b);
Int2 int2_subtract(Int2 a, Int2 b);
bool int2_equals(Int2 a, Int2 b);
bool int2_not_equals(Int2 a, Int2 b);

extern const Int2 int2_zero;

#endif // INT2_H_
