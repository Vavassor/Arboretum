#include "vector_math.h"

#include "assert.h"
#include "float_utilities.h"
#include "math_basics.h"

#include <float.h>

const Float2 float2_zero = {{0.0f, 0.0f}};
const Float2 float2_minus_infinity = {{-FLT_MAX, -FLT_MAX}};
const Float2 float2_plus_infinity = {{+FLT_MAX, +FLT_MAX}};

Float2 float2_add(Float2 a, Float2 b)
{
    return (Float2){{a.x + b.x, a.y + b.y}};
}

Float2 float2_subtract(Float2 a, Float2 b)
{
    return (Float2){{a.x - b.x, a.y - b.y}};
}

Float2 float2_multiply(float s, Float2 v)
{
    return (Float2){{s * v.x, s * v.y}};
}

Float2 float2_divide(Float2 v, float s)
{
    return (Float2){{v.x / s, v.y / s}};
}

Float2 float2_negate(Float2 v)
{
    return (Float2){{-v.x, -v.y}};
}

void float2_add_assign(Float2* a, Float2 b)
{
    *a = (Float2){{a->x + b.x, a->y + b.y}};
}

Float2 float2_perp(Float2 v)
{
    return (Float2){{v.y, -v.x}};
}

float float2_squared_length(Float2 v)
{
    return (v.x * v.x) + (v.y * v.y);
}

float float2_length(Float2 v)
{
    return sqrtf(float2_squared_length(v));
}

float float2_squared_distance(Float2 a, Float2 b)
{
    return float2_squared_length(float2_subtract(b, a));
}

Float2 float2_normalise(Float2 v)
{
    float l = float2_length(v);
    ASSERT(l != 0.0f && isfinite(l));
    return float2_divide(v, l);
}

float float2_dot(Float2 a, Float2 b)
{
    return (a.x * b.x) + (a.y * b.y);
}

Float2 float2_pointwise_multiply(Float2 a, Float2 b)
{
    return (Float2){{a.x * b.x, a.y * b.y}};
}

Float2 float2_pointwise_divide(Float2 a, Float2 b)
{
    return (Float2){{a.x / b.x, a.y / b.y}};
}

Float2 float2_lerp(Float2 a, Float2 b, float t)
{
    Float2 result;
    result.x = lerp(a.x, b.x, t);
    result.y = lerp(a.y, b.y, t);
    return result;
}

Float2 float2_min(Float2 a, Float2 b)
{
    Float2 result;
    result.x = fminf(a.x, b.x);
    result.y = fminf(a.y, b.y);
    return result;
}

Float2 float2_max(Float2 a, Float2 b)
{
    Float2 result;
    result.x = fmaxf(a.x, b.x);
    result.y = fmaxf(a.y, b.y);
    return result;
}

bool float2_exactly_equals(Float2 a, Float2 b)
{
    return a.x == b.x && a.y == b.y;
}

Float2 float3_extract_float2(Float3 v)
{
    return (Float2){{v.x, v.y}};
}

float float2_determinant(Float2 a, Float2 b)
{
    return (a.x * b.y) - (a.y * b.x);
}

const Float3 float3_zero              = {{0.0f, 0.0f, 0.0f}};
const Float3 float3_one               = {{1.0f, 1.0f, 1.0f}};
const Float3 float3_unit_x            = {{1.0f, 0.0f, 0.0f}};
const Float3 float3_unit_y            = {{0.0f, 1.0f, 0.0f}};
const Float3 float3_unit_z            = {{0.0f, 0.0f, 1.0f}};
const Float3 float3_minus_infinity    = {{-FLT_MAX, -FLT_MAX, -FLT_MAX}};
const Float3 float3_plus_infinity     = {{+FLT_MAX, +FLT_MAX, +FLT_MAX}};

Float3 float3_add(Float3 a, Float3 b)
{
    return (Float3){{a.x + b.x, a.y + b.y, a.z + b.z}};
}

Float3 float3_subtract(Float3 a, Float3 b)
{
    return (Float3){{a.x - b.x, a.y - b.y, a.z - b.z}};
}

Float3 float3_multiply(float s, Float3 v)
{
    return (Float3){{s * v.x, s * v.y, s * v.z}};
}

