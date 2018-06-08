#include "closest_point_of_approach.h"

#include "assert.h"
#include "float_utilities.h"
#include "math_basics.h"

float distance_point_plane(Vector3 point, Vector3 origin, Vector3 normal)
{
    ASSERT(is_normalised(normal));
    return abs(dot(origin - point, normal));
}

Vector3 project_onto_plane(Vector3 point, Vector3 origin, Vector3 normal)
{
    ASSERT(is_normalised(normal));
    Vector3 off_origin = point - origin;
    float d = dot(off_origin, normal);
    return off_origin - (d * normal) + origin;
}

Vector3 closest_disk_point(Disk disk, Vector3 point)
{
    Vector3 off_center = point - disk.center;
    float distance = length(off_center);
    if(distance == 0.0f)
    {
        return disk.center;
    }
    Vector3 on_sphere = disk.radius * (off_center / distance);
    float d = dot(on_sphere, disk.axis);
    Vector3 rejection = on_sphere - (d * disk.axis);
    return rejection + disk.center;
}

static bool is_parallel(Vector3 a, Vector3 b)
{
    return almost_one(abs(dot(a, b)));
}

Vector3 closest_disk_plane(Disk disk, Vector3 origin, Vector3 normal)
{
    if(is_parallel(disk.axis, normal))
    {
        return disk.center;
    }

    // Find the line of intersection of plane the disk lies on and the given
    // plane.

    float d[2];
    d[0] = dot(disk.axis, disk.center);
    d[1] = dot(normal, origin);

    Vector3 line_point;
    if(dot(disk.axis, vector3_unit_z) == 0.0f
        || dot(normal, vector3_unit_z) == 0.0f)
    {
        Vector2 a = extract_vector2(disk.axis);
        Vector2 b = extract_vector2(normal);
        float denominator = determinant(a, b);
        line_point.x = ((d[0] * b.y) - (a.y * d[1])) / denominator;
        line_point.y = ((a.x * d[1]) - (d[0] * b.x)) / denominator;
        line_point.z = 0.0f;
    }
    else
    {
        Vector2 a = {disk.axis.y, disk.axis.z};
        Vector2 b = {normal.y, normal.z};
        float denominator = determinant(a, b);
        line_point.x = 0.0f;
        line_point.y = ((d[0] * b.y) - (a.y * d[1])) / denominator;
        line_point.z = ((a.x * d[1]) - (d[0] * b.x)) / denominator;
    }

    Vector3 line_direction = cross(disk.axis, normal);

    // Since the disk and line are in the same plane, this can be treated as
    // finding the closest point on a line to a circle.

    Vector3 off_origin = -reject(disk.center - line_point, line_direction);
    float from_origin = length(off_origin);
    if(from_origin >= disk.radius)
    {
        Vector3 on_rim = disk.radius * (off_origin / from_origin);
        return on_rim + disk.center;
    }
    else
    {
        return off_origin + disk.center;
    }
}

Vector3 closest_point_on_line(Ray ray, Vector3 start, Vector3 end)
{
    Vector3 line = end - start;
    float a = squared_length(ray.direction);
    float b = dot(ray.direction, line);
    float e = squared_length(line);
    float d = (a * e) - (b * b);

    if(d == 0.0f)
    {
        return start;
    }
    else
    {
        Vector3 r = ray.origin - start;
        float c = dot(ray.direction, r);
        float f = dot(line, r);
        float t = ((a * f) - (c * b)) / d;
        return (t * line) + start;
    }
}

Vector3 closest_ray_point(Ray ray, Vector3 point)
{
    double d = dot(point - ray.origin, ray.direction);
    if(d <= 0.0f)
    {
        return ray.origin;
    }
    else
    {
        return (d * ray.direction) + ray.origin;
    }
}
