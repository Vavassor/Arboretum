#include "float_utilities.h"

#include <float.h>
#include <limits.h>

#include "math_basics.h"
#include "assert.h"

const float infinity = FLT_MAX;
const float tau = 6.28318530717958647692f;
const float pi = 3.14159265358979323846f;
const float pi_over_2 = 1.57079632679489661923f;
const float three_pi_over_2 = 4.71238898038f;

float clamp(float a, float min, float max)
{
    return fminf(fmaxf(a, min), max);
}

float saturate(float a)
{
    return fminf(fmaxf(a, 0.0f), 1.0f);
}

float lerp(float v0, float v1, float t)
{
    return (1.0f - t) * v0 + t * v1;
}

float unlerp(float v0, float v1, float t)
{
    ASSERT(v0 != v1);
    return (t - v0) / (v1 - v0);
}

bool almost_zero(float x)
{
    return x > -1e-6f && x < 1e-6f;
}

bool almost_one(float x)
{
    return fabsf(x - 1.0f) <= 0.5e-5f;
}

bool almost_equals(float x, float y)
{
    return fabsf(x - y) < 1e-6f;
}
