#include "intersection.h"

#include "assert.h"
#include "float_utilities.h"
#include "jan.h"

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

static bool intersect_ray_plane(Vector3 start, Vector3 direction, Vector3 origin, Vector3 normal, float* distance)
{
	ASSERT(is_normalised(normal));
	ASSERT(is_normalised(direction));
	float d = dot(normal, direction);
	if(d > 1e-6)
	{
		float q = dot(origin - start, normal) / d;
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

Face* first_face_hit_by_ray(Mesh* mesh, Ray ray)
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
	return result;
}

} // namespace jan
