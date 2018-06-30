#ifndef CAMERA_H_
#define CAMERA_H_

#if defined(__cplusplus)
extern "C" {
#endif

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

Float2 viewport_point_to_ndc(Float2 point, Int2 viewport);
Ray ray_from_viewport_point(Float2 point, Int2 viewport, Matrix4 view, Matrix4 projection, bool orthographic);

#if defined(__cplusplus)
} // extern "C"
#endif

#endif // CAMERA_H_