Float3 float3_divide(Float3 v, float s)
{
    return (Float3){{v.x / s, v.y / s, v.z / s}};
}

Float3 float3_negate(Float3 v)
{
    return (Float3){{-v.x, -v.y, -v.z}};
}

Float3 float3_madd(float a, Float3 b, Float3 c)
{
    Float3 result;
    result.x = (a * b.x) + c.x;
    result.y = (a * b.y) + c.y;
    result.z = (a * b.z) + c.z;
    return result;
}

float float3_squared_length(Float3 v)
{
    return (v.x * v.x) + (v.y * v.y) + (v.z * v.z);
}

float float3_length(Float3 v)
{
    return sqrtf(float3_squared_length(v));
}

float float3_squared_distance(Float3 a, Float3 b)
{
    return float3_squared_length(float3_subtract(b, a));
}

float float3_distance(Float3 a, Float3 b)
{
    return float3_length(float3_subtract(b, a));
}

Float3 float3_normalise(Float3 v)
{
    float l = float3_length(v);
    ASSERT(l != 0.0f && isfinite(l));
    return float3_divide(v, l);
}

float float3_dot(Float3 a, Float3 b)
{
    return (a.x * b.x) + (a.y * b.y) + (a.z * b.z);
}

Float3 float3_cross(Float3 a, Float3 b)
{
    Float3 result;
    result.x = (a.y * b.z) - (a.z * b.y);
    result.y = (a.z * b.x) - (a.x * b.z);
    result.z = (a.x * b.y) - (a.y * b.x);
    return result;
}

Float3 float3_pointwise_multiply(Float3 a, Float3 b)
{
    Float3 result;
    result.x = a.x * b.x;
    result.y = a.y * b.y;
    result.z = a.z * b.z;
    return result;
}

Float3 float3_pointwise_divide(Float3 a, Float3 b)
{
    Float3 result;
    result.x = a.x / b.x;
    result.y = a.y / b.y;
    result.z = a.z / b.z;
    return result;
}

Float3 float3_lerp(Float3 a, Float3 b, float t)
{
    Float3 result;
    result.x = lerp(a.x, b.x, t);
    result.y = lerp(a.y, b.y, t);
    result.z = lerp(a.z, b.z, t);
    return result;
}

Float3 float3_reciprocal(Float3 v)
{
    ASSERT(v.x != 0.0f && v.y != 0.0f && v.z != 0.0f);
    Float3 result;
    result.x = 1.0f / v.x;
    result.y = 1.0f / v.y;
    result.z = 1.0f / v.z;
    return result;
}

Float3 float3_max(Float3 a, Float3 b)
{
    Float3 result;
    result.x = fmaxf(a.x, b.x);
    result.y = fmaxf(a.y, b.y);
    result.z = fmaxf(a.z, b.z);
    return result;
}

Float3 float3_min(Float3 a, Float3 b)
{
    Float3 result;
    result.x = fminf(a.x, b.x);
    result.y = fminf(a.y, b.y);
    result.z = fminf(a.z, b.z);
    return result;
}

Float3 float3_perp(Float3 v)
{
    Float3 a = {{fabsf(v.x), fabsf(v.y), fabsf(v.z)}};

    bool syx = signbit(a.x - a.y);
    bool szx = signbit(a.x - a.z);
    bool szy = signbit(a.y - a.z);

    bool xm = syx & szx;
    bool ym = (1 ^ xm) & szy;
    bool zm = 1 ^ (xm & ym);

    Float3 m;
    m.x = (float) xm;
    m.y = (float) ym;
    m.z = (float) zm;

    return float3_cross(v, m);
}

Float3 float3_project(Float3 a, Float3 b)
{
    float l = float3_squared_length(b);
    ASSERT(l != 0.0f && isfinite(l));
    float d = float3_dot(a, b) / l;
    return float3_multiply(d, b);
}

Float3 float3_reject(Float3 a, Float3 b)
{
    return float3_subtract(a, float3_project(a, b));
}

float float3_angle_between(Float3 a, Float3 b)
{
    float l = float3_length(a) * float3_length(b);
    float phase = float3_dot(a, b) / l;
    return acosf(phase);
}

