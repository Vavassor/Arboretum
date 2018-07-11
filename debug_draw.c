#include "debug_draw.h"

#include "assert.h"

DebugDraw debug_draw;

void debug_draw_reset()
{
    debug_draw.spheres_count = 0;
}

void debug_draw_add_sphere(Sphere sphere)
{
    ASSERT(debug_draw.spheres_count < DEBUG_DRAW_SPHERE_CAP);
    debug_draw.spheres[debug_draw.spheres_count] = sphere;
    debug_draw.spheres_count += 1;
}
