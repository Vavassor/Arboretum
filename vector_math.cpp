#include "vector_math.h"

#include <cfloat>

#include "assert.h"
#include "float_utilities.h"
#include "math_basics.h"

extern const Vector2 vector2_zero = {0.0f, 0.0f};
extern const Vector2 vector2_min = {-infinity, -infinity};
extern const Vector2 vector2_max = {+infinity, +infinity};

Vector2 operator + (Vector2 v0, Vector2 v1)
{
	Vector2 result;
	result.x = v0.x + v1.x;
	result.y = v0.y + v1.y;
	return result;
}

Vector2& operator += (Vector2& v0, Vector2 v1)
{
	v0.x += v1.x;
	v0.y += v1.y;
	return v0;
}

Vector2 operator - (Vector2 v0, Vector2 v1)
{
	Vector2 result;
	result.x = v0.x - v1.x;
	result.y = v0.y - v1.y;
	return result;
}

Vector2& operator -= (Vector2& v0, Vector2 v1)
{
	v0.x -= v1.x;
	v0.y -= v1.y;
	return v0;
}

Vector2 operator * (Vector2 v, float s)
{
	Vector2 result;
	result.x = v.x * s;
	result.y = v.y * s;
	return result;
}

Vector2 operator * (float s, Vector2 v)
{
	Vector2 result;
	result.x = s * v.x;
	result.y = s * v.y;
	return result;
}

Vector2& operator *= (Vector2& v, float s)
{
	v.x *= s;
	v.y *= s;
	return v;
}

Vector2 operator / (Vector2 v, float s)
{
	Vector2 result;
	result.x = v.x / s;
	result.y = v.y / s;
	return result;
}

Vector2& operator /= (Vector2& v, float s)
{
	v.x /= s;
	v.y /= s;
	return v;
}

Vector2 operator - (Vector2 v)
{
	return {-v.x, -v.y};
}

Vector2 perp(Vector2 v)
{
	return {v.y, -v.x};
}

float squared_length(Vector2 v)
{
	return (v.x * v.x) + (v.y * v.y);
}

float length(Vector2 v)
{
	return sqrt(squared_length(v));
}

Vector2 normalise(Vector2 v)
{
	float l = length(v);
	ASSERT(l != 0.0f && isfinite(l));
	return v / l;
}

float dot(Vector2 v0, Vector2 v1)
{
	return (v0.x * v1.x) + (v0.y * v1.y);
}

Vector2 pointwise_multiply(Vector2 v0, Vector2 v1)
{
	Vector2 result;
	result.x = v0.x * v1.x;
	result.y = v0.y * v1.y;
	return result;
}

Vector2 pointwise_divide(Vector2 v0, Vector2 v1)
{
	Vector2 result;
	result.x = v0.x / v1.x;
	result.y = v0.y / v1.y;
	return result;
}

Vector2 lerp(Vector2 v0, Vector2 v1, float t)
{
	Vector2 result;
	result.x = lerp(v0.x, v1.x, t);
	result.y = lerp(v0.y, v1.y, t);
	return result;
}

Vector2 min2(Vector2 a, Vector2 b)
{
	Vector2 result;
	result.x = fmin(a.x, b.x);
	result.y = fmin(a.y, b.y);
	return result;
}

Vector2 max2(Vector2 a, Vector2 b)
{
	Vector2 result;
	result.x = fmax(a.x, b.x);
	result.y = fmax(a.y, b.y);
	return result;
}

extern const Vector3 vector3_zero   = {0.0f, 0.0f, 0.0f};
extern const Vector3 vector3_one    = {1.0f, 1.0f, 1.0f};
extern const Vector3 vector3_unit_x = {1.0f, 0.0f, 0.0f};
extern const Vector3 vector3_unit_y = {0.0f, 1.0f, 0.0f};
extern const Vector3 vector3_unit_z = {0.0f, 0.0f, 1.0f};
extern const Vector3 vector3_min    = {-infinity, -infinity, -infinity};
extern const Vector3 vector3_max    = {+infinity, +infinity, +infinity};

