#ifndef CAMERA_H_
#define CAMERA_H_

#include "intersection.h"
#include "int2.h"
#include "vector_math.h"

typedef struct Camera
{
    Float3 position;
    Float3 target;
    float field_of_view;
    float near_plane;
    float far_plane;
} Camera;

Matrix4 camera_get_view(Camera* camera);
Matrix4 camera_get_projection(Camera* camera, Int2 viewport);
Ray camera_get_ray(Camera* camera, Float2 point, Int2 viewport);
Float2 viewport_point_to_ndc(Float2 point, Int2 viewport);
Ray ray_from_viewport_point(Float2 point, Int2 viewport, Matrix4 view, Matrix4 projection, bool orthographic);

#endif // CAMERA_H_
