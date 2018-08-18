#ifndef INTERSECTION_H_
#define INTERSECTION_H_

#include "int2.h"
#include "jan.h"
#include "memory.h"
#include "vector_math.h"

typedef struct Box
{
    Quaternion orientation;
    Float3 center;
    Float3 extents;
} Box;

typedef struct Cone
{
    Float3 base_center;
    Float3 axis;
    float radius;
} Cone;

typedef struct Cylinder
{
    Float3 start;
    Float3 end;
    float radius;
} Cylinder;

typedef struct Disk
{
    Float3 center;
    Float3 axis;
    float radius;
} Disk;

typedef struct LineSegment
{
    Float3 start;
    Float3 end;
} LineSegment;

typedef struct Ray
{
    Float3 origin;
    Float3 direction;
} Ray;

typedef struct Sphere
{
    Float3 center;
    float radius;
} Sphere;

typedef struct Torus
{
    Float3 center;
    Float3 axis;
    float major_radius;
    float minor_radius;
} Torus;

JanVertex* jan_first_vertex_hit_by_ray(JanMesh* mesh, Ray ray, float hit_radius, float viewport_width, float* vertex_distance);
JanEdge* jan_first_edge_under_point(JanMesh* mesh, Float2 hit_center, float hit_radius, Matrix4 model_view_projection, Matrix4 inverse, Int2 viewport, Float3 view_position, Float3 view_direction, float* edge_distance);
JanFace* jan_first_face_hit_by_ray(JanMesh* mesh, Ray ray, float* face_distance, Stack* stack);

bool point_in_polygon(Float2 point, Float2* vertices, int vertices_count);
float distance_point_plane(Float3 point, Float3 origin, Float3 normal);
Ray transform_ray(Ray ray, Matrix4 transform);
bool intersect_ray_plane(Ray ray, Float3 origin, Float3 normal, Float3* intersection);
bool intersect_ray_sphere(Ray ray, Sphere sphere, Float3* intersection);
bool intersect_ray_cylinder(Ray ray, Cylinder cylinder, Float3* intersection);
bool intersect_ray_cone(Ray ray, Cone cone, Float3* intersection);
bool intersect_ray_torus(Ray ray, Torus torus, Float3* intersection);
bool intersect_ray_box(Ray ray, Box box, Float3* intersection);

#endif // INTERSECTION_H_
