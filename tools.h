#ifndef TOOLS_H_
#define TOOLS_H_

#if defined(__cplusplus)
extern "C" {
#endif

#include "vector_math.h"

typedef struct MoveTool
{
    Quaternion orientation;
    Float3 position;
    Float3 reference_position;
    Float3 reference_offset;
    float scale;
    float shaft_length;
    float shaft_radius;
    float head_height;
    float head_radius;
    float plane_extent;
    float plane_thickness;
    int hovered_axis;
    int hovered_plane;
    int selected_axis;
    int selected_plane;
} MoveTool;

typedef struct RotateTool
{
    float angles[3];
    Float3 position;
    float radius;
} RotateTool;

#if defined(__cplusplus)
} // extern "C"
#endif

#endif // TOOLS_H_
