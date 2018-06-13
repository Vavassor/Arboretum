#ifndef TOOLS_H_
#define TOOLS_H_

#include "vector_math.h"

struct MoveTool
{
    Quaternion orientation;
    Vector3 position;
    Vector3 reference_position;
    Vector3 reference_offset;
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
};

struct RotateTool
{
    float angles[3];
    Vector3 position;
    float radius;
};

#endif // TOOLS_H_
