#ifndef CLOSEST_POINT_OF_APPROACH_H_
#define CLOSEST_POINT_OF_APPROACH_H_

#include "intersection.h"
#include "vector_math.h"

float distance_point_plane(Vector3 point, Vector3 origin, Vector3 normal);

Vector3 project_onto_plane(Vector3 point, Vector3 origin, Vector3 normal);

Vector3 closest_disk_point(Disk disk, Vector3 point);
Vector3 closest_disk_plane(Disk disk, Vector3 origin, Vector3 normal);
Vector3 closest_point_on_line(Ray ray, Vector3 start, Vector3 end);
Vector3 closest_ray_point(Ray ray, Vector3 point);

#endif // CLOSEST_POINT_OF_APPROACH_H_
