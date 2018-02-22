#ifndef COMPLEX_MATH_H_
#define COMPLEX_MATH_H_

struct Complex
{
    float r, i;
};

Complex operator - (Complex c0);

Complex operator + (Complex v0, Complex v1);
Complex& operator += (Complex& v0, Complex v1);
Complex operator - (Complex v0, Complex v1);
Complex& operator -= (Complex& v0, Complex v1);
Complex operator * (Complex c0, Complex c1);
Complex& operator *= (Complex& c0, Complex c1);
Complex operator * (Complex c0, float s);
Complex operator * (float s, Complex c0);
Complex& operator *= (Complex& c0, float s);
Complex operator / (Complex c0, Complex c1);
Complex& operator /= (Complex& c0, Complex c1);
Complex operator / (Complex c0, float r);
Complex& operator /= (Complex& c0, float r);

float abs(Complex x);
float complex_angle(Complex x);
float complex_angle_or_zero(Complex x);
Complex exp(Complex x);
Complex log(Complex x);
Complex pow(Complex x, Complex y);
Complex sqrt(Complex x);
Complex cbrt(Complex x);

Complex polar(float magnitude, float angle);
Complex conjugate(Complex x);
Complex reciprocal(Complex x);

extern const Complex complex_zero;

#endif // COMPLEX_MATH_H_
