Vector Math
===========


Vectors
-------

.. c:type:: Float2

    A Float2 is a couple of floating-point numbers.

.. c:type:: Float3

    A Float3 is a triplet of floating-point numbers.

.. c:type:: Float4

    A Float4 is a quartet of floating-point numbers.

.. c:var:: const Float2 float2_minus_infinity

    A vector whose components are each negative infinity
    :math:`\langle -\infty, -\infty \rangle`.

.. c:var:: const Float2 float2_one

    The ones vector :math:`\langle 1, 1 \rangle`.

.. c:var:: const Float2 float2_plus_infinity

    A vector whose components are each infinity
    :math:`\langle \infty, \infty \rangle`.

.. c:var:: const Float2 float2_zero

    The zero vector :math:`\langle 0, 0 \rangle`.

.. c:var:: const Float3 float3_minus_infinity

    A vector whose components are each negative infinity
    :math:`\langle -\infty, -\infty, -\infty \rangle`.

.. c:var:: const Float3 float3_one

    The ones vector :math:`\langle 1, 1, 1 \rangle`.

.. c:var:: const Float3 float3_plus_infinity

    A vector whose components are each infinity
    :math:`\langle \infty, \infty, \infty \rangle`.

.. c:var:: const Float3 float3_unit_x

    The unit length X-axis vector :math:`\langle 1, 0, 0 \rangle`.

.. c:var:: const Float3 float3_unit_y

    The unit length Y-axis vector :math:`\langle 0, 1, 0 \rangle`.

.. c:var:: const Float3 float3_unit_z

    The unit length Z-axis vector :math:`\langle 0, 0, 1 \rangle`.

.. c:var:: const Float3 float3_zero

    The zero vector :math:`\langle 0, 0, 0 \rangle`.

.. c:function:: Float2 float2_add(Float2 a, Float2 b)
        Float3 float3_add(Float3 a, Float3 b)

    Add two vectors.

    :param a: the first addend
    :param b: the second addend
    :return: a sum

.. c:function:: void float2_add_assign(Float2* a, Float2 b)

    Add a vector to another in-place.

    :param a: the first addend

            The resulting sum is stored in its place.
    :param b: the second addend

.. c:function:: float float3_angle_between(Float3 a, Float3 b)

    Determine the angle between two vectors.

    :param a: the first vector
    :param b: the second vector
    :return: an angle in radians

.. c:function:: Float3 float3_cross(Float3 a, Float3 b)

    Produce the cross product of two vectors.

    The cross product is orthogonal to both vectors, except when they're
    parallel, when it produces the zero vector.

    :param a: the first vector
    :param b: the second vector
    :return: a product

.. c:function:: float float2_determinant(Float2 a, Float2 b)

    Produce the determinant of a 2x2 matrix where the vectors are
    its two rows. Using the Leibniz formula, this is:

    :math:`\begin{vmatrix} a_0 a_1 \\ b_0 b_1 \end{vmatrix} = a_0 b_1 - a_1 b_0`

    This is equivalent to taking the magnitude of the bivector
    :math:`a \wedge b`, where :math:`\wedge` is the outer product.

    :param a: the first row
    :param b: the second row
    :return: a determinant

.. c:function:: float float3_distance(Float3 a, Float3 b)

    Produce the distance between two points.

    :param a: the first point
    :param b: the second point
    :return: a distance

.. c:function:: Float2 float2_divide(Float2 v, float s)
        Float3 float3_divide(Float3 v, float s)

    Divide a vector by a scalar.

    :param v: the dividend
    :param s: the divisor

        This must be nonzero.
    :return: a quotient

.. c:function:: float float2_dot(Float2 a, Float2 b)
        float float3_dot(Float3 a, Float3 b)

    Take the dot product, or inner product, of two vectors. This is the sum
    of the product of their corresponding components.

    Given :math:`a=\langle a_0, a_1, \dots, a_n \rangle` and 
    :math:`b=\langle b_0, b_1, \dots, a_n \rangle`, the dot product
    :math:`a \cdot b` is
    :math:`\displaystyle\sum_{i=1}^{n} a_0 b_0 + a_1 b_1 + \dots + a_n b_n`.

    Orthogonal vectors have a dot product of 0. Vectors pointing in a similar
    direction have a positive dot product, and a negative dot product when when
    pointing away from one another.

    :param a: the first vector
    :param b: the second vector
    :return: a product

.. c:function:: bool float2_exactly_equals(Float2 a, Float2 b)
        bool float3_exactly_equals(Float3 a, Float3 b)

    Check whether two vectors are exactly equal.

    :param a: a vector
    :param b: another vector
    :return: true if each pair of corresponding components of the vectors are
            all equal

