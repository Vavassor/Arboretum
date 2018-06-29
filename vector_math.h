#ifndef VECTOR_MATH_H_
#define VECTOR_MATH_H_

#if defined(__cplusplus)
extern "C" {
#endif

#include <stdbool.h>

// Vectors......................................................................

typedef union Float2
{
    struct
    {
        float x, y;
    };
    float e[2];
} Float2;

extern const Float2 float2_zero;
extern const Float2 float2_minus_infinity;
extern const Float2 float2_plus_infinity;

Float2 float2_add(Float2 a, Float2 b);
Float2 float2_subtract(Float2 a, Float2 b);
Float2 float2_multiply(float s, Float2 v);
Float2 float2_divide(Float2 v, float s);
Float2 float2_negate(Float2 v);

Float2 float2_perp(Float2 v);
float float2_squared_length(Float2 v);
float float2_length(Float2 v);
float float2_squared_distance(Float2 a, Float2 b);
Float2 float2_normalise(Float2 v);
float float2_dot(Float2 a, Float2 b);
Float2 float2_pointwise_multiply(Float2 a, Float2 b);
Float2 float2_pointwise_divide(Float2 a, Float2 b);
Float2 float2_lerp(Float2 a, Float2 b, float t);
Float2 float2_min(Float2 a, Float2 b);
Float2 float2_max(Float2 a, Float2 b);
bool float2_exactly_equals(Float2 a, Float2 b);
float float2_determinant(Float2 a, Float2 b);

typedef union Float3
{
    struct
    {
        float x, y, z;
    };
    float e[3];
} Float3;

extern const Float3 float3_zero;
extern const Float3 float3_one;
extern const Float3 float3_unit_x;
extern const Float3 float3_unit_y;
extern const Float3 float3_unit_z;
extern const Float3 float3_minus_infinity;
extern const Float3 float3_plus_infinity;

Float3 float3_add(Float3 a, Float3 b);
Float3 float3_subtract(Float3 a, Float3 b);
Float3 float3_multiply(float s, Float3 v);
Float3 float3_divide(Float3 v, float s);
Float3 float3_negate(Float3 v);
Float3 float3_madd(float a, Float3 b, Float3 c);

float float3_squared_length(Float3 v);
float float3_length(Float3 v);
float float3_squared_distance(Float3 a, Float3 b);
float float3_distance(Float3 a, Float3 b);
Float3 float3_normalise(Float3 v);
float float3_dot(Float3 a, Float3 b);
Float3 float3_cross(Float3 a, Float3 b);
Float3 float3_pointwise_multiply(Float3 a, Float3 b);
Float3 float3_pointwise_divide(Float3 a, Float3 b);
Float3 float3_lerp(Float3 a, Float3 b, float t);
Float3 float3_reciprocal(Float3 v);
Float3 float3_max(Float3 a, Float3 b);
Float3 float3_min(Float3 a, Float3 b);
Float3 float3_perp(Float3 v);
Float3 float3_project(Float3 a, Float3 b);
Float3 float3_reject(Float3 a, Float3 b);
float float3_angle_between(Float3 a, Float3 b);
bool float3_exactly_equals(Float3 a, Float3 b);
bool float3_is_normalised(Float3 v);
Float3 float2_make_float3(Float2 v);
Float3 float3_set_all(float x);
Float2 float3_extract_float2(Float3 v);

typedef union Float4
{
    struct
    {
        float x, y, z, w;
    };
    float e[4];
} Float4;

Float4 float3_make_float4(Float3 v);
Float3 float4_extract_float3(Float4 v);

// Quaternions..................................................................

typedef struct Quaternion
{
    float w, x, y, z;
} Quaternion;

extern const Quaternion quaternion_identity;

Quaternion quaternion_multiply(Quaternion a, Quaternion b);
Float3 quaternion_rotate(Quaternion q, Float3 v);
Quaternion quaternion_divide(Quaternion q, float s);

float quaternion_norm(Quaternion q);
Quaternion quaternion_conjugate(Quaternion q);
Quaternion quaternion_axis_angle(Float3 axis, float angle);

// Matrices.....................................................................

typedef struct Matrix3
{
    float e[9]; // elements in row-major order
} Matrix3;

extern const Matrix3 matrix3_identity;

Float2 matrix3_transform(Matrix3 m, Float3 v);

Matrix3 matrix3_transpose(Matrix3 m);
Matrix3 matrix3_orthogonal_basis(Float3 v);

typedef struct Matrix4
{
    float e[16]; // elements in row-major order
} Matrix4;

extern const Matrix4 matrix4_identity;

Matrix4 matrix4_multiply(Matrix4 a, Matrix4 b);
Float3 matrix4_transform_point(Matrix4 m, Float3 v);
Float3 matrix4_transform_vector(Matrix4 m, Float3 v);

Matrix4 matrix4_transpose(Matrix4 m);
Matrix4 matrix4_view(Float3 x_axis, Float3 y_axis, Float3 z_axis, Float3 position);
Matrix4 matrix4_look_at(Float3 position, Float3 target, Float3 world_up);
Matrix4 matrix4_turn(Float3 position, float yaw, float pitch, Float3 world_up);
Matrix4 matrix4_perspective_projection(float fovy, float width, float height, float near_plane, float far_plane);
Matrix4 matrix4_orthographic_projection(float width, float height, float near_plane, float far_plane);
Matrix4 matrix4_dilation(Float3 dilation);
Matrix4 matrix4_compose_transform(Float3 position, Quaternion orientation, Float3 scale);
Matrix4 matrix4_inverse_view(Matrix4 m);
Matrix4 matrix4_inverse_perspective(Matrix4 m);
Matrix4 matrix4_inverse_orthographic(Matrix4 m);
Matrix4 matrix4_inverse_transform(Matrix4 m);

#if defined(__cplusplus)
} // extern "C"
#endif

#endif // VECTOR_MATH_H_
