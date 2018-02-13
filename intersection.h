#ifndef INTERSECTION_H_
#define INTERSECTION_H_

#include "vector_math.h"

struct Ray
{
	Vector3 origin;
	Vector3 direction;
};

namespace jan {

struct Mesh;
struct Face;

Face* first_face_hit_by_ray(Mesh* mesh, Ray ray, float* face_distance);

} // namespace jan

bool point_in_polygon(Vector2 point, Vector2* vertices, int vertices_count);
float distance_point_plane(Vector3 point, Vector3 origin, Vector3 normal);

#endif // INTERSECTION_H_