.. c:function:: Float2 float3_extract_float2(Float3 v)

    Make a 2D vector with the first two components of a 3D vector.

    :param v: the 3D vector
    :return: a 2D vector

.. c:function:: Float3 float4_extract_float3(Float4 v)

    Make a 3D vector with the first three components of a 4D vector.

    :param v: the 4D vector
    :return: a 3D vector

.. c:function:: bool float3_is_normalised(Float3 v)

    Check if a vector is normalised, meaning its length is 1.

    :param v: the vector
    :return: true if it is normalised

.. c:function:: float float2_length(Float2 v)
        float float3_length(Float3 v)

    Produce the length, or magnitude, of a vector.

    :param v: the vector
    :return: a length

.. c:function:: Float2 float2_lerp(Float2 a, Float2 b, float t)
        Float3 float3_lerp(Float3 a, Float3 b, float t)

    Linearly interpolate between two points. Produce a point which is some
    fraction of the way along a line from one point to the other.

    :param a: the first point
    :param b: the second point
    :param t: the interpolant between 0 and 1 inclusive

            A value of 0 results in the first point. A value of 0.1 produces
            a point near the first point. 0.5 would be halfway between the
            points, and 1 would produce the second point.
    :return: a point between the two given points

.. c:function:: Float3 float3_madd(float a, Float3 b, Float3 c)

    Multiply by a scalar and add a vector. This is just :math:`ab + c`.

    :param a: the multiplier
    :param b: the multiplicand
    :param c: the second addend
    :return: a vector

.. c:function:: Float3 float2_make_float3(Float2 v)

    Make a 3D vector from a 2D one so that the result is
    :math:`\langle v_0, v_1, 0 \rangle`.

    :param v: the 2D vector
    :return: a 3D vector

.. c:function:: Float4 float3_make_float4(Float3 v)

    Make a 4D vector from a 3D one so that the result is
    :math:`\langle v_0, v_1, v_2, 0 \rangle`.

    :param v: the 3D vector
    :return: a 4D vector

.. c:function:: Float2 float2_max(Float2 a, Float2 b)
        Float3 float3_max(Float3 a, Float3 b)

    Produce a maximum vector where each component is the larger of the
    corresponding components of two vectors.

    :param a: the first vector
    :param b: the second vector
    :return: a maximum vector

.. c:function:: Float2 float2_min(Float2 a, Float2 b)
    Float3 float3_min(Float3 a, Float3 b)

    Produce a minimum vector where each component is the smaller of the
    corresponding components of two vectors.

    :param a: the first vector
    :param b: the second vector
    :return: a minimum vector

.. c:function:: Float2 float2_multiply(float s, Float2 v)
        Float3 float3_multiply(float s, Float3 v)

    Multiply a vector by a scalar.

    :param s: the multiplier
    :param v: the multiplicand
    :return: a product

.. c:function:: Float2 float2_negate(Float2 v)
        Float3 float3_negate(Float3 v)

    Negate a vector.

    :param v: a vector
    :return: a vector of equal length and opposite direction

.. c:function:: Float2 float2_normalise(Float2 v)
        Float3 float3_normalise(Float3 v)

    Produce a normalised vector in the same direction as the given vector.

    :param v: a vector with finite and nonzero length
    :return: a unit length vector

.. c:function:: Float2 float2_perp(Float2 v)
        Float3 float3_perp(Float3 v)

    Produce a vector perpendicular to the given vector.

    :param v: the vector
    :return: a perpendicular vector

.. c:function:: Float2 float2_pointwise_divide(Float2 a, Float2 b)
        Float3 float3_pointwise_divide(Float3 a, Float3 b)

    Divide one vector by another such that the quotient's components are the
    corresponding component in the first vector divided by the same component in
    the second vector.

    :param a: the dividend
    :param b: the divisor

            None of its components can be zero.
    :return: a quotient

.. c:function:: Float2 float2_pointwise_multiply(Float2 a, Float2 b)
        Float3 float3_pointwise_multiply(Float3 a, Float3 b)

    Multiply two vectors such that the product's components are the product of
    the corresponding components in the multiplicands.

    :param a: the first multiplicand
    :param b: the second multiplicand
    :return: a product

.. c:function:: Float3 float3_project(Float3 a, Float3 b)

    Project a vector onto a line parallel to another vector.

    This uses the formula :math:`\displaystyle\frac{a \cdot b}{b \cdot b}b`.

    :param a: the vector to project
    :param b: the vector to project onto
    :return: a projection