bool float3_exactly_equals(Float3 a, Float3 b)
{
    return a.x == b.x && a.y == b.y && a.z == b.z;
}

bool float3_is_normalised(Float3 v)
{
    return almost_one(float3_length(v));
}

Float3 float2_make_float3(Float2 v)
{
    return (Float3){{v.x, v.y, 0.0f}};
}

Float4 float4_make_vector4(Float3 v)
{
    return (Float4){{v.x, v.y, v.z, 0.0f}};
}

Float3 float3_set_all(float x)
{
    return (Float3){{x, x, x}};
}

Float3 float4_extract_float3(Float4 v)
{
    return (Float3){{v.x, v.y, v.z}};
}

// Quaternions..................................................................

const Quaternion quaternion_identity = {1.0f, 0.0f, 0.0f, 0.0f};

Quaternion quaternion_multiply(Quaternion a, Quaternion b)
{
    // In terms of the quaternions' vector and scalar parts, multiplication is:
    // qq′=[v,w][v′,w′]
    //    ≝[v×v′+wv′+w′v,ww′-v⋅v′]

    Quaternion result;
    result.w = (a.w * b.w) - ((a.x * b.x) + (a.y * b.y) + (a.z * b.z));
    result.x = ((a.y * b.z) - (a.z * b.y)) + (a.w * b.x) + (b.w * a.x);
    result.y = ((a.z * b.x) - (a.x * b.z)) + (a.w * b.y) + (b.w * a.y);
    result.z = ((a.x * b.y) - (a.y * b.x)) + (a.w * b.z) + (b.w * a.z);
    return result;
}

Float3 quaternion_rotate(Quaternion q, Float3 v)
{
    Float3 vector_part = {{q.x, q.y, q.z}};
    Float3 t = float3_multiply(2.0f, float3_cross(vector_part, v));
    Float3 arm = float3_cross(vector_part, t);
    return float3_add(float3_madd(q.w, t, v), arm);
}

Quaternion quaternion_divide(Quaternion q, float s)
{
    Quaternion result;
    result.w = q.w / s;
    result.x = q.x / s;
    result.y = q.y / s;
    result.z = q.z / s;
    return result;
}

float quaternion_norm(Quaternion q)
{
    float result = sqrtf((q.w * q.w) + (q.x * q.x) + (q.y * q.y) + (q.z * q.z));
    return result;
}

Quaternion quaternion_conjugate(Quaternion q)
{
    Quaternion result;
    result.w = q.w;
    result.x = -q.x;
    result.y = -q.y;
    result.z = -q.z;
    return result;
}

Quaternion quaternion_axis_angle(Float3 axis, float angle)
{
    ASSERT(isfinite(angle));

    angle /= 2.0f;
    float phase = sinf(angle);
    Float3 v = float3_normalise(axis);

    Quaternion result;
    result.w = cosf(angle);
    result.x = v.x * phase;
    result.y = v.y * phase;
    result.z = v.z * phase;
    // Rotations must be unit quaternions.
    ASSERT(almost_one(quaternion_norm(result)));
    return result;
}

// Matrices.....................................................................

const Matrix3 matrix3_identity =
{{
    1.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 0.0f,
    0.0f, 0.0f, 1.0f
}};

Float2 matrix3_transform(Matrix3 m, Float3 v)
{
    Float2 result;
    result.x = (m.e[0] * v.x) + (m.e[1] * v.y) + (m.e[2] * v.z);
    result.y = (m.e[3] * v.x) + (m.e[4] * v.y) + (m.e[5] * v.z);
    return result;
}

Matrix3 matrix3_transpose(Matrix3 m)
{
    return (Matrix3)
    {{
        m.e[0], m.e[3], m.e[6],
        m.e[1], m.e[4], m.e[7],
        m.e[2], m.e[5], m.e[8],
    }};
}