Vector3 operator + (Vector3 v0, Vector3 v1)
{
	Vector3 result;
	result.x = v0.x + v1.x;
	result.y = v0.y + v1.y;
	result.z = v0.z + v1.z;
	return result;
}

Vector3& operator += (Vector3& v0, Vector3 v1)
{
	v0.x += v1.x;
	v0.y += v1.y;
	v0.z += v1.z;
	return v0;
}

Vector3 operator - (Vector3 v0, Vector3 v1)
{
	Vector3 result;
	result.x = v0.x - v1.x;
	result.y = v0.y - v1.y;
	result.z = v0.z - v1.z;
	return result;
}

Vector3& operator -= (Vector3& v0, Vector3 v1)
{
	v0.x -= v1.x;
	v0.y -= v1.y;
	v0.z -= v1.z;
	return v0;
}

Vector3 operator * (Vector3 v, float s)
{
	Vector3 result;
	result.x = v.x * s;
	result.y = v.y * s;
	result.z = v.z * s;
	return result;
}

Vector3 operator * (float s, Vector3 v)
{
	Vector3 result;
	result.x = s * v.x;
	result.y = s * v.y;
	result.z = s * v.z;
	return result;
}

Vector3& operator *= (Vector3& v, float s)
{
	v.x *= s;
	v.y *= s;
	v.z *= s;
	return v;
}

Vector3 operator / (Vector3 v, float s)
{
	Vector3 result;
	result.x = v.x / s;
	result.y = v.y / s;
	result.z = v.z / s;
	return result;
}

Vector3& operator /= (Vector3& v, float s)
{
	v.x /= s;
	v.y /= s;
	v.z /= s;
	return v;
}

Vector3 operator + (Vector3 v)
{
	return v;
}

Vector3 operator - (Vector3 v)
{
	return {-v.x, -v.y, -v.z};
}

float squared_length(Vector3 v)
{
	return (v.x * v.x) + (v.y * v.y) + (v.z * v.z);
}

float length(Vector3 v)
{
	return sqrt(squared_length(v));
}

float squared_distance(Vector3 v0, Vector3 v1)
{
	return squared_length(v1 - v0);
}

float distance(Vector3 v0, Vector3 v1)
{
	return length(v1 - v0);
}

Vector3 normalise(Vector3 v)
{
	float l = length(v);
	ASSERT(l != 0.0f && isfinite(l));
	return v / l;
}

float dot(Vector3 v0, Vector3 v1)
{
	return (v0.x * v1.x) + (v0.y * v1.y) + (v0.z * v1.z);
}

Vector3 cross(Vector3 v0, Vector3 v1)
{
	Vector3 result;
	result.x = (v0.y * v1.z) - (v0.z * v1.y);
	result.y = (v0.z * v1.x) - (v0.x * v1.z);
	result.z = (v0.x * v1.y) - (v0.y * v1.x);
	return result;
}

Vector3 pointwise_multiply(Vector3 v0, Vector3 v1)
{
	Vector3 result;
	result.x = v0.x * v1.x;
	result.y = v0.y * v1.y;
	result.z = v0.z * v1.z;
	return result;
}

Vector3 pointwise_divide(Vector3 v0, Vector3 v1)
{
	Vector3 result;
	result.x = v0.x / v1.x;
	result.y = v0.y / v1.y;
	result.z = v0.z / v1.z;
	return result;
}

Vector3 lerp(Vector3 v0, Vector3 v1, float t)
{
	Vector3 result;
	result.x = lerp(v0.x, v1.x, t);
	result.y = lerp(v0.y, v1.y, t);
	result.z = lerp(v0.z, v1.z, t);
	return result;
}

Vector3 reciprocal(Vector3 v)
{
	ASSERT(v.x != 0.0f && v.y != 0.0f && v.z != 0.0f);
	Vector3 result;
	result.x = 1.0f / v.x;
	result.y = 1.0f / v.y;
	result.z = 1.0f / v.z;
	return result;
}

