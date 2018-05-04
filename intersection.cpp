#include "intersection.h"

#include "assert.h"
#include "complex_math.h"
#include "float_utilities.h"
#include "jan.h"
#include "logging.h"
#include "math_basics.h"
#include "memory.h"
#include "restrict.h"

static float orient(Vector2 v0, Vector2 v1, Vector2 v2)
{
    return (v1.x - v0.x) * (v2.y - v0.y) - (v2.x - v0.x) * (v1.y - v0.y);
}

static bool to_left_of_line(Vector2 e0, Vector2 e1, Vector2 p)
{
    return orient(e0, e1, p) > 0.0f;
}

static bool to_right_of_line(Vector2 e0, Vector2 e1, Vector2 p)
{
    return orient(e0, e1, p) < 0.0f;
}

// This winding number algorithm works for complex polygons. Also, since all
// that matters is whether the final winding number is zero, the input polygon
// can be clockwise or counterclockwise.
bool point_in_polygon(Vector2 point, Vector2* vertices, int vertices_count)
{
    int winding = 0;
    for(int i = 0; i < vertices_count; i += 1)
    {
        Vector2 v0 = vertices[i];
        Vector2 v1 = vertices[(i + 1) % vertices_count];
        if(v0.y <= point.y)
        {
            if(v1.y > point.y)
            {
                 if(to_left_of_line(v0, v1, point))
                 {
                     winding += 1;
                 }
            }
        }
        else
        {
            if(v1.y <= point.y)
            {
                 if(to_right_of_line(v0, v1, point))
                 {
                     winding -= 1;
                 }
            }
        }
    }
    return winding != 0;
}

float distance_point_plane(Vector3 point, Vector3 origin, Vector3 normal)
{
    ASSERT(is_normalised(normal));
    return abs(dot(origin - point, normal));
}

Ray transform_ray(Ray ray, Matrix4 transform)
{
    Ray result;
    result.origin = transform_point(transform, ray.origin);
    result.direction = normalise(transform_vector(transform, ray.direction));
    return result;
}

static bool solve_quadratic_equation(float a, float b, float c, float* RESTRICT t0, float* RESTRICT t1)
{
    float discriminant = (b * b) - (4.0f * a * c);
    if(discriminant < 0.0f)
    {
        return false;
    }
    else
    {
        float s = sqrt(discriminant);
        float u0 = (-b + s) / (2.0f * a);
        float u1 = (-b - s) / (2.0f * a);
        if(u0 > u1)
        {
            *t0 = u1;
            *t1 = u0;
        }
        else
        {
            *t0 = u0;
            *t1 = u1;
        }
        return true;
    }
}

static void solve_quartic_equation(Complex coefficients[5], Complex roots[4])
{
    Complex a = coefficients[0];
    Complex b = coefficients[1] / a;
    Complex c = coefficients[2] / a;
    Complex d = coefficients[3] / a;
    Complex e = coefficients[4] / a;

    Complex q0 = (c * c) - (3.0f * b * d) + (12.0f * e);
    Complex q1 = (2.0 * c * c * c) - (9.0f * b * c * d) + (27.0f * d * d) + (27.0f * b * b * e) - (72.0f * c * e);
    Complex q2 = (8.0f * b * c) - (16.0f * d) - (2.0f * b * b * b);
    Complex q3 = (3.0f * b * b) - (8.0f * c);
    Complex q4 = cbrt((q1 / 2.0f) + sqrt((q1 * q1 / 4.0f) - (q0 * q0 * q0)));
    Complex q5 = ((q0 / q4) + q4) / 3.0f;
    Complex q6 = 2.0f * sqrt((q3 / 12.0f) + q5);

    Complex s = (4.0f * q3 / 6.0f) - (4.0f * q5);

    roots[0] = (-b - q6 - sqrt(s - (q2 / q6))) / 4.0f;
    roots[1] = (-b - q6 + sqrt(s - (q2 / q6))) / 4.0f;
    roots[2] = (-b + q6 - sqrt(s + (q2 / q6))) / 4.0f;
    roots[3] = (-b + q6 + sqrt(s + (q2 / q6))) / 4.0f;
}

static int solve_real_quartic_equation(float coefficients[5], float roots[4])
{
    Complex c[5];
    for(int i = 0; i < 5; i += 1)
    {
        c[i] = {coefficients[i], 0.0f};
    }

    Complex r[4];
    solve_quartic_equation(c, r);

    int real_roots = 0;
    for(int i = 0; i < 4; i += 1)
    {
        if(almost_zero(r[i].i))
        {
            roots[real_roots] = r[i].r;
            real_roots += 1;
        }
    }

    return real_roots;
}

bool intersect_ray_plane(Ray ray, Vector3 origin, Vector3 normal, Vector3* intersection)
{
    ASSERT(is_normalised(normal));
    ASSERT(is_normalised(ray.direction));

    float d = dot(-normal, ray.direction);
    if(abs(d) < 1e-6)
    {
        return false;
    }

    float t = dot(origin - ray.origin, -normal) / d;
    if(t < 0.0f)
    {
        return false;
    }
    else
    {
        *intersection = (t * ray.direction) + ray.origin;
        return true;
    }
}

