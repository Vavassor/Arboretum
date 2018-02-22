#include "float_utilities.h"

#include <cfloat>
#include <climits>

#include "math_basics.h"
#include "assert.h"

extern const float infinity = FLT_MAX;
extern const float tau = 6.28318530717958647692f;
extern const float pi = 3.14159265358979323846f;
extern const float pi_over_2 = 1.57079632679489661923f;

float clamp(float a, float min, float max)
{
    return fmin(fmax(a, min), max);
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
    return abs(x - 1.0f) <= 0.5e-5f;
}

bool almost_equals(float x, float y)
{
    return abs(x - y) < 1e-6f;
}
