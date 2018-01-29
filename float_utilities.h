#ifndef FLOAT_UTILITIES_H_
#define FLOAT_UTILITIES_H_

extern const float infinity;
extern const float tau;
extern const float pi;
extern const float pi_over_2;

float clamp(float a, float min, float max);
float lerp(float v0, float v1, float t);
float unlerp(float v0, float v1, float t);

bool almost_zero(float x);
bool almost_one(float x);
bool almost_equals(float x, float y);

#endif // FLOAT_UTILITIES_H_
