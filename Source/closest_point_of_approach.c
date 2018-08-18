#include "closest_point_of_approach.h"

#include "assert.h"
#include "float_utilities.h"
#include "math_basics.h"

float distance_point_plane(Float3 point, Float3 origin, Float3 normal)
{
    ASSERT(float3_is_normalised(normal));
    return fabsf(float3_dot(float3_subtract(origin, point), normal));
}

Float3 project_onto_plane(Float3 point, Float3 origin, Float3 normal)
{
    ASSERT(float3_is_normalised(normal));
    Float3 off_origin = float3_subtract(point, origin);
    float d = float3_dot(off_origin, normal);
    Float3 projected = float3_subtract(off_origin, float3_multiply(d, normal));
    return float3_add(projected, origin);
}

Float3 closest_disk_point(Disk disk, Float3 point)
{
    Float3 off_center = float3_subtract(point, disk.center);
    float distance = float3_length(off_center);
    if(distance == 0.0f)
    {
        return disk.center;
    }
    Float3 on_sphere = float3_multiply(disk.radius, float3_divide(off_center, distance));
    float d = float3_dot(on_sphere, disk.axis);
    Float3 rejection = float3_subtract(on_sphere, float3_multiply(d, disk.axis));
    return float3_add(rejection, disk.center);
}

static bool is_parallel(Float3 a, Float3 b)
{
    return almost_one(fabsf(float3_dot(a, b)));
}

Float3 closest_disk_plane(Disk disk, Float3 origin, Float3 normal)
{
    if(is_parallel(disk.axis, normal))
    {
        return disk.center;
    }

    // Find the line of intersection of plane the disk lies on and the given
    // plane.

    float d[2];
    d[0] = float3_dot(disk.axis, disk.center);
    d[1] = float3_dot(normal, origin);

    Float3 line_point;
    if(float3_dot(disk.axis, float3_unit_z) == 0.0f
            || float3_dot(normal, float3_unit_z) == 0.0f)
    {
        Float2 a = float3_extract_float2(disk.axis);
        Float2 b = float3_extract_float2(normal);
        float denominator = float2_determinant(a, b);
        line_point.x = ((d[0] * b.y) - (a.y * d[1])) / denominator;
        line_point.y = ((a.x * d[1]) - (d[0] * b.x)) / denominator;
        line_point.z = 0.0f;
    }
    else
    {
        Float2 a = {{disk.axis.y, disk.axis.z}};
        Float2 b = {{normal.y, normal.z}};
        float denominator = float2_determinant(a, b);
        line_point.x = 0.0f;
        line_point.y = ((d[0] * b.y) - (a.y * d[1])) / denominator;
        line_point.z = ((a.x * d[1]) - (d[0] * b.x)) / denominator;
    }

    Float3 line_direction = float3_cross(disk.axis, normal);

    // Since the disk and line are in the same plane, this can be treated as
    // finding the closest point on a line to a circle.

    Float3 to_center = float3_subtract(disk.center, line_point);
    Float3 off_origin = float3_negate(float3_reject(to_center, line_direction));
    float from_origin = float3_length(off_origin);
    if(from_origin >= disk.radius)
    {
        Float3 on_rim = float3_multiply(disk.radius, float3_divide(off_origin, from_origin));
        return float3_add(on_rim, disk.center);
    }
    else
    {
        return float3_add(off_origin, disk.center);
    }
}

Float3 closest_point_on_line(Ray ray, Float3 start, Float3 end)
{
    Float3 line = float3_subtract(end, start);
    float a = float3_squared_length(ray.direction);
    float b = float3_dot(ray.direction, line);
    float e = float3_squared_length(line);
    float d = (a * e) - (b * b);

    if(d == 0.0f)
    {
        return start;
    }
    else
    {
        Float3 r = float3_subtract(ray.origin, start);
        float c = float3_dot(ray.direction, r);
        float f = float3_dot(line, r);
        float t = ((a * f) - (c * b)) / d;
        return float3_add(float3_multiply(t, line), start);
    }
}

Float3 closest_ray_point(Ray ray, Float3 point)
{
    double d = float3_dot(float3_subtract(point, ray.origin), ray.direction);
    if(d <= 0.0f)
    {
        return ray.origin;
    }
    else
    {
        return float3_add(float3_multiply(d, ray.direction), ray.origin);
    }
}