Matrix3 matrix3_orthogonal_basis(Float3 v)
{
    Matrix3 result;
    float l = (v.x * v.x) + (v.y * v.y);
    if(!almost_zero(l))
    {
        float d = sqrtf(l);
        Float3 n = {{v.y / d, -v.x / d, 0.0f}};
        result.e[0] = n.x;
        result.e[1] = n.y;
        result.e[2] = n.z;
        result.e[3] = (-v.z * n.y);
        result.e[4] = (v.z * n.x);
        result.e[5] = (v.x * n.y) - (v.y * n.x);
    }
    else
    {
        if(v.z < 0.0f)
        {
            result.e[0] = -1.0f;
        }
        else
        {
            result.e[0] = 1.0f;
        }
        result.e[1] = 0.0f;
        result.e[2] = 0.0f;
        result.e[3] = 0.0f;
        result.e[4] = 1.0f;
        result.e[5] = 0.0f;
    }
    result.e[6] = v.x;
    result.e[7] = v.y;
    result.e[8] = v.z;
    return result;
}

const Matrix4 matrix4_identity =
{{
    1.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 1.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 1.0f
}};

Matrix4 matrix4_multiply(Matrix4 a, Matrix4 b)
{
    Matrix4 result;
    for(int i = 0; i < 4; ++i)
    {
        for(int j = 0; j < 4; ++j)
        {
            float m = 0.0f;
            for(int w = 0; w < 4; ++w)
            {
                 m += a.e[4 * i + w] * b.e[4 * w + j];
            }
            result.e[4 * i + j] = m;
        }
    }
    return result;
}

Float3 matrix4_transform_point(Matrix4 m, Float3 v)
{
    float a = (m.e[12] * v.x) + (m.e[13] * v.y) + (m.e[14] * v.z) + m.e[15];

    Float3 result;
    result.x = ((m.e[0] * v.x) + (m.e[1] * v.y) + (m.e[2]  * v.z) + m.e[3])  / a;
    result.y = ((m.e[4] * v.x) + (m.e[5] * v.y) + (m.e[6]  * v.z) + m.e[7])  / a;
    result.z = ((m.e[8] * v.x) + (m.e[9] * v.y) + (m.e[10] * v.z) + m.e[11]) / a;
    return result;
}

Float3 matrix4_transform_vector(Matrix4 m, Float3 v)
{
    float a = (m.e[12] * v.x) + (m.e[13] * v.y) + (m.e[14] * v.z) + 1.0f;

    Float3 result;
    result.x = ((m.e[0] * v.x) + (m.e[1] * v.y) + (m.e[2]  * v.z)) / a;
    result.y = ((m.e[4] * v.x) + (m.e[5] * v.y) + (m.e[6]  * v.z)) / a;
    result.z = ((m.e[8] * v.x) + (m.e[9] * v.y) + (m.e[10] * v.z)) / a;
    return result;
}

Matrix4 matrix4_transpose(Matrix4 m)
{
    return (Matrix4)
    {{
        m.e[0], m.e[4], m.e[8],  m.e[12],
        m.e[1], m.e[5], m.e[9],  m.e[13],
        m.e[2], m.e[6], m.e[10], m.e[14],
        m.e[3], m.e[7], m.e[11], m.e[15]
    }};
}

Matrix4 matrix4_view(Float3 x_axis, Float3 y_axis, Float3 z_axis, Float3 position)
{
    Matrix4 result;

    result.e[0]  = x_axis.x;
    result.e[1]  = x_axis.y;
    result.e[2]  = x_axis.z;
    result.e[3]  = -float3_dot(x_axis, position);

    result.e[4]  = y_axis.x;
    result.e[5]  = y_axis.y;
    result.e[6]  = y_axis.z;
    result.e[7]  = -float3_dot(y_axis, position);

    result.e[8]  = z_axis.x;
    result.e[9]  = z_axis.y;
    result.e[10] = z_axis.z;
    result.e[11] = -float3_dot(z_axis, position);

    result.e[12] = 0.0f;
    result.e[13] = 0.0f;
    result.e[14] = 0.0f;
    result.e[15] = 1.0f;

    return result;
}

// This function assumes a right-handed coordinate system.
Matrix4 matrix4_look_at(Float3 position, Float3 target, Float3 world_up)
{
    Float3 forward = float3_normalise(float3_subtract(position, target));
    Float3 right = float3_normalise(float3_cross(world_up, forward));
    Float3 up = float3_normalise(float3_cross(forward, right));
    return matrix4_view(right, up, forward, position);
}