Vector3 max3(Vector3 v0, Vector3 v1)
{
	Vector3 result;
	result.x = fmax(v0.x, v1.x);
	result.y = fmax(v0.y, v1.y);
	result.z = fmax(v0.z, v1.z);
	return result;
}

Vector3 min3(Vector3 v0, Vector3 v1)
{
	Vector3 result;
	result.x = fmin(v0.x, v1.x);
	result.y = fmin(v0.y, v1.y);
	result.z = fmin(v0.z, v1.z);
	return result;
}

Vector3 perp(Vector3 v)
{
	Vector3 a = {abs(v.x), abs(v.y), abs(v.z)};

	bool syx = signbit(a.x - a.y);
	bool szx = signbit(a.x - a.z);
	bool szy = signbit(a.y - a.z);

	bool xm = syx & szx;
	bool ym = (1 ^ xm) & szy;
	bool zm = 1 ^ (xm & ym);

	Vector3 m = {xm, ym, zm};

	return cross(v, m);
}

bool exactly_equals(Vector3 v0, Vector3 v1)
{
	return v0.x == v1.x && v0.y == v1.y && v0.z == v1.z;
}

bool is_normalised(Vector3 v)
{
	return almost_one(length(v));
}

Vector3 make_vector3(Vector2 v)
{
	return {v.x, v.y, 0.0f};
}

Vector4 make_vector4(Vector3 v)
{
	return {v.x, v.y, v.z, 0.0f};
}

Vector3 set_all_vector3(float x)
{
	return {x, x, x};
}

Vector3 extract_vector3(Vector4 v)
{
	return {v.x, v.y, v.z};
}

// Quaternions..................................................................

extern const Quaternion quaternion_identity = {1.0f, 0.0f, 0.0f, 0.0f};

Quaternion operator * (Quaternion q0, Quaternion q1)
{
	// In terms of the quaternions' vector and scalar parts, multiplication is:
	// qq′=[v,w][v′,w′]
	//    ≝[v×v′+wv′+w′v,ww′-v⋅v′]

	Quaternion result;
	result.w = (q0.w * q1.w) - ((q0.x * q1.x) + (q0.y * q1.y) + (q0.z * q1.z));
	result.x = ((q0.y * q1.z) - (q0.z * q1.y)) + (q0.w * q1.x) + (q1.w * q0.x);
	result.y = ((q0.z * q1.x) - (q0.x * q1.z)) + (q0.w * q1.y) + (q1.w * q0.y);
	result.z = ((q0.x * q1.y) - (q0.y * q1.x)) + (q0.w * q1.z) + (q1.w * q0.z);
	return result;
}

Vector3 operator * (Quaternion q, Vector3 v)
{
    Vector3 vector_part = {q.x, q.y, q.z};
    Vector3 t = 2.0f * cross(vector_part, v);
    return v + (q.w * t) + cross(vector_part, t);
}

Quaternion& operator /= (Quaternion& q, float s)
{
	q.w /= s;
	q.x /= s;
	q.y /= s;
	q.z /= s;
	return q;
}

float norm(Quaternion q)
{
	float result = sqrt((q.w * q.w) + (q.x * q.x) + (q.y * q.y) + (q.z * q.z));
	return result;
}

Quaternion conjugate(Quaternion q)
{
	Quaternion result;
	result.w = q.w;
	result.x = -q.x;
	result.y = -q.y;
	result.z = -q.z;
	return result;
}

Quaternion axis_angle_rotation(Vector3 axis, float angle)
{
	ASSERT(isfinite(angle));

	angle /= 2.0f;
	float phase = sin(angle);
	Vector3 v = normalise(axis);

	Quaternion result;
	result.w = cos(angle);
	result.x = v.x * phase;
	result.y = v.y * phase;
	result.z = v.z * phase;
	// Rotations must be unit quaternions.
	ASSERT(almost_one(norm(result)));
	return result;
}

