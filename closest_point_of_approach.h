#ifndef CLOSEST_POINT_OF_APPROACH_H_
#define CLOSEST_POINT_OF_APPROACH_H_

#if defined(__cplusplus)
extern "C" {
#endif

#include "intersection.h"
#include "vector_math.h"

float distance_point_plane(Float3 point, Float3 origin, Float3 normal);

Float3 project_onto_plane(Float3 point, Float3 origin, Float3 normal);

Float3 closest_disk_point(Disk disk, Float3 point);
Float3 closest_disk_plane(Disk disk, Float3 origin, Float3 normal);
Float3 closest_point_on_line(Ray ray, Float3 start, Float3 end);
Float3 closest_ray_point(Ray ray, Float3 point);

#if defined(__cplusplus)
} // extern "C"
#endif

#endif // CLOSEST_POINT_OF_APPROACH_H_
