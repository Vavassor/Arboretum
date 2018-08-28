#include "random.h"

#include <time.h>

/*  Written in 2015 by Sebastiano Vigna (vigna@acm.org)

To the extent possible under law, the author has dedicated all copyright and
related and neighboring rights to this software to the public domain worldwide.
This software is distributed without any warranty.

See <http://creativecommons.org/publicdomain/zero/1.0/>. */

static uint64_t splitmix64(uint64_t* x)
{
    *x += UINT64_C(0x9E3779B97F4A7C15);
    uint64_t z = *x;
    z = (z ^ (z >> 30)) * UINT64_C(0xBF58476D1CE4E5B9);
    z = (z ^ (z >> 27)) * UINT64_C(0x94D049BB133111EB);
    return z ^ (z >> 31);
}

/*  Written in 2016 by David Blackman and Sebastiano Vigna (vigna@acm.org)

To the extent possible under law, the author has dedicated all copyright and
related and neighboring rights to this software to the public domain worldwide.
This software is distributed without any warranty.

See <http://creativecommons.org/publicdomain/zero/1.0/>. */

static uint64_t rotl(const uint64_t x, int k)
{
    return (x << k) | (x >> (64 - k));
}

// Xoroshiro128+
uint64_t random_generate(RandomGenerator* generator)
{
    const uint64_t s0 = generator->s[0];
    uint64_t s1 = generator->s[1];
    const uint64_t result = s0 + s1;

    s1 ^= s0;
    generator->s[0] = rotl(s0, 55) ^ s1 ^ (s1 << 14); // a, b
    generator->s[1] = rotl(s1, 36); // c

    return result;
}

// End of Blackman & Vigna's code

static float to_float(uint64_t x)
{
    union
    {
        uint32_t i;
        float f;
    } u;
    u.i = UINT32_C(0x7f) << 23 | x >> 41;
    return u.f - 1.0f;
}

float random_float_range(RandomGenerator* generator, float min, float max)
{
    float f = to_float(random_generate(generator));
    return f * (max - min) + min;
}

int random_int_range(RandomGenerator* generator, int min, int max)
{
    int x = random_generate(generator) % (uint64_t) (max - min + 1);
    return min + x;
}

uint64_t random_seed(RandomGenerator* generator, uint64_t value)
{
    uint64_t old_seed = generator->seed;
    generator->seed = value;
    generator->s[0] = splitmix64(&generator->seed);
    generator->s[1] = splitmix64(&generator->seed);
    return old_seed;
}

uint64_t random_seed_by_time(RandomGenerator* generator)
{
    return random_seed(generator, (uint64_t) time(NULL));
}

void shuffle(RandomGenerator* generator, int* numbers, int count)
{
    for(int i = 0; i < count - 2; i += 1)
    {
        int j = random_int_range(generator, i, count - 1);
        int temp = numbers[i];
        numbers[i] = numbers[j];
        numbers[j] = temp;
    }
}
