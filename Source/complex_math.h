// Complex Number Arithmetic

#ifndef COMPLEX_MATH_H_
#define COMPLEX_MATH_H_

typedef struct Complex
{
    float r, i;
} Complex;

Complex complex_negate(Complex c);

Complex complex_add(Complex a, Complex b);
Complex complex_subtract(Complex a, Complex b);
Complex complex_multiply(Complex a, Complex b);
Complex complex_divide(Complex a, Complex b);
Complex complex_scalar_multiply(float s, Complex c);
Complex complex_scalar_divide(Complex c, float s);

float complex_abs(Complex x);
float complex_angle(Complex x);
float complex_angle_or_zero(Complex x);
Complex complex_exp(Complex x);
Complex complex_log(Complex x);
Complex complex_pow(Complex x, Complex y);
Complex complex_sqrt(Complex x);
Complex complex_cbrt(Complex x);

Complex complex_polar(float magnitude, float angle);
Complex complex_conjugate(Complex x);
Complex complex_reciprocal(Complex x);

extern const Complex complex_zero;

#endif // COMPLEX_MATH_H_
