#include "int_utilities.h"

bool is_power_of_two(unsigned int x)
{
    return (x != 0) && !(x & (x - 1));
}

uint32_t next_power_of_two(uint32_t x)
{
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    return x + 1;
}

// In an expression x % n, if n is a power of two the expression can be
// simplified to x & (n - 1). So, this check is for making sure that
// reduction is legal for a given n.
bool can_use_bitwise_and_to_cycle(int count)
{
    return is_power_of_two(count);
}

// Generally, only use this for situations where x can be negative.
// For positive values of x, you can instead use x % n.
// For cases where x is positive and n is a power of two, this can be reduced
// to x & (n - 1).
int mod(int x, int n)
{
    return (x % n + n) % n;
}

unsigned int next_multiple(unsigned int x, unsigned int n)
{
    return n * (x + n - 1) / n;
}

bool signs_opposite(int x, int y)
{
    return (x ^ y) < 0;
}

int imax(int a, int b)
{
    return (a > b) ? a : b;
}

int imin(int a, int b)
{
    return (a < b) ? a : b;
}