.. c:function:: Float3 float3_reciprocal(Float3 v)

    Produce a vector whose components are the reciprocal :math:`1/v_i` of the
    corresponding components in the given vector.

    :param v: the vector
    :return: a reciprocal vector

.. c:function:: Float3 float3_reject(Float3 a, Float3 b)

    Project a vector onto a plane orthogonal to another vector.

    This uses the formula :math:`a - \displaystyle\frac{a \cdot b}{b \cdot b}b`.

    The rejection can be used to get the distance from a point to a line.
    If ``a`` is a point and ``b`` is vector along a line, the length of the
    rejection is the distance.

    :param a: the first vector
    :param a: the second vector
    :return: a rejection

.. c:function:: Float3 float3_set_all(float x)

    Make a vector where all components are a given number.

    :param x: the number
    :return: a vector

.. c:function:: float float2_squared_distance(Float2 a, Float2 b)
        float float3_squared_distance(Float3 a, Float3 b)

    Produce the square of the distance between two points.

    :param a: the first point
    :param b: the second point
    :return: the square of the distance

.. c:function:: float float2_squared_length(Float2 v)
        float float3_squared_length(Float3 v)

    Produce the square of the length of the vector.
    
    :param v: the vector
    :return: the square of the length

.. c:function:: Float2 float2_subtract(Float2 a, Float2 b)
        Float3 float3_subtract(Float3 a, Float3 b)

    Subtract one vector from another.

    :param a: the minuend
    :param b: the subtrahend
    :return: a difference


Matrices
--------

.. c:type:: Matrix3

    A Matrix3 is a 3x3 square matrix.

.. c:type:: Matrix4

    A Matrix4 is a 4x4 square matrix.

.. c:var:: Matrix3 matrix3_identity

    The identity 3x3 matrix.

.. c:var:: Matrix4 matrix4_identity

    The identity 4x4 matrix.

.. c:function:: Matrix3 matrix3_orthogonal_basis(Float3 v)

    Produce an orthogonal basis of vectors orthogonal to a given vector.
    
    Since there's an infinite number of orthogonal vectors to any given one,
    this picks one pair to form the other axes of the basis arbitrarily.

    :param v: a vector
    :return: an orthogonal basis

.. c:function:: Float2 matrix3_transform(Matrix3 m, Float3 v)

    Transform a point to obtain a 2D point.

    :param m: the transform
    :param v: a point
    :return: a transformed point

.. c:function:: Matrix3 matrix3_transpose(Matrix3 m)

    Produce a transposed matrix. This flips the matrix across its diagonal or,
    equivalently, swaps each row with its corresponding column.

    :param m: the matrix
    :return: a transposed matrix

.. c:function:: Matrix4 matrix4_compose_transform(Float3 position, \
        Quaternion orientation, Float3 scale)

    Produce an affine transformation matrix.

    :param position: a position or translation
    :param orientation: an orientation or rotation
    :param scale: a scale or dilation
    :return: a transform

.. c:function:: Matrix4 matrix4_dilation(Float3 dilation)

    Produce a dilation transform, which scales a vector when applied to it.

    :dilation: the dilation amount along each axis
    :return: a dilation transform

.. c:function:: Matrix4 matrix4_inverse_view(Matrix4 m)

    Produce the inverse of a view transform.

    :param m: the view transform
    :return: an inverse

.. c:function:: Matrix4 matrix4_inverse_perspective(Matrix4 m)

    Produce the inverse of a perspective projection.

    :param m: the perspective projection
    :return: an inverse projection

.. c:function:: Matrix4 matrix4_inverse_orthographic(Matrix4 m)

    Produce the inverse of an orthographic projection.

    :param m: the orthographic projection
    :return: an inverse projection

.. c:function:: Matrix4 matrix4_inverse_transform(Matrix4 m)

    Produce the inverse of an affine transform.

    :param m: the affine transform
    :return: an inverse

.. c:function:: Matrix4 matrix4_look_at(Float3 position, Float3 target, \
        Float3 world_up)

    Produce a view transform. This is used to transform positions from world
    space into view space.

    This function assumes a right-handed coordinate system.

    :param position: a camera position
    :param position: a focal target position
    :param world_up: the world-space up axis
    :return: a view transform

.. c:function:: Matrix4 matrix4_multiply(Matrix4 a, Matrix4 b)

    Multiply two matrices. This operation is noncommutative, meaning
    :math:`ab ≠ ba`.

    :param a: the multiplier
    :param b: the multiplicand
    :return: a product