bool intersect_ray_sphere(Ray ray, Sphere sphere, Vector3* intersection)
{
    Vector3 origin = (ray.origin - sphere.center) / sphere.radius;
    Vector3 direction = ray.direction;

    float a = squared_length(direction);
    float b = dot(2.0f * direction, origin);
    float c = squared_length(origin) - 1.0f;

    float t0, t1;
    if(!solve_quadratic_equation(a, b, c, &t0, &t1))
    {
        return false;
    }
    else
    {
        *intersection = (ray.direction * t0) + ray.origin;
        return true;
    }
}

bool intersect_ray_cylinder(Ray ray, Cylinder cylinder, Vector3* intersection)
{
    float radius = cylinder.radius;
    Vector3 center = (cylinder.start + cylinder.end) / 2.0f;
    float half_length = distance(center, cylinder.end);

    Vector3 forward = normalise(cylinder.end - cylinder.start);
    Vector3 right = normalise(perp(forward));
    Vector3 up = normalise(cross(forward, right));
    Matrix4 view = view_matrix(right, up, forward, center);
    Vector3 dilation = {radius, radius, half_length};
    Matrix4 transform = dilation_matrix(reciprocal(dilation)) * view;

    Ray cylinder_ray = transform_ray(ray, transform);
    Vector3 origin = cylinder_ray.origin;
    Vector3 direction = cylinder_ray.direction;

    float dx = direction.x;
    float dy = direction.y;
    float ox = origin.x;
    float oy = origin.y;

    float a = (dx * dx) + (dy * dy);
    float b = (2.0f * ox * dx) + (2.0f * oy * dy);
    float c = (ox * ox) + (oy * oy) - 1.0f;

    float t0, t1;
    if(!solve_quadratic_equation(a, b, c, &t0, &t1))
    {
        return false;
    }
    float z0 = (t0 * direction.z) + origin.z;
    float z1 = (t1 * direction.z) + origin.z;

    if(z0 < -1.0f)
    {
        if(z1 < -1.0f)
        {
            return false;
        }

        float t2 = t0 + (t1 - t0) * (z0 + 1.0f) / (z0 - z1);
        if(t2 <= 0.0f)
        {
            return false;
        }
        else
        {
            *intersection = (ray.direction * t2) + ray.origin;
            return true;
        }
    }
    else if(z0 >= -1.0f && z0 <= 1.0f)
    {
        if(t0 <= 0.0f)
        {
            return false;
        }
        else
        {
            *intersection = (ray.direction * t0) + ray.origin;
            return true;
        }
    }
    else if(z0 > 1.0f)
    {
        if(z1 > 1.0f)
        {
            return false;
        }

        float t2 = t0 + (t1 - t0) * (z0 - 1.0f) / (z0 - z1);
        if(t2 <= 0.0f)
        {
            return false;
        }
        else
        {
            *intersection = (ray.direction * t2) + ray.origin;
            return true;
        }
    }

    return false;
}

bool intersect_ray_cone(Ray ray, Cone cone, Vector3* intersection)
{
    float radius = cone.radius;
    float height = length(cone.axis);
    Vector3 tip = cone.axis + cone.base_center;

    Vector3 forward = normalise(cone.axis);
    Vector3 right = normalise(perp(forward));
    Vector3 up = normalise(cross(forward, right));
    Matrix4 view = view_matrix(right, up, forward, tip);
    Vector3 dilation = {radius, radius, height};
    Matrix4 transform = dilation_matrix(reciprocal(dilation)) * view;

    Ray cone_ray = transform_ray(ray, transform);
    Vector3 origin = cone_ray.origin;
    Vector3 direction = cone_ray.direction;

    float dx = direction.x;
    float dy = direction.y;
    float dz = direction.z;
    float ox = origin.x;
    float oy = origin.y;
    float oz = origin.z;

    float a = (dx * dx) + (dy * dy) - (dz * dz);
    float b = (2.0f * ox * dx) + (2.0f * oy * dy) - (2.0f * oz * dz);
    float c = (ox * ox) + (oy * oy) - (oz * oz);

    float t0, t1;
    if(!solve_quadratic_equation(a, b, c, &t0, &t1))
    {
        return false;
    }

    float z0 = (t0 * direction.z) + origin.z;
    float z1 = (t1 * direction.z) + origin.z;

    if(z0 >= -1.0f && z0 <= 0.0f)
    {
        if(t0 <= 0.0f)
        {
            return false;
        }
        else
        {
            *intersection = (t0 * ray.direction) + ray.origin;
            return true;
        }
    }
    else if(z0 < -1.0f)
    {
        if(z1 < -1.0f || z1 > 0.0f)
        {
            return false;
        }

        float t2 = t0 + (t1 - t0) * (z0 + 1.0f) / (z0 - z1);
        if(t2 <= 0.0f)
        {
            return false;
        }
        else
        {
            *intersection = (t2 * ray.direction) + ray.origin;
            return true;
        }
    }

    return false;
}

