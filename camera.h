#ifndef CAMERA_H_
#define CAMERA_H_

#include "vector_math.h"
#include "intersection.h"

struct Camera
{
    Vector3 position;
    Vector3 target;
    float field_of_view;
    float near_plane;
    float far_plane;
};

struct Viewport
{
    int width;
    int height;
};

Ray ray_from_viewport_point(Vector2 point, Viewport viewport, Matrix4 view, Matrix4 projection, bool orthographic);

#endif // CAMERA_H_
