#ifndef INTERSECTION_H_
#define INTERSECTION_H_

#include "vector_math.h"

struct Box
{
    Quaternion orientation;
    Vector3 center;
    Vector3 extents;
};

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

struct Sphere
{
    Vector3 center;
    float radius;
};

struct Torus
{
    Vector3 center;
    Vector3 axis;
    float major_radius;
    float minor_radius;
};

struct Stack;

namespace jan {

struct Mesh;
struct Face;

Face* first_face_hit_by_ray(Mesh* mesh, Ray ray, float* face_distance, Stack* stack);

} // namespace jan

bool point_in_polygon(Vector2 point, Vector2* vertices, int vertices_count);
float distance_point_plane(Vector3 point, Vector3 origin, Vector3 normal);
Ray transform_ray(Ray ray, Matrix4 transform);
bool intersect_ray_plane(Ray ray, Vector3 origin, Vector3 normal, Vector3* intersection);
bool intersect_ray_sphere(Ray ray, Sphere sphere, Vector3* intersection);
bool intersect_ray_cylinder(Ray ray, Cylinder cylinder, Vector3* intersection);
bool intersect_ray_cone(Ray ray, Cone cone, Vector3* intersection);
bool intersect_ray_torus(Ray ray, Torus torus, Vector3* intersection);
bool intersect_ray_box(Ray ray, Box box, Vector3* intersection);

Vector3 project_onto_plane(Vector3 v, Vector3 normal);
Vector3 closest_point_on_line(Ray ray, Vector3 start, Vector3 end);
Vector3 closest_ray_point(Ray ray, Vector3 point);

#endif // INTERSECTION_H_
