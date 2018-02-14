#ifndef INTERSECTION_H_
#define INTERSECTION_H_

#include "vector_math.h"

struct Cone
{
	Vector3 base_center;
	Vector3 axis;
	float radius;
};

struct Cylinder
{
	Vector3 start;
	Vector3 end;
	float radius;
};

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
Ray transform_ray(Ray ray, Matrix4 transform);
bool intersect_ray_cylinder(Ray ray, Cylinder cylinder, Vector3* intersection);
bool intersect_ray_cone(Ray ray, Cone cone, Vector3* intersection);

#endif // INTERSECTION_H_
