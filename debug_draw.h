#ifndef DEBUG_DRAW_H_
#define DEBUG_DRAW_H_

#include "intersection.h"

#define DEBUG_DRAW_SPHERE_CAP 8

typedef struct DebugDraw
{
    Sphere spheres[DEBUG_DRAW_SPHERE_CAP];
    int spheres_count;
} DebugDraw;

void debug_draw_reset();
void debug_draw_add_sphere(Sphere sphere);

extern DebugDraw debug_draw;

#endif // DEBUG_DRAW_H_