// Matrices.....................................................................

extern const Matrix3 matrix3_identity =
{{
	1.0f, 0.0f, 0.0f,
	0.0f, 1.0f, 0.0f,
	0.0f, 0.0f, 1.0f
}};

Vector2 operator * (Matrix3 m, Vector3 v)
{
	Vector2 result;
	result.x = (m[0] * v.x) + (m[1] * v.y) + (m[2] * v.z);
	result.y = (m[3] * v.x) + (m[4] * v.y) + (m[5] * v.z);
	return result;
}

Matrix3 transpose(Matrix3 m)
{
	return
	{{
		m[0], m[3], m[6],
		m[1], m[4], m[7],
		m[2], m[5], m[8],
	}};
}

Matrix3 orthogonal_basis(Vector3 v)
{
	Matrix3 result;
	float l = (v.x * v.x) + (v.y * v.y);
	if(!almost_zero(l))
	{
		float d = sqrt(l);
		Vector3 n = {v.y / d, -v.x / d, 0.0f};
		result[0] = n.x;
		result[1] = n.y;
		result[2] = n.z;
		result[3] = (-v.z * n.y);
		result[4] = (v.z * n.x);
		result[5] = (v.x * n.y) - (v.y * n.x);
	}
	else
	{
		if(v.z < 0.0f)
		{
			result[0] = -1.0f;
		}
		else
		{
			result[0] = 1.0f;
		}
		result[1] = 0.0f;
		result[2] = 0.0f;
		result[3] = 0.0f;
		result[4] = 1.0f;
		result[5] = 0.0f;
	}
	result[6] = v.x;
	result[7] = v.y;
	result[8] = v.z;
	return result;
}

extern const Matrix4 matrix4_identity =
{{
	1.0f, 0.0f, 0.0f, 0.0f,
	0.0f, 1.0f, 0.0f, 0.0f,
	0.0f, 0.0f, 1.0f, 0.0f,
	0.0f, 0.0f, 0.0f, 1.0f
}};

Matrix4 operator * (const Matrix4& a, const Matrix4& b)
{
	Matrix4 result;
	for(int i = 0; i < 4; ++i)
	{
		for(int j = 0; j < 4; ++j)
		{
			float m = 0.0f;
			for(int w = 0; w < 4; ++w)
			{
				 m += a[4 * i + w] * b[4 * w + j];
			}
			result[4 * i + j] = m;
		}
	}
	return result;
}

Vector3 transform_point(const Matrix4& m, Vector3 v)
{
	float a = (m[12] * v.x) + (m[13] * v.y) + (m[14] * v.z) + m[15];

	Vector3 result;
	result.x = ((m[0] * v.x) + (m[1] * v.y) + (m[2]  * v.z) + m[3])  / a;
	result.y = ((m[4] * v.x) + (m[5] * v.y) + (m[6]  * v.z) + m[7])  / a;
	result.z = ((m[8] * v.x) + (m[9] * v.y) + (m[10] * v.z) + m[11]) / a;
	return result;
}

Vector3 transform_vector(const Matrix4& m, Vector3 v)
{
	float a = (m[12] * v.x) + (m[13] * v.y) + (m[14] * v.z) + 1.0f;

	Vector3 result;
	result.x = ((m[0] * v.x) + (m[1] * v.y) + (m[2]  * v.z)) / a;
	result.y = ((m[4] * v.x) + (m[5] * v.y) + (m[6]  * v.z)) / a;
	result.z = ((m[8] * v.x) + (m[9] * v.y) + (m[10] * v.z)) / a;
	return result;
}

Matrix4 transpose(const Matrix4& m)
{
	return
	{{
		m[0], m[4], m[8],  m[12],
		m[1], m[5], m[9],  m[13],
		m[2], m[6], m[10], m[14],
		m[3], m[7], m[11], m[15]
	}};
}

