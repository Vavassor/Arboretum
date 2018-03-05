#ifndef INT_UTILITIES_H_
#define INT_UTILITIES_H_

#include "sized_types.h"

bool is_power_of_two(unsigned int x);
u32 next_power_of_two(u32 x);
bool can_use_bitwise_and_to_cycle(int count);
int mod(int x, int n);
unsigned int next_multiple(unsigned int x, unsigned int n);

// Prefer these for integers and fmax() and fmin() for floating-point numbers.
#define MAX(a, b)\
    (((a) > (b)) ? (a) : (b))
#define MIN(a, b)\
    (((a) < (b)) ? (a) : (b))

#define CLAMP(x, min, max)\
    MIN(MAX(x, min), max)

#endif // INT_UTILITIES_H_