// This function assumes a right-handed coordinate system.
Matrix4 matrix4_turn(Float3 position, float yaw, float pitch, Float3 world_up)
{
    Float3 facing;
    facing.x = cosf(pitch) * cosf(yaw);
    facing.y = cosf(pitch) * sinf(yaw);
    facing.z = sinf(pitch);
    Float3 forward = float3_normalise(facing);
    Float3 right = float3_normalise(float3_cross(world_up, forward));
    Float3 up = float3_normalise(float3_cross(forward, right));
    return matrix4_view(right, up, forward, position);
}

// This transforms from a right-handed coordinate system to OpenGL's default
// clip space. A position will be viewable in this clip space if its x, y, and
// z components are in the range [-w,w] of its w component.
Matrix4 matrix4_perspective_projection(float fovy, float width, float height, float near_plane, float far_plane)
{
    float coty = 1.0f / tanf(fovy / 2.0f);
    float aspect_ratio = width / height;
    float neg_depth = near_plane - far_plane;

    Matrix4 result;

    result.e[0] = coty / aspect_ratio;
    result.e[1] = 0.0f;
    result.e[2] = 0.0f;
    result.e[3] = 0.0f;

    result.e[4] = 0.0f;
    result.e[5] = coty;
    result.e[6] = 0.0f;
    result.e[7] = 0.0f;

    result.e[8] = 0.0f;
    result.e[9] = 0.0f;
    result.e[10] = (near_plane + far_plane) / neg_depth;
    result.e[11] = 2.0f * near_plane * far_plane / neg_depth;

    result.e[12] = 0.0f;
    result.e[13] = 0.0f;
    result.e[14] = -1.0f;
    result.e[15] = 0.0f;

    return result;
}

// This transforms from a right-handed coordinate system to OpenGL's default
// clip space. A position will be viewable in this clip space if its x, y,
// and z components are in the range [-w,w] of its w component.
Matrix4 matrix4_orthographic_projection(float width, float height, float near_plane, float far_plane)
{
    float neg_depth = near_plane - far_plane;

    Matrix4 result;

    result.e[0] = 2.0f / width;
    result.e[1] = 0.0f;
    result.e[2] = 0.0f;
    result.e[3] = 0.0f;

    result.e[4] = 0.0f;
    result.e[5] = 2.0f / height;
    result.e[6] = 0.0f;
    result.e[7] = 0.0f;

    result.e[8] = 0.0f;
    result.e[9] = 0.0f;
    result.e[10] = 2.0f / neg_depth;
    result.e[11] = (far_plane + near_plane) / neg_depth;

    result.e[12] = 0.0f;
    result.e[13] = 0.0f;
    result.e[14] = 0.0f;
    result.e[15] = 1.0f;

    return result;
}

Matrix4 matrix4_dilation(Float3 dilation)
{
    return (Matrix4)
    {{
        dilation.x, 0.0f, 0.0f, 0.0f,
        0.0f, dilation.y, 0.0f, 0.0f,
        0.0f, 0.0f, dilation.z, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
    }};
}

Matrix4 matrix4_compose_transform(Float3 position, Quaternion orientation, Float3 scale)
{
    float w = orientation.w;
    float x = orientation.x;
    float y = orientation.y;
    float z = orientation.z;

    float xw = x * w;
    float xx = x * x;
    float xy = x * y;
    float xz = x * z;

    float yw = y * w;
    float yy = y * y;
    float yz = y * z;

    float zw = z * w;
    float zz = z * z;

    Matrix4 result;

    result.e[0]  = (1.0f - 2.0f * (yy + zz)) * scale.x;
    result.e[1]  = (       2.0f * (xy + zw)) * scale.y;
    result.e[2]  = (       2.0f * (xz - yw)) * scale.z;
    result.e[3]  = position.x;

    result.e[4]  = (       2.0f * (xy - zw)) * scale.x;
    result.e[5]  = (1.0f - 2.0f * (xx + zz)) * scale.y;
    result.e[6]  = (       2.0f * (yz + xw)) * scale.z;
    result.e[7]  = position.y;

    result.e[8]  = (       2.0f * (xz + yw)) * scale.x;
    result.e[9]  = (       2.0f * (yz - xw)) * scale.y;
    result.e[10] = (1.0f - 2.0f * (xx + yy)) * scale.z;
    result.e[11] = position.z;

    result.e[12] = 0.0f;
    result.e[13] = 0.0f;
    result.e[14] = 0.0f;
    result.e[15] = 1.0f;

    return result;
}