bool intersect_ray_torus(Ray ray, Torus torus, Vector3* intersection)
{
    float r = torus.major_radius;
    float s = torus.minor_radius;

    Vector3 forward = normalise(torus.axis);
    Vector3 right = normalise(perp(forward));
    Vector3 up = normalise(cross(forward, right));
    Matrix4 tranform = view_matrix(right, up, forward, torus.center);
    Ray torus_ray = transform_ray(ray, tranform);

    Vector3 direction = torus_ray.direction;
    Vector3 origin = torus_ray.origin;

    float dx = direction.x;
    float dy = direction.y;
    float ox = origin.x;
    float oy = origin.y;

    float f = 4.0f * r * r;
    float g = f * ((dx * dx) + (dy * dy));
    float h = 2.0f * f * ((ox * dx) + (oy * dy));
    float i = f * ((ox * ox) + (oy * oy));
    float j = squared_length(direction);
    float k = 2.0f * dot(origin, direction);
    float l = squared_length(origin) + (r * r) - (s * s);

    float coefficients[5] =
    {
        j * j,
        2.0f * j * k,
        (2.0f * j * l) + (k * k) - g,
        (2.0f * k * l) - h,
        (l * l) - i,
    };

    float roots[4];
    int roots_found = solve_real_quartic_equation(coefficients, roots);
    if(roots_found == 0)
    {
        return false;
    }

    float t = 1e-4f;
    for(int i = 0; i < roots_found; i += 1)
    {
        if(roots[i] > t)
        {
            t = roots[i];
        }
    }

    *intersection =  (t * ray.direction) + ray.origin;
    return true;
}

bool intersect_ray_box(Ray ray, Box box, Vector3* intersection)
{
    Vector3 direction = box.orientation * ray.direction;
    Vector3 origin = box.orientation * (ray.origin - box.center);

    float t0 = (-box.extents.x - origin.x) / direction.x;
    float t1 = (+box.extents.x - origin.x) / direction.x;

    float tl = fmin(t0, t1);
    float th = fmax(t0, t1);

    for(int i = 1; i < 3; i += 1)
    {
        t0 = (-box.extents[i] - origin[i]) / direction[i];
        t1 = (+box.extents[i] - origin[i]) / direction[i];

        tl = fmax(tl, fmin(fmin(t0, t1), th));
        th = fmin(th, fmax(fmax(t0, t1), tl));
    }

    if(th <= fmax(tl, 0.0f))
    {
        return false;
    }
    else
    {
        *intersection = (tl * ray.direction) + ray.origin;
        return true;
    }
}

static bool intersect_ray_plane_one_sided(Vector3 start, Vector3 direction, Vector3 origin, Vector3 normal, Vector3* intersection)
{
    ASSERT(is_normalised(normal));
    ASSERT(is_normalised(direction));
    float d = dot(-normal, direction);
    if(d > 1e-6)
    {
        float t = dot(origin - start, -normal) / d;
        if(t < 0.0f)
        {
            return false;
        }
        else
        {
            *intersection = (t * direction) + start;
            return true;
        }
    }
    return false;
}

Vector3 project_onto_plane(Vector3 v, Vector3 normal)
{
    ASSERT(is_normalised(normal));
    float d = -dot(normal, v);
    return (d * normal) + v;
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

namespace jan {

static void project_face_onto_plane(Face* face, Vector2* vertices)
{
    // @Incomplete: Holes in faces aren't yet supported!
    ASSERT(!face->first_border->next);

    Matrix3 mi = transpose(orthogonal_basis(face->normal));
    Link* first = face->first_border->first;
    Link* link = first;
    int i = 0;
    do
    {
        vertices[i] = mi * link->vertex->position;
        i += 1;
        link = link->next;
    } while(link != first);
}

Face* first_face_hit_by_ray(Mesh* mesh, Ray ray, float* face_distance, Stack* stack)
{
    float closest = infinity;
    Face* result = nullptr;
    FOR_EACH_IN_POOL(Face, face, mesh->face_pool)
    {
        // @Incomplete: Holes in faces aren't yet supported!
        ASSERT(!face->first_border->next);

        Vector3 intersection;
        bool intersected = intersect_ray_plane_one_sided(ray.origin, ray.direction, face->first_border->first->vertex->position, face->normal, &intersection);
        if(intersected)
        {
            float distance = squared_distance(ray.origin, intersection);
            if(distance < closest)
            {
                Matrix3 mi = transpose(orthogonal_basis(face->normal));
                Vector2 point = mi * intersection;

                Vector2* projected = STACK_ALLOCATE(stack, Vector2, face->edges);
                project_face_onto_plane(face, projected);

                if(point_in_polygon(point, projected, face->edges))
                {
                    closest = distance;
                    result = face;
                }
                STACK_DEALLOCATE(stack, projected);
            }
        }
    }
    if(face_distance)
    {
        *face_distance = closest;
    }
    return result;
}

} // namespace jan
