#ifndef CAMERA_H_
#define CAMERA_H_

#include "intersection.h"
#include "int2.h"
#include "vector_math.h"

struct Camera
{
    Vector3 position;
    Vector3 target;
    float field_of_view;
    float near_plane;
    float far_plane;
};

Vector2 viewport_point_to_ndc(Vector2 point, Int2 viewport);
Ray ray_from_viewport_point(Vector2 point, Int2 viewport, Matrix4 view, Matrix4 projection, bool orthographic);

#endif // CAMERA_H_