Matrix4 matrix4_inverse_view(Matrix4 m)
{
    float a = -((m.e[0] * m.e[3]) + (m.e[4] * m.e[7]) + (m.e[8]  * m.e[11]));
    float b = -((m.e[1] * m.e[3]) + (m.e[5] * m.e[7]) + (m.e[9]  * m.e[11]));
    float c = -((m.e[2] * m.e[3]) + (m.e[6] * m.e[7]) + (m.e[10] * m.e[11]));
    return (Matrix4)
    {{
        m.e[0], m.e[4], m.e[8],  a,
        m.e[1], m.e[5], m.e[9],  b,
        m.e[2], m.e[6], m.e[10], c,
        0.0f, 0.0f, 0.0f,  1.0f
    }};
}

Matrix4 matrix4_inverse_perspective(Matrix4 m)
{
    float m0  = 1.0f / m.e[0];
    float m5  = 1.0f / m.e[5];
    float m14 = 1.0f / m.e[11];
    float m15 = m.e[10] / m.e[11];
    return (Matrix4)
    {{
        m0,   0.0f, 0.0f,  0.0f,
        0.0f, m5,   0.0f,  0.0f,
        0.0f, 0.0f, 0.0f, -1.0f,
        0.0f, 0.0f, m14,   m15
    }};
}

Matrix4 matrix4_inverse_orthographic(Matrix4 m)
{
    float m0 = 1.0f / m.e[0];
    float m5 = 1.0f / m.e[5];
    float m10 = 1.0f / m.e[10];
    float m11 = m.e[11] / m.e[10];
    return (Matrix4)
    {{
        m0,   0.0f, 0.0f, 0.0f,
        0.0f, m5,   0.0f, 0.0f,
        0.0f, 0.0f, m10,  m11,
        0.0f, 0.0f, 0.0f, 1.0f
    }};
}

Matrix4 matrix4_inverse_transform(Matrix4 m)
{
    // The scale can be extracted from the rotation data by just taking the
    // length of the first three row vectors.

    float dx = sqrtf(m.e[0] * m.e[0] + m.e[4] * m.e[4] + m.e[8]  * m.e[8]);
    float dy = sqrtf(m.e[1] * m.e[1] + m.e[5] * m.e[5] + m.e[9]  * m.e[9]);
    float dz = sqrtf(m.e[2] * m.e[2] + m.e[6] * m.e[6] + m.e[10] * m.e[10]);

    // The extracted scale can then be divided out to isolate the rotation rows.

    float m00 = m.e[0] / dx;
    float m10 = m.e[4] / dx;
    float m20 = m.e[8] / dx;

    float m01 = m.e[1] / dy;
    float m11 = m.e[5] / dy;
    float m21 = m.e[9] / dy;

    float m02 = m.e[2] / dz;
    float m12 = m.e[6] / dz;
    float m22 = m.e[10] / dz;

    // The inverse of the translation elements is the negation of the
    // translation vector multiplied by the transpose of the rotation and the
    // reciprocal of the dilation.

    float a = -(m00 * m.e[3] + m10 * m.e[7] + m20 * m.e[11]) / dx;
    float b = -(m01 * m.e[3] + m11 * m.e[7] + m21 * m.e[11]) / dy;
    float c = -(m02 * m.e[3] + m12 * m.e[7] + m22 * m.e[11]) / dz;

    // After the unmodified rotation elements have been used to figure out the
    // inverse translation, they can be modified with the inverse dilation.

    m00 /= dx;
    m10 /= dx;
    m20 /= dx;

    m01 /= dy;
    m11 /= dy;
    m21 /= dy;

    m02 /= dz;
    m12 /= dz;
    m22 /= dz;

    // Put everything in, making sure to place the rotation elements in
    // transposed order.

    return (Matrix4)
    {{
        m00, m10, m20, a,
        m01, m11, m21, b,
        m02, m12, m22, c,
        0.0f, 0.0f, 0.0f, 1.0f
    }};
}