.. c:function:: Matrix4 matrix4_orthographic_projection(float width, \
        float height, float near_plane, float far_plane)

    Produce an orthographic projection. This is used to transform positions from
    view space into clip space.

    This function assumes a right-handed coordinate system

    :param width: the distance between the left and right clipping planes
    :param height: the distance between the top and bottom clipping planes
    :param near_plane: the distance from the viewer to the near clipping plane
    :param far_plane: the distance from the viewer to the far clipping plane
    :return: an orthographic projection

.. c:function:: Matrix4 matrix4_perspective_projection(float fovy, \
        float width, float height, float near_plane, float far_plane)

    Produce an orthographic projection. This is used to transform positions from
    view space into clip space.

    This function assumes a right-handed coordinate system

    :param fovy: the vertical angular field-of-view in radians
    :param width: the distance between left and right clipping planes
    :param height: the distance between top and bottom clipping planes
    :param near_plane: the distance from the viewer to the near clipping plane
    :param far_plane: the distance from the viewer to the far clipping plane
    :return: a perspective projection

.. c:function:: Float3 matrix4_transform_point(Matrix4 m, Float3 v)

    Transform a point.

    :param m: the transformation
    :param v: a point
    :return: a transformed point

.. c:function:: Float3 matrix4_transform_vector(Matrix4 m, Float3 v)

    Transform a vector.

    :param m: the transformation
    :param v: a vector
    :return: a transformed vector

.. c:function:: Matrix4 matrix4_translation(Float3 translation)

    Produce a translation transform, which moves a point when applied to it.

    :param translation: a translation vector
    :return: a translation transform

.. c:function:: Matrix4 matrix4_transpose(Matrix4 m)

    Transpose a matrix. This flips the matrix across its diagonal or,
    equivalently, swaps each row with its corresponding column.

    :param m: the matrix
    :return: a transposed matrix

.. c:function:: Matrix4 matrix4_turn(Float3 position, float yaw, float pitch, \
        Float3 world_up)

    Produce a view transform. This is used to transform positions from world
    space into view space.

    This function assumes a right-handed coordinate system.

    :param position: the camera position
    :param yaw: the angle of rotation in radians about the vertical axis
    :param pitch: the angle of rotation in radians about the horizontal axis
    :param world_up: the world-space up axis
    :return: a view transform

.. c:function:: Matrix4 matrix4_view(Float3 x_axis, Float3 y_axis, \
        Float3 z_axis, Float3 position)

    Produce a view transform. This is used to transform positions from world
    space into view space.

    :param x_axis: the X-axis of the orthonormal basis
    :param y_axis: the Y-axis of the orthonormal basis
    :param z_axis: the Z-axis of the orthonormal basis
    :param position: the position of the view-space origin in world-space
    :return: a view transform


Quaternion
----------

.. c:type:: Quaternion

    A quaternion is a four-dimensional number system. Generally they're used
    to represent rotations and orientations. In particular, those used for
    rotation are expected to be unit quaternions or *versors*, meaning their
    norm is 1.

.. c:var:: Quaternion quaternion_identity

    The multiplicative identity quaternion. It is :math:`1 + 0i + 0j + 0k`.

.. c:function:: Quaternion quaternion_axis_angle(Float3 axis, float angle)

    Create a quaternion representing a rotation given an axis and an angle.

    :param axis: the axis of rotation
    :param angle: the number of radians to rotate around the axis
    :return: a rotation

.. c:function:: Quaternion quaternion_conjugate(Quaternion q)

    Conjugate a quaternion. Conjugation gives a quaternion :math:`q^*` that,
    when multiplied by the original quaternion :math:`q`, produces a scalar
    :math:`s` like :math:`q^*q = qq^* = s`.

    When the quaternion represents a rotation, its conjugate represents an
    inverse rotation.

    :param q: the quaternion
    :return: a conjugate

.. c:function:: Quaternion quaternion_divide(Quaternion q, float s)

    Divide a quaternion by a scalar.

    :param q: the quaternion
    :param s: a scalar divisor
    :return: a quotient

.. c:function:: Quaternion quaternion_multiply(Quaternion a, Quaternion b)

    Multiply two quaternions. This operation is noncommutative, meaning
    :math:`ab ≠ ba`.

    :param a: the multiplier
    :param b: the multiplicand
    :return: a product

.. c:function:: float quaternion_norm(Quaternion q)

    Compute the norm of the quaternion, which is the formula
    :math:`\sqrt{w^2 + x^2 + y^2 + z^2}`.

    A quaternion divided by its norm produces a unit quaternion, or *versor*.

    :param q: the quaternion
    :return: its norm

.. c:function:: Float3 quaternion_rotate(Quaternion q, Float3 v)

    Rotate a vector.

    :param q: the rotation
    :param v: the vector
    :return: a rotated vector

