#ifndef MAYBE_TYPES_H_
#define MAYBE_TYPES_H_

#include <stdbool.h>
#include <stdint.h>

typedef struct MaybeDouble
{
    double value;
    bool valid;
} MaybeDouble;

typedef struct MaybeFloat
{
    float value;
    bool valid;
} MaybeFloat;

typedef struct MaybeInt
{
    int value;
    bool valid;
} MaybeInt;

typedef struct MaybePointer
{
    void* value;
    bool valid;
} MaybePointer;

typedef struct MaybeUint64
{
    uint64_t value;
    bool valid;
} MaybeUint64;

#endif // MAYBE_TYPES_H_
