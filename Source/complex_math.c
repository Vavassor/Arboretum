#include "complex_math.h"

#include "math_basics.h"
#include "assert.h"

const Complex complex_zero = {0.0f, 0.0f};

Complex complex_negate(Complex c)
{
    return (Complex){-c.r, -c.i};
}

Complex complex_add(Complex a, Complex b)
{
    return (Complex){a.r + b.r, a.i + b.i};
}

Complex complex_subtract(Complex a, Complex b)
{
    return (Complex){a.r - b.r, a.i - b.i};
}

Complex complex_multiply(Complex a, Complex b)
{
    Complex result;
    result.r = (a.r * b.r) - (a.i * b.i);
    result.i = (a.r * b.i) + (a.i * b.r);
    return result;
}

Complex complex_divide(Complex a, Complex b)
{
    float c[4] = {a.r, a.i, b.r, b.i};
    float z = (c[2] * c[2]) + (c[3] * c[3]);

    Complex result;
    result.r = ((c[0] * c[2]) + (c[1] * c[3])) / z;
    result.i = ((c[1] * c[2]) - (c[0] * c[3])) / z;
    return result;
}

Complex complex_scalar_multiply(float s, Complex c)
{
    return (Complex){s * c.r, s * c.i};
}

Complex complex_scalar_divide(Complex c, float s)
{
    return (Complex){c.r / s, c.i / s};
}

float complex_abs(Complex x)
{
    return sqrtf((x.r * x.r) + (x.i * x.i));
}

float complex_angle(Complex x)
{
    ASSERT((x.r * x.r) + (x.i * x.i) != 0.0f);
    return atan2f(x.i, x.r);
}

float complex_angle_or_zero(Complex x)
{
    if((x.r * x.r) + (x.i * x.i) == 0.0f)
    {
        return 0.0f;
    }
    else
    {
        return complex_angle(x);
    }
}

Complex complex_exp(Complex x)
{
    Complex result;
    float b = expf(x.r);
    result.r = b * cosf(x.i);
    result.i = b * sinf(x.i);
    return result;
}

Complex complex_log(Complex x)
{
    Complex result;
    result.r = logf(complex_abs(x));
    result.i = complex_angle(x);
    return result;
}

Complex complex_pow(Complex x, Complex y)
{
    return complex_exp(complex_multiply(y, complex_log(x)));
}

Complex complex_sqrt(Complex x)
{
    Complex half = {1.0f / 2.0f, 0.0f};
    return complex_pow(x, half);
}

Complex complex_cbrt(Complex x)
{
    Complex third = {1.0f / 3.0f, 0.0f};
    return complex_pow(x, third);
}

Complex complex_polar(float magnitude, float angle)
{
    Complex result;
    result.r = magnitude * cosf(angle);
    result.i = magnitude * sinf(angle);
    return result;
}

Complex complex_conjugate(Complex x)
{
    Complex result;
    result.r = x.r;
    result.i = -x.i;
    return result;
}

Complex reciprocal(Complex x)
{
    Complex result;
    float d = complex_abs(x);
    result.r = x.r / d;
    result.i = -x.i / d;
    return result;
}
