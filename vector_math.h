#ifndef VECTOR_MATH_H_
#define VECTOR_MATH_H_

// Vectors......................................................................

struct Vector2
{
	float x, y;

	float& operator [] (int index) {return reinterpret_cast<float*>(this)[index];}
	const float& operator [] (int index) const {return reinterpret_cast<const float*>(this)[index];}
};

extern const Vector2 vector2_zero;
extern const Vector2 vector2_min;
extern const Vector2 vector2_max;

Vector2 operator + (Vector2 v0, Vector2 v1);
Vector2& operator += (Vector2& v0, Vector2 v1);
Vector2 operator - (Vector2 v0, Vector2 v1);
Vector2& operator -= (Vector2& v0, Vector2 v1);
Vector2 operator * (Vector2 v, float s);
Vector2 operator * (float s, Vector2 v);
Vector2& operator *= (Vector2& v, float s);
Vector2 operator / (Vector2 v, float s);
Vector2& operator /= (Vector2& v, float s);
Vector2 operator - (Vector2 v);

Vector2 perp(Vector2 v);
float squared_length(Vector2 v);
float length(Vector2 v);
Vector2 normalise(Vector2 v);
float dot(Vector2 v0, Vector2 v1);
Vector2 pointwise_multiply(Vector2 v0, Vector2 v1);
Vector2 pointwise_divide(Vector2 v0, Vector2 v1);
Vector2 lerp(Vector2 v0, Vector2 v1, float t);
Vector2 min2(Vector2 a, Vector2 b);
Vector2 max2(Vector2 a, Vector2 b);

struct Vector3
{
	float x, y, z;

	float& operator [] (int index) {return reinterpret_cast<float*>(this)[index];}
	const float& operator [] (int index) const {return reinterpret_cast<const float*>(this)[index];}
};

extern const Vector3 vector3_zero;
extern const Vector3 vector3_one;
extern const Vector3 vector3_unit_x;
extern const Vector3 vector3_unit_y;
extern const Vector3 vector3_unit_z;
extern const Vector3 vector3_min;
extern const Vector3 vector3_max;

Vector3 operator + (Vector3 v0, Vector3 v1);
Vector3& operator += (Vector3& v0, Vector3 v1);
Vector3 operator - (Vector3 v0, Vector3 v1);
Vector3& operator -= (Vector3& v0, Vector3 v1);
Vector3 operator * (Vector3 v, float s);
Vector3 operator * (float s, Vector3 v);
Vector3& operator *= (Vector3& v, float s);
Vector3 operator / (Vector3 v, float s);
Vector3& operator /= (Vector3& v, float s);
Vector3 operator + (Vector3 v);
Vector3 operator - (Vector3 v);

float squared_length(Vector3 v);
float length(Vector3 v);
float squared_distance(Vector3 v0, Vector3 v1);
float distance(Vector3 v0, Vector3 v1);
Vector3 normalise(Vector3 v);
float dot(Vector3 v0, Vector3 v1);
Vector3 cross(Vector3 v0, Vector3 v1);
Vector3 pointwise_multiply(Vector3 v0, Vector3 v1);
Vector3 pointwise_divide(Vector3 v0, Vector3 v1);
Vector3 lerp(Vector3 v0, Vector3 v1, float t);
Vector3 reciprocal(Vector3 v);
Vector3 max3(Vector3 v0, Vector3 v1);
Vector3 min3(Vector3 v0, Vector3 v1);
Vector3 perp(Vector3 v);
bool exactly_equals(Vector3 v0, Vector3 v1);
bool is_normalised(Vector3 v);
Vector3 make_vector3(Vector2 v);

struct Vector4
{
	float x, y, z, w;

	float& operator [] (int index) {return reinterpret_cast<float*>(this)[index];}
	const float& operator [] (int index) const {return reinterpret_cast<const float*>(this)[index];}
};

Vector4 make_vector4(Vector3 v);
Vector3 extract_vector3(Vector4 v);

// Quaternions..................................................................

struct Quaternion
{
	float w, x, y, z;
};

extern const Quaternion quaternion_identity;

Quaternion operator * (Quaternion q0, Quaternion q1);
Vector3 operator * (Quaternion q, Vector3 v);
Quaternion& operator /= (Quaternion& q, float s);

float norm(Quaternion q);
Quaternion conjugate(Quaternion q);
Quaternion axis_angle_rotation(Vector3 axis, float angle);

// Matrices.....................................................................

struct Matrix3
{
	float elements[9]; // in row-major order

	float& operator [] (int index) {return elements[index];}
	const float& operator [] (int index) const {return elements[index];}
};

extern const Matrix3 matrix3_identity;

Vector2 operator * (Matrix3 m, Vector3 v);

Matrix3 transpose(Matrix3 m);
Matrix3 orthogonal_basis(Vector3 v);

struct Matrix4
{
	float elements[16]; // in row-major order

	float& operator [] (int index) {return elements[index];}
	const float& operator [] (int index) const {return elements[index];}
};

extern const Matrix4 matrix4_identity;

Matrix4 operator * (const Matrix4& a, const Matrix4& b);
Vector3 operator * (const Matrix4& m, Vector3 v);

Matrix4 transpose(const Matrix4& m);
Matrix4 view_matrix(Vector3 x_axis, Vector3 y_axis, Vector3 z_axis, Vector3 position);
Matrix4 look_at_matrix(Vector3 position, Vector3 target, Vector3 world_up);
Matrix4 turn_matrix(Vector3 position, float yaw, float pitch, Vector3 world_up);
Matrix4 perspective_projection_matrix(float fovy, float width, float height, float near_plane, float far_plane);
Matrix4 orthographic_projection_matrix(float width, float height, float near_plane, float far_plane);
Matrix4 compose_transform(Vector3 position, Quaternion orientation, Vector3 scale);
Matrix4 inverse_view_matrix(const Matrix4& m);
Matrix4 inverse_perspective_matrix(const Matrix4& m);
Matrix4 inverse_orthographic_matrix(const Matrix4& m);
Matrix4 inverse_transform(const Matrix4& m);

#endif // VECTOR_MATH_H_
