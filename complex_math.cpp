#include "complex_math.h"

#include "math_basics.h"
#include "assert.h"

extern const Complex complex_zero = {0.0f, 0.0f};

Complex operator - (Complex c0)
{
	Complex result;
	result.r = -c0.r;
	result.i = -c0.i;
	return result;
}

Complex operator + (Complex v0, Complex v1)
{
	Complex result;
	result.r = v0.r + v1.r;
	result.i = v0.i + v1.i;
	return result;
}

Complex& operator += (Complex& v0, Complex v1)
{
	v0.r += v1.r;
	v0.i += v1.i;
	return v0;
}

Complex operator - (Complex v0, Complex v1)
{
	Complex result;
	result.r = v0.r - v1.r;
	result.i = v0.i - v1.i;
	return result;
}

Complex& operator -= (Complex& v0, Complex v1)
{
	v0.r -= v1.r;
	v0.i -= v1.i;
	return v0;
}

Complex operator * (Complex c0, Complex c1)
{
	Complex result;
	result.r = (c0.r * c1.r) - (c0.i * c1.i);
	result.i = (c0.r * c1.i) + (c0.i * c1.r);
	return result;
}

Complex& operator *= (Complex& c0, Complex c1)
{
	float a = c0.r;
	float b = c0.i;
	c0.r = (a * c1.r) - (b * c1.i);
	c0.i = (a * c1.i) + (b * c1.r);
	return c0;
}

Complex operator * (Complex c0, float s)
{
	Complex result;
	result.r = c0.r * s;
	result.i = c0.i * s;
	return result;
}

Complex operator * (float s, Complex c0)
{
	Complex result;
	result.r = s * c0.r;
	result.i = s * c0.i;
	return result;
}

Complex& operator *= (Complex& c0, float s)
{
	c0.r *= s;
	c0.i *= s;
	return c0;
}

Complex operator / (Complex c0, Complex c1)
{
	float a = c0.r;
	float b = c0.i;
	float c = c1.r;
	float d = c1.i;
	float z = (c * c) + (d * d);

	Complex result;
	result.r = ((a * c) + (b * d)) / z;
	result.i = ((b * c) - (a * d)) / z;
	return result;
}

Complex& operator /= (Complex& c0, Complex c1)
{
	float a = c0.r;
	float b = c0.i;
	float c = c1.r;
	float d = c1.i;
	float z = (c * c) + (d * d);

	c0.r = ((a * c) + (b * d)) / z;
	c0.i = ((b * c) - (a * d)) / z;
	return c0;
}

Complex operator / (Complex c0, float r)
{
	Complex result;
	result.r = c0.r / r;
	result.i = c0.i / r;
	return result;
}

Complex& operator /= (Complex& c0, float r)
{
	c0.r /= r;
	c0.i /= r;
	return c0;
}

float abs(Complex x)
{
	return sqrt((x.r * x.r) + (x.i * x.i));
}

float complex_angle(Complex x)
{
	ASSERT((x.r * x.r) + (x.i * x.i) != 0.0f);
	return atan2(x.i, x.r);
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

Complex exp(Complex x)
{
	Complex result;
	float b = exp(x.r);
	result.r = b * cos(x.i);
	result.i = b * sin(x.i);
	return result;
}

Complex log(Complex x)
{
	Complex result;
	result.r = log(abs(x));
	result.i = complex_angle(x);
	return result;
}

Complex pow(Complex x, Complex y)
{
	return exp(y * log(x));
}

Complex sqrt(Complex x)
{
	Complex half = {1.0f / 2.0f, 0.0f};
	return pow(x, half);
}

Complex cbrt(Complex x)
{
	Complex third = {1.0f / 3.0f, 0.0f};
	return pow(x, third);
}

Complex polar(float magnitude, float angle)
{
	Complex result;
	result.r = magnitude * cos(angle);
	result.i = magnitude * sin(angle);
	return result;
}

Complex conjugate(Complex x)
{
	Complex result;
	result.r = x.r;
	result.i = -x.i;
	return result;
}

Complex reciprocal(Complex x)
{
	Complex result;
	float d = abs(x);
	result.r = x.r / d;
	result.i = -x.i / d;
	return result;
}
