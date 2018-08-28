#ifndef RANDOM_H_
#define RANDOM_H_

#include <stdint.h>

typedef struct RandomGenerator
{
    uint64_t s[2];
    uint64_t seed;
} RandomGenerator;

float random_float_range(RandomGenerator* generator, float min, float max);
uint64_t random_generate(RandomGenerator* generator);
int random_int_range(RandomGenerator* generator, int min, int max);
uint64_t random_seed(RandomGenerator* generator, uint64_t value);
uint64_t random_seed_by_time(RandomGenerator* generator);
void shuffle(RandomGenerator* generator, int* numbers, int count);

#endif // RANDOM_H_
