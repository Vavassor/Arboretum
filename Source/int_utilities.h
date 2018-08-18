#ifndef INT_UTILITIES_H_
#define INT_UTILITIES_H_

#include <stdbool.h>
#include <stdint.h>

bool is_power_of_two(unsigned int x);
uint32_t next_power_of_two(uint32_t x);
bool can_use_bitwise_and_to_cycle(int count);
int mod(int x, int n);
unsigned int next_multiple(unsigned int x, unsigned int n);
bool signs_opposite(int x, int y);
int imax(int a, int b);
int imin(int a, int b);

#endif // INT_UTILITIES_H_
