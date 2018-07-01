#include "intersection.h"

#include "assert.h"
#include "camera.h"
#include "complex_math.h"
#include "float_utilities.h"
#include "jan.h"
#include "math_basics.h"
#include "memory.h"

static float orient(Float2 v0, Float2 v1, Float2 v2)
{
    return (v1.x - v0.x) * (v2.y - v0.y) - (v2.x - v0.x) * (v1.y - v0.y);
}

static bool to_left_of_line(Float2 e0, Float2 e1, Float2 p)
{
    return orient(e0, e1, p) > 0.0f;
}

static bool to_right_of_line(Float2 e0, Float2 e1, Float2 p)
{
    return orient(e0, e1, p) < 0.0f;
}

// This winding number algorithm works for complex polygons. Also, since all
// that matters is whether the final winding number is zero, the input polygon
// can be clockwise or counterclockwise.
bool point_in_polygon(Float2 point, Float2* vertices, int vertices_count)
{
    int winding = 0;
    for(int i = 0; i < vertices_count; i += 1)
    {
        Float2 v0 = vertices[i];
        Float2 v1 = vertices[(i + 1) % vertices_count];
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

Ray transform_ray(Ray ray, Matrix4 transform)
{
    Ray result;
    result.origin = matrix4_transform_point(transform, ray.origin);
    result.direction = float3_normalise(matrix4_transform_vector(transform, ray.direction));
    return result;
}

LineSegment transform_line_segment(LineSegment segment, Matrix4 transform)
{
    LineSegment result;
    result.start = matrix4_transform_point(transform, segment.start);
    result.end = matrix4_transform_point(transform, segment.end);
    return result;
}

static bool solve_quadratic_equation(float a, float b, float c, float* restrict t0, float* restrict t1)
{
    float discriminant = (b * b) - (4.0f * a * c);
    if(discriminant < 0.0f)
    {
        return false;
    }
    else
    {
        float s = sqrtf(discriminant);
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

static Complex degree1(float s, Complex a)
{
    return complex_scalar_multiply(s, a);
}

static Complex degree2(float s, Complex a, Complex b)
{
    return complex_multiply(degree1(s, a), b);
}

static Complex degree3(float s, Complex a, Complex b, Complex c)
{
    return complex_multiply(degree2(s, a, b), c);
}

static void solve_quartic_equation(Complex coefficients[5], Complex roots[4])
{
    Complex a = coefficients[0];
    Complex b = complex_divide(coefficients[1], a);
    Complex c = complex_divide(coefficients[2], a);
    Complex d = complex_divide(coefficients[3], a);
    Complex e = complex_divide(coefficients[4], a);

    Complex t0 = complex_multiply(c, c);
    Complex t1 = degree2(-3.0f, b, d);
    Complex t2 = degree1(12.0f, e);
    Complex q0 = complex_add(complex_add(t0, t1), t2);

    t0 = degree3(2.0f, c, c, c);
    t1 = degree3(-9.0f, b, c, d);
    t2 = degree3(27.0f, b, b, e);
    Complex t3 = degree2(27.0f, d, d);
    Complex t4 = degree2(-72.0f, c, e);
    Complex q1 = complex_add(complex_add(complex_add(complex_add(t0, t1), t2), t3), t4);

    t0 = degree2(8.0f, b, c);
    t1 = degree1(-16.0f, d);
    t2 = degree3(-2.0f, b, b, b);
    Complex q2 = complex_add(complex_add(t0, t1), t2);

    t0 = degree2(3.0f, b, b);
    t1 = degree1(-8.0f, c);
    Complex q3 = complex_add(t0, t1);

    t0 = complex_scalar_divide(q1, 2.0f);
    t1 = complex_scalar_divide(complex_multiply(q1, q1), 4.0f);
    t2 = complex_multiply(complex_multiply(q0, q0), q0);
    t3 = complex_sqrt(complex_subtract(t1, t2));
    Complex q4 = complex_cbrt(complex_add(t0, t3));

    t0 = complex_add(complex_divide(q0, q4), q4);
    Complex q5 = complex_scalar_divide(t0, 3.0f);

    t0 = complex_scalar_divide(q3, 12.0f);
    t1 = complex_sqrt(complex_add(t0, q5));
    Complex q6 = complex_scalar_multiply(2.0f, t1);

    t0 = complex_scalar_multiply(4.0f, q3);
    t1 = complex_scalar_divide(t0, 6.0f);
    t2 = complex_scalar_multiply(4.0f, q5);
    Complex s = complex_subtract(t1, t2);

    b = complex_negate(b);
    t0 = complex_divide(q2, q6);
    t1 = complex_sqrt(complex_subtract(s, t0));
    t2 = complex_sqrt(complex_add(s, t0));
    t3 = complex_subtract(b, q6);
    t4 = complex_add(b, q6);

    Complex p[4];
    p[0] = complex_subtract(t3, t1);
    p[1] = complex_add(t3, t1);
    p[2] = complex_subtract(t4, t2);
    p[3] = complex_add(t4, t2);

    roots[0] = complex_scalar_divide(p[0], 4.0f);
    roots[1] = complex_scalar_divide(p[1], 4.0f);
    roots[2] = complex_scalar_divide(p[2], 4.0f);
    roots[3] = complex_scalar_divide(p[3], 4.0f);
}

static int solve_real_quartic_equation(float coefficients[5], float roots[4])
{
    Complex c[5];
    for(int i = 0; i < 5; i += 1)
    {
        c[i] = (Complex){coefficients[i], 0.0f};
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

bool intersect_ray_plane(Ray ray, Float3 origin, Float3 normal, Float3* intersection)
{
    ASSERT(float3_is_normalised(normal));
    ASSERT(float3_is_normalised(ray.direction));

    float d = float3_dot(float3_negate(normal), ray.direction);
    if(fabsf(d) < 1e-6f)
    {
        return false;
    }

    float t = float3_dot(float3_subtract(origin, ray.origin), float3_negate(normal)) / d;
    if(t < 0.0f)
    {
        return false;
    }
    else
    {
        *intersection = float3_madd(t, ray.direction, ray.origin);
        return true;
    }
}

bool intersect_ray_sphere(Ray ray, Sphere sphere, Float3* intersection)
{
    Float3 to_center = float3_subtract(sphere.center, ray.origin);
    float radius2 = sphere.radius * sphere.radius;
    float t_axis = float3_dot(to_center, ray.direction);
    float distance2 = float3_squared_length(to_center) - (t_axis * t_axis);
    if(distance2 > radius2)
    {
        return false;
    }

    float t_intersect = sqrtf(radius2 - distance2);
    float t[2];
    t[0] = t_axis - t_intersect;
    t[1] = t_axis + t_intersect;

    if(t[0] > t[1])
    {
        float temp = t[0];
        t[0] = t[1];
        t[1] = temp;
    }

    if(t[0] < 0.0f)
    {
        t[0] = t[1];
        if(t[0] < 0.0f)
        {
            return false;
        }
    }

    *intersection = float3_madd(t[0], ray.direction, ray.origin);

    return true;
}

bool intersect_ray_cylinder(Ray ray, Cylinder cylinder, Float3* intersection)
{
    float radius = cylinder.radius;
    Float3 center = float3_divide(float3_add(cylinder.start, cylinder.end), 2.0f);
    float half_length = float3_distance(center, cylinder.end);

    Float3 forward = float3_normalise(float3_subtract(cylinder.end, cylinder.start));
    Float3 right = float3_normalise(float3_perp(forward));
    Float3 up = float3_normalise(float3_cross(forward, right));
    Matrix4 view = matrix4_view(right, up, forward, center);
    Float3 dilation = {{radius, radius, half_length}};
    Matrix4 transform = matrix4_multiply(matrix4_dilation(float3_reciprocal(dilation)), view);

    Ray cylinder_ray = transform_ray(ray, transform);
    Float3 origin = cylinder_ray.origin;
    Float3 direction = cylinder_ray.direction;

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
            *intersection = float3_madd(t2, ray.direction, ray.origin);
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
            *intersection = float3_madd(t0, ray.direction, ray.origin);
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
            *intersection = float3_madd(t2, ray.direction, ray.origin);
            return true;
        }
    }

    return false;
}

bool intersect_ray_cone(Ray ray, Cone cone, Float3* intersection)
{
    float radius = cone.radius;
    float height = float3_length(cone.axis);
    Float3 tip = float3_add(cone.axis, cone.base_center);

    Float3 forward = float3_normalise(cone.axis);
    Float3 right = float3_normalise(float3_perp(forward));
    Float3 up = float3_normalise(float3_cross(forward, right));
    Matrix4 view = matrix4_view(right, up, forward, tip);
    Float3 dilation = {{radius, radius, height}};
    Matrix4 transform = matrix4_multiply(matrix4_dilation(float3_reciprocal(dilation)), view);

    Ray cone_ray = transform_ray(ray, transform);
    Float3 origin = cone_ray.origin;
    Float3 direction = cone_ray.direction;

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
            *intersection = float3_madd(t0, ray.direction, ray.origin);
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
            *intersection = float3_madd(t2, ray.direction, ray.origin);
            return true;
        }
    }

    return false;
}

bool intersect_ray_torus(Ray ray, Torus torus, Float3* intersection)
{
    float r = torus.major_radius;
    float s = torus.minor_radius;

    Float3 forward = float3_normalise(torus.axis);
    Float3 right = float3_normalise(float3_perp(forward));
    Float3 up = float3_normalise(float3_cross(forward, right));
    Matrix4 tranform = matrix4_view(right, up, forward, torus.center);
    Ray torus_ray = transform_ray(ray, tranform);

    Float3 direction = torus_ray.direction;
    Float3 origin = torus_ray.origin;

    float dx = direction.x;
    float dy = direction.y;
    float ox = origin.x;
    float oy = origin.y;

    float f = 4.0f * r * r;
    float g = f * ((dx * dx) + (dy * dy));
    float h = 2.0f * f * ((ox * dx) + (oy * dy));
    float i = f * ((ox * ox) + (oy * oy));
    float j = float3_squared_length(direction);
    float k = 2.0f * float3_dot(origin, direction);
    float l = float3_squared_length(origin) + (r * r) - (s * s);

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

    *intersection = float3_madd(t, ray.direction, ray.origin);
    return true;
}

bool intersect_ray_box(Ray ray, Box box, Float3* intersection)
{
    Float3 direction = quaternion_rotate(box.orientation, ray.direction);
    Float3 origin = quaternion_rotate(box.orientation, float3_subtract(ray.origin, box.center));

    float t0 = (-box.extents.x - origin.x) / direction.x;
    float t1 = (+box.extents.x - origin.x) / direction.x;

    float tl = fminf(t0, t1);
    float th = fmaxf(t0, t1);

    for(int i = 1; i < 3; i += 1)
    {
        t0 = (-box.extents.e[i] - origin.e[i]) / direction.e[i];
        t1 = (+box.extents.e[i] - origin.e[i]) / direction.e[i];

        tl = fmaxf(tl, fminf(fminf(t0, t1), th));
        th = fminf(th, fmaxf(fmaxf(t0, t1), tl));
    }

    if(th <= fmaxf(tl, 0.0f))
    {
        return false;
    }
    else
    {
        *intersection = float3_madd(tl, ray.direction, ray.origin);
        return true;
    }
}

static bool intersect_ray_plane_one_sided(Float3 start, Float3 direction, Float3 origin, Float3 normal, Float3* intersection)
{
    ASSERT(float3_is_normalised(normal));
    ASSERT(float3_is_normalised(direction));
    float d = float3_dot(float3_negate(normal), direction);
    if(d > 1e-6)
    {
        float t = float3_dot(float3_subtract(origin, start), float3_negate(normal)) / d;
        if(t < 0.0f)
        {
            return false;
        }
        else
        {
            *intersection = float3_madd(t, direction, start);
            return true;
        }
    }
    return false;
}

static bool intersect_line_segment_cylinder(LineSegment segment, Cylinder cylinder, Float3* intersection)
{
    float radius = cylinder.radius;
    Float3 center = float3_divide(float3_add(cylinder.start, cylinder.end), 2.0f);
    float half_length = float3_distance(center, cylinder.end);

    Float3 forward = float3_normalise(float3_subtract(cylinder.end, cylinder.start));
    Float3 right = float3_normalise(float3_perp(forward));
    Float3 up = float3_normalise(float3_cross(forward, right));
    Matrix4 view = matrix4_view(right, up, forward, center);
    Float3 dilation = {{radius, radius, half_length}};
    Matrix4 transform = matrix4_multiply(matrix4_dilation(float3_reciprocal(dilation)), view);

    LineSegment cylinder_segment = transform_line_segment(segment, transform);
    Float3 start = cylinder_segment.start;
    Float3 direction = float3_subtract(cylinder_segment.end, start);
    float d = float3_length(direction);
    direction = float3_divide(direction, d);

    float dx = direction.x;
    float dy = direction.y;
    float ox = start.x;
    float oy = start.y;

    float a = (dx * dx) + (dy * dy);
    float b = (2.0f * ox * dx) + (2.0f * oy * dy);
    float c = (ox * ox) + (oy * oy) - 1.0f;

    float t0, t1;
    if(!solve_quadratic_equation(a, b, c, &t0, &t1))
    {
        return false;
    }
    float z0 = (t0 * direction.z) + start.z;
    float z1 = (t1 * direction.z) + start.z;

    if(z0 < -1.0f)
    {
        if(z1 < -1.0f)
        {
            return false;
        }

        float t2 = t0 + (t1 - t0) * (z0 + 1.0f) / (z0 - z1);
        if(t2 <= 0.0f || t2 >= d)
        {
            return false;
        }
        else
        {
            Float3 point = float3_madd(t2, direction, start);
            Matrix4 inverse = matrix4_multiply(matrix4_inverse_view(view), matrix4_dilation(dilation));
            *intersection = matrix4_transform_point(inverse, point);
            return true;
        }
    }
    else if(z0 >= -1.0f && z0 <= 1.0f)
    {
        if(t0 <= 0.0f || t0 >= d)
        {
            return false;
        }
        else
        {
            Float3 point = float3_madd(t0, direction, start);
            Matrix4 inverse = matrix4_multiply(matrix4_inverse_view(view), matrix4_dilation(dilation));
            *intersection = matrix4_transform_point(inverse, point);
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
        if(t2 <= 0.0f || t2 >= d)
        {
            return false;
        }
        else
        {
            Float3 point = float3_madd(t2, direction, start);
            Matrix4 inverse = matrix4_multiply(matrix4_inverse_view(view), matrix4_dilation(dilation));
            *intersection = matrix4_transform_point(inverse, point);
            return true;
        }
    }

    return false;
}

JanVertex* jan_first_vertex_hit_by_ray(JanMesh* mesh, Ray ray, float hit_radius, float viewport_width, float* vertex_distance)
{
    float closest = infinity;
    JanVertex* result = NULL;

    float radius = hit_radius / viewport_width;

    FOR_EACH_IN_POOL(JanVertex, vertex, mesh->vertex_pool)
    {
        Sphere sphere;
        sphere.center = vertex->position;
        sphere.radius = radius * distance_point_plane(vertex->position, ray.origin, ray.direction);

        Float3 intersection;
        bool hit = intersect_ray_sphere(ray, sphere, &intersection);
        if(hit)
        {
            float distance = float3_squared_distance(ray.origin, intersection);
            if(distance < closest)
            {
                closest = distance;
                result = vertex;
            }
        }
    }

    if(vertex_distance)
    {
        *vertex_distance = closest;
    }

    return result;
}

JanEdge* jan_first_edge_under_point(JanMesh* mesh, Float2 hit_center, float hit_radius, Matrix4 model_view_projection, Matrix4 inverse, Int2 viewport, Float3 view_position, Float3 view_direction, float* edge_distance)
{
    float closest = infinity;
    JanEdge* result = NULL;

    Float2 ndc_point = viewport_point_to_ndc(hit_center, viewport);
    Float3 near = {{ndc_point.x, ndc_point.y, -1.0f}};
    Float3 far = {{ndc_point.x, ndc_point.y, +1.0f}};
    Cylinder cylinder = {near, far, hit_radius / viewport.x};

    FOR_EACH_IN_POOL(JanEdge, edge, mesh->edge_pool)
    {
        LineSegment segment;
        segment.start = matrix4_transform_point(model_view_projection, edge->vertices[0]->position);
        segment.end = matrix4_transform_point(model_view_projection, edge->vertices[1]->position);

        Float3 intersection;
        bool hit = intersect_line_segment_cylinder(segment, cylinder, &intersection);
        if(hit)
        {
            Float3 world_point = matrix4_transform_point(inverse, intersection);

            float distance = distance_point_plane(world_point, view_position, view_direction);
            if(distance < closest)
            {
                closest = distance;
                result = edge;
            }
        }
    }

    if(edge_distance)
    {
        *edge_distance = closest;
    }

    return result;
}

static void project_border_onto_plane(JanBorder* border, Matrix3 transform, Float2* vertices)
{
    JanLink* first = border->first;
    JanLink* link = first;
    int i = 0;
    do
    {
        vertices[i] = matrix3_transform(transform, link->vertex->position);
        i += 1;
        link = link->next;
    } while(link != first);
}

JanFace* jan_first_face_hit_by_ray(JanMesh* mesh, Ray ray, float* face_distance, Stack* stack)
{
    float closest = infinity;
    JanFace* result = NULL;

    FOR_EACH_IN_POOL(JanFace, face, mesh->face_pool)
    {
        Float3 intersection;
        Float3 any_point = face->first_border->first->vertex->position;
        bool intersected = intersect_ray_plane_one_sided(ray.origin, ray.direction, any_point, face->normal, &intersection);
        if(intersected)
        {
            float distance = float3_squared_distance(ray.origin, intersection);
            if(distance < closest)
            {
                Matrix3 mi = matrix3_transpose(matrix3_orthogonal_basis(face->normal));
                Float2 point = matrix3_transform(mi, intersection);

                bool on_face = false;

                for(JanBorder* border = face->first_border; border; border = border->next)
                {
                    int edges = jan_count_border_edges(border);
                    Float2* projected = STACK_ALLOCATE(stack, Float2, edges);
                    project_border_onto_plane(border, mi, projected);
                    if(point_in_polygon(point, projected, edges))
                    {
                        if(border == face->first_border)
                        {
                            on_face = true;
                        }
                        else
                        {
                            on_face = false;
                            break;
                        }
                    }
                    STACK_DEALLOCATE(stack, projected);
                }

                if(on_face)
                {
                    closest = distance;
                    result = face;
                }
            }
        }
    }

    if(face_distance)
    {
        *face_distance = closest;
    }

    return result;
}