Matrix4 view_matrix(Vector3 x_axis, Vector3 y_axis, Vector3 z_axis, Vector3 position)
{
	Matrix4 result;

	result[0]  = x_axis.x;
	result[1]  = x_axis.y;
	result[2]  = x_axis.z;
	result[3]  = -dot(x_axis, position);

	result[4]  = y_axis.x;
	result[5]  = y_axis.y;
	result[6]  = y_axis.z;
	result[7]  = -dot(y_axis, position);

	result[8]  = z_axis.x;
	result[9]  = z_axis.y;
	result[10] = z_axis.z;
	result[11] = -dot(z_axis, position);

	result[12] = 0.0f;
	result[13] = 0.0f;
	result[14] = 0.0f;
	result[15] = 1.0f;

	return result;
}

// This function assumes a right-handed coordinate system.
Matrix4 look_at_matrix(Vector3 position, Vector3 target, Vector3 world_up)
{
	Vector3 forward = normalise(position - target);
	Vector3 right = normalise(cross(world_up, forward));
	Vector3 up = normalise(cross(forward, right));
	return view_matrix(right, up, forward, position);
}

// This function assumes a right-handed coordinate system.
Matrix4 turn_matrix(Vector3 position, float yaw, float pitch, Vector3 world_up)
{
	Vector3 facing;
	facing.x = cos(pitch) * cos(yaw);
	facing.y = cos(pitch) * sin(yaw);
	facing.z = sin(pitch);
	Vector3 forward = normalise(facing);
	Vector3 right = normalise(cross(world_up, forward));
	Vector3 up = normalise(cross(forward, right));
	return view_matrix(right, up, forward, position);
}

// This transforms from a right-handed coordinate system to OpenGL's default
// clip space. A position will be viewable in this clip space if its x, y, and
// z components are in the range [-w,w] of its w component.
Matrix4 perspective_projection_matrix(float fovy, float width, float height, float near_plane, float far_plane)
{
	float coty = 1.0f / tan(fovy / 2.0f);
	float aspect_ratio = width / height;
	float neg_depth = near_plane - far_plane;

	Matrix4 result;

	result[0] = coty / aspect_ratio;
	result[1] = 0.0f;
	result[2] = 0.0f;
	result[3] = 0.0f;

	result[4] = 0.0f;
	result[5] = coty;
	result[6] = 0.0f;
	result[7] = 0.0f;

	result[8] = 0.0f;
	result[9] = 0.0f;
	result[10] = (near_plane + far_plane) / neg_depth;
	result[11] = 2.0f * near_plane * far_plane / neg_depth;

	result[12] = 0.0f;
	result[13] = 0.0f;
	result[14] = -1.0f;
	result[15] = 0.0f;

	return result;
}

// This transforms from a right-handed coordinate system to OpenGL's default
// clip space. A position will be viewable in this clip space if its x, y,
// and z components are in the range [-w,w] of its w component.
Matrix4 orthographic_projection_matrix(float width, float height, float near_plane, float far_plane)
{
	float neg_depth = near_plane - far_plane;

	Matrix4 result;

	result[0] = 2.0f / width;
	result[1] = 0.0f;
	result[2] = 0.0f;
	result[3] = 0.0f;

	result[4] = 0.0f;
	result[5] = 2.0f / height;
	result[6] = 0.0f;
	result[7] = 0.0f;

	result[8] = 0.0f;
	result[9] = 0.0f;
	result[10] = 2.0f / neg_depth;
	result[11] = (far_plane + near_plane) / neg_depth;

	result[12] = 0.0f;
	result[13] = 0.0f;
	result[14] = 0.0f;
	result[15] = 1.0f;

	return result;
}

Matrix4 dilation_matrix(Vector3 dilation)
{
	return
	{{
		dilation.x, 0.0f, 0.0f, 0.0f,
		0.0f, dilation.y, 0.0f, 0.0f,
		0.0f, 0.0f, dilation.z, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f,
	}};
}

