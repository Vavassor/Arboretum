#include "intersection.h"

#include "assert.h"
#include "float_utilities.h"
#include "math_basics.h"
#include "restrict.h"
#include "jan.h"
#include "logging.h"

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

static bool intersect_ray_plane(Vector3 start, Vector3 direction, Vector3 origin, Vector3 normal, float* distance)
{
	ASSERT(is_normalised(normal));
	ASSERT(is_normalised(direction));
	float d = dot(-normal, direction);
	if(d > 1e-6)
	{
		float q = dot(origin - start, -normal) / d;
		if(q < 0.0f)
		{
			return false;
		}
		else
		{
			*distance = q;
			return true;
		}
	}
	return false;
}

static bool quadratic(float a, float b, float c, float* RESTRICT t0, float* RESTRICT t1)
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
	if(!quadratic(a, b, c, &t0, &t1))
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
	if(!quadratic(a, b, c, &t0, &t1))
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

namespace jan {

static void project_face_onto_plane(Face* face, Vector2* vertices)
{
	Matrix3 mi = transpose(orthogonal_basis(face->normal));
	Link* first = face->link;
	Link* link = first;
	int i = 0;
	do
	{
		vertices[i] = mi * link->vertex->position;
		i += 1;
		link = link->next;
	} while(link != first);
}

Face* first_face_hit_by_ray(Mesh* mesh, Ray ray, float* face_distance)
{
	float closest = infinity;
	Face* result = nullptr;
	FOR_EACH_IN_POOL(Face, face, mesh->face_pool)
	{
		float distance;
		bool intersected = intersect_ray_plane(ray.origin, ray.direction, face->link->vertex->position, face->normal, &distance);
		if(intersected && distance < closest)
		{
			Vector3 intersection = (distance * ray.direction) + ray.origin;
			Matrix3 mi = transpose(orthogonal_basis(face->normal));
			Vector2 point = mi * intersection;

			Vector2 projected[face->edges];
			project_face_onto_plane(face, projected);

			if(point_in_polygon(point, projected, face->edges))
			{
				closest = distance;
				result = face;
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
