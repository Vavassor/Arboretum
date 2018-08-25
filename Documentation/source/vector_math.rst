Vector Math
===========


Vectors
-------

.. c:type:: Float2

    A Float2 is a couple of floating-point numbers.

.. c:type:: Float3

    A Float3 is a triplet of floating-point numbers.


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

    :param a: the multiplicand
    :param b: the multiplier
    :return: a product

.. c:function:: Matrix4 matrix4_orthographic_projection(float width, \
        float height, float near_plane, float far_plane)

    Produce an orthographic projection. This is used to transform positions from
    view space into clip space.

    This function assumes a right-handed coordinate system

    :param width: the distance between the left and right clipping planes
    :param height: the distance between the top and bottom clipping planes
    :near_plane: the distance from the viewer to the near clipping plane
    :far_plane: the distance from the viewer to the far clipping plane
    :return: an orthographic projection

.. c:function:: Matrix4 matrix4_perspective_projection(float fovy, \
        float width, float height, float near_plane, float far_plane)

    Produce an orthographic projection. This is used to transform positions from
    view space into clip space.

    This function assumes a right-handed coordinate system

    :fovy: the vertical angular field-of-view in radians
    :width: the width of the view
    :height: the height of the view
    :near_plane: the distance from the viewer to the near clipping plane
    :far_plane: the distance from the viewer to the far clipping plane
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

.. c:function:: Matrix4 matrix4_transpose(Matrix4 m)

    Transpose a matrix. This flips the matrix across its diagonal or,
    equivalently, swaps each row with its corresponding column.

    :param m: the matrix
    :return: a transposed matrix

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
    rotation are expected to be unit quaternions, meaning their norm is 1.

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

    :param a: the multiplicand
    :param b: the multiplier
    :return: a product

.. c:function:: float quaternion_norm(Quaternion q)

    Compute the norm of the quaternion, which is the formula
    :math:`\sqrt{w^2 + x^2 + y^2 + z^2}`.

    :param q: the quaternion
    :return: its norm

.. c:function:: Float3 quaternion_rotate(Quaternion q, Float3 v)

    Rotate a vector.

    :param q: the rotation
    :param v: the vector
    :return: a rotated vector