Matrix4 compose_transform(Vector3 position, Quaternion orientation, Vector3 scale)
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

	result[0]  = (1.0f - 2.0f * (yy + zz)) * scale.x;
	result[1]  = (       2.0f * (xy + zw)) * scale.y;
	result[2]  = (       2.0f * (xz - yw)) * scale.z;
	result[3]  = position.x;

	result[4]  = (       2.0f * (xy - zw)) * scale.x;
	result[5]  = (1.0f - 2.0f * (xx + zz)) * scale.y;
	result[6]  = (       2.0f * (yz + xw)) * scale.z;
	result[7]  = position.y;

	result[8]  = (       2.0f * (xz + yw)) * scale.x;
	result[9]  = (       2.0f * (yz - xw)) * scale.y;
	result[10] = (1.0f - 2.0f * (xx + yy)) * scale.z;
	result[11] = position.z;

	result[12] = 0.0f;
	result[13] = 0.0f;
	result[14] = 0.0f;
	result[15] = 1.0f;

	return result;
}

Matrix4 inverse_view_matrix(const Matrix4& m)
{
	float a = -((m[0] * m[3]) + (m[4] * m[7]) + (m[8]  * m[11]));
	float b = -((m[1] * m[3]) + (m[5] * m[7]) + (m[9]  * m[11]));
	float c = -((m[2] * m[3]) + (m[6] * m[7]) + (m[10] * m[11]));
	return
	{{
		m[0], m[4], m[8],  a,
		m[1], m[5], m[9],  b,
		m[2], m[6], m[10], c,
		0.0f, 0.0f, 0.0f,  1.0f
	}};
}

Matrix4 inverse_perspective_matrix(const Matrix4& m)
{
	float m0  = 1.0f / m[0];
	float m5  = 1.0f / m[5];
	float m14 = 1.0f / m[11];
	float m15 = m[10] / m[11];
	return
	{{
		m0,   0.0f, 0.0f,  0.0f,
		0.0f, m5,   0.0f,  0.0f,
		0.0f, 0.0f, 0.0f, -1.0f,
		0.0f, 0.0f, m14,   m15
	}};
}

Matrix4 inverse_orthographic_matrix(const Matrix4& m)
{
	float m0 = 1.0f / m[0];
	float m5 = 1.0f / m[5];
	float m10 = 1.0f / m[10];
	float m11 = m[11] / m[10];
	return
	{{
		m0,   0.0f, 0.0f, 0.0f,
		0.0f, m5,   0.0f, 0.0f,
		0.0f, 0.0f, m10,  m11,
		0.0f, 0.0f, 0.0f, 1.0f
	}};
}

Matrix4 inverse_transform(const Matrix4& m)
{
	// The scale can be extracted from the rotation data by just taking the
	// length of the first three row vectors.

	float dx = sqrt(m[0] * m[0] + m[4] * m[4] + m[8]  * m[8]);
	float dy = sqrt(m[1] * m[1] + m[5] * m[5] + m[9]  * m[9]);
	float dz = sqrt(m[2] * m[2] + m[6] * m[6] + m[10] * m[10]);

	// The extracted scale can then be divided out to isolate the rotation rows.

	float m00 = m[0] / dx;
	float m10 = m[4] / dx;
	float m20 = m[8] / dx;

	float m01 = m[1] / dy;
	float m11 = m[5] / dy;
	float m21 = m[9] / dy;

	float m02 = m[2] / dz;
	float m12 = m[6] / dz;
	float m22 = m[10] / dz;

	// The inverse of the translation elements is the negation of the
	// translation vector multiplied by the transpose of the rotation and the
	// reciprocal of the dilation.

	float a = -(m00 * m[3] + m10 * m[7] + m20 * m[11]) / dx;
	float b = -(m01 * m[3] + m11 * m[7] + m21 * m[11]) / dy;
	float c = -(m02 * m[3] + m12 * m[7] + m22 * m[11]) / dz;

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

	return
	{{
		m00, m10, m20, a,
		m01, m11, m21, b,
		m02, m12, m22, c,
		0.0f, 0.0f, 0.0f, 1.0f
	}};
}
