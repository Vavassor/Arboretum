#include "jan.h"

#include "array2.h"
#include "assert.h"
#include "float_utilities.h"
#include "int_utilities.h"
#include "invalid_index.h"
#include "jan_internal.h"
#include "math_basics.h"
#include "sorting.h"

static void add_to_pointcloud(JanVertex* vertex, Float4 colour, Heap* heap,
        Pointcloud* pointcloud)
{
    PointVertex* vertices = pointcloud->vertices;
    uint16_t* indices = pointcloud->indices;

    const uint32_t texcoords[4] =
    {
        texcoord_to_u32((Float2){{0.0f, 0.0f}}),
        texcoord_to_u32((Float2){{1.0f, 0.0f}}),
        texcoord_to_u32((Float2){{1.0f, 1.0f}}),
        texcoord_to_u32((Float2){{0.0f, 1.0f}}),
    };
    const Float2 offsets[4] =
    {
        {{-1.0f, -1.0f}},
        {{+1.0f, -1.0f}},
        {{+1.0f, +1.0f}},
        {{-1.0f, +1.0f}},
    };
    uint32_t colour_value = rgba_to_u32(colour);

    uint16_t base_index = array_count(vertices);

    Float3 center = vertex->position;
    for(int i = 0; i < 4; i += 1)
    {
        PointVertex v = {center, offsets[i], colour_value, texcoords[i]};
        ARRAY_ADD(vertices, v, heap);
    }

    ARRAY_ADD(indices, base_index + 0, heap);
    ARRAY_ADD(indices, base_index + 1, heap);
    ARRAY_ADD(indices, base_index + 2, heap);
    ARRAY_ADD(indices, base_index + 0, heap);
    ARRAY_ADD(indices, base_index + 2, heap);
    ARRAY_ADD(indices, base_index + 3, heap);

    pointcloud->vertices = vertices;
    pointcloud->indices = indices;
}

Pointcloud jan_make_pointcloud(JanMesh* mesh, Heap* heap, PointcloudSpec* spec)
{
    Pointcloud pointcloud = {0};

    if(spec->selection)
    {
        FOR_EACH_IN_POOL(JanVertex, vertex, mesh->vertex_pool)
        {
            if(vertex == spec->hovered)
            {
                add_to_pointcloud(vertex, spec->hover_colour, heap,
                        &pointcloud);
            }
            else if(jan_vertex_selected(spec->selection, vertex))
            {
                add_to_pointcloud(vertex, spec->select_colour, heap,
                        &pointcloud);
            }
            else
            {
                add_to_pointcloud(vertex, spec->colour, heap, &pointcloud);
            }
        }
    }
    else
    {
        FOR_EACH_IN_POOL(JanVertex, vertex, mesh->vertex_pool)
        {
            add_to_pointcloud(vertex, spec->colour, heap, &pointcloud);
        }
    }

    return pointcloud;
}

static void add_edge_to_wireframe(JanEdge* edge, Float4 colour, Heap* heap,
        Wireframe* wireframe)
{
    LineVertex* vertices = wireframe->vertices;
    uint16_t* indices = wireframe->indices;

    const uint32_t texcoords[4] =
    {
        texcoord_to_u32((Float2){{0.0f, 0.0f}}),
        texcoord_to_u32((Float2){{0.0f, 1.0f}}),
        texcoord_to_u32((Float2){{1.0f, 1.0f}}),
        texcoord_to_u32((Float2){{1.0f, 0.0f}}),
    };
    uint32_t colour_value = rgba_to_u32(colour);

    JanVertex* vertex = edge->vertices[0];
    JanVertex* other = edge->vertices[1];

    Float3 start = vertex->position;
    Float3 end = other->position;
    Float3 direction = float3_subtract(end, start);

    float left = -1.0f;
    float right = 1.0f;

    uint16_t base = array_count(vertices);

    LineVertex v0 = {end, float3_negate(direction), colour_value, texcoords[0], right};
    LineVertex v1 = {start, direction, colour_value, texcoords[1], left};
    LineVertex v2 = {start, direction, colour_value, texcoords[2], right};
    LineVertex v3 = {end, float3_negate(direction), colour_value, texcoords[3], left};
    ARRAY_ADD(vertices, v0, heap);
    ARRAY_ADD(vertices, v1, heap);
    ARRAY_ADD(vertices, v2, heap);
    ARRAY_ADD(vertices, v3, heap);

    ARRAY_ADD(indices, base + 1, heap);
    ARRAY_ADD(indices, base + 2, heap);
    ARRAY_ADD(indices, base + 0, heap);
    ARRAY_ADD(indices, base + 0, heap);
    ARRAY_ADD(indices, base + 2, heap);
    ARRAY_ADD(indices, base + 3, heap);

    wireframe->vertices = vertices;
    wireframe->indices = indices;
}

Wireframe jan_make_wireframe(JanMesh* mesh, Heap* heap, WireframeSpec* spec)
{
    Wireframe wireframe = {0};

    if(spec->selection)
    {
        FOR_EACH_IN_POOL(JanEdge, edge, mesh->edge_pool)
        {
            if(edge == spec->hovered)
            {
                add_edge_to_wireframe(edge, spec->hover_colour, heap,
                        &wireframe);
            }
            else if(jan_edge_selected(spec->selection, edge))
            {
                add_edge_to_wireframe(edge, spec->select_colour, heap,
                        &wireframe);
            }
            else
            {
                add_edge_to_wireframe(edge, spec->colour, heap, &wireframe);
            }
        }
    }
    else
    {
        FOR_EACH_IN_POOL(JanEdge, edge, mesh->edge_pool)
        {
            add_edge_to_wireframe(edge, spec->colour, heap, &wireframe);
        }
    }

    return wireframe;
}

static float signed_double_area(Float2 v0, Float2 v1, Float2 v2)
{
    Float2 a = float2_subtract(v0, v2);
    Float2 b = float2_subtract(v1, v2);
    return float2_determinant(a, b);
}

static bool is_clockwise(Float2 v0, Float2 v1, Float2 v2)
{
    return signed_double_area(v0, v1, v2) < 0.0f;
}

static bool point_in_triangle(Float2 v0, Float2 v1, Float2 v2, Float2 p)
{
    bool f0 = signed_double_area(p, v0, v1) < 0.0f;
    bool f1 = signed_double_area(p, v1, v2) < 0.0f;
    bool f2 = signed_double_area(p, v2, v0) < 0.0f;
    return (f0 == f1) && (f1 == f2);
}

static bool are_vertices_clockwise(Float2* vertices, int vertices_count)
{
    float d = 0.0f;
    for(int i = 0; i < vertices_count; i += 1)
    {
        Float2 v0 = vertices[i];
        Float2 v1 = vertices[(i + 1) % vertices_count];
        d += (v1.x - v0.x) * (v1.y + v0.y);
    }
    return d < 0.0f;
}

// Diagonal from ⟨a1,b⟩ is between ⟨a1,a0⟩ and ⟨a1,a2⟩
static bool locally_inside(Float2 a0, Float2 a1, Float2 a2, Float2 b)
{
    if(signed_double_area(a0, a1, a2) < 0)
    {
        return signed_double_area(a1, b, a2) >= 0
            && signed_double_area(a1, a0, b) >= 0;
    }
    else
    {
        return signed_double_area(a1, b, a0) < 0
            || signed_double_area(a1, a2, b) < 0;
    }
}

typedef struct FlatLoop
{
    VertexPNC* vertices;
    Float2* positions;
    int edges;
    int rightmost;
} FlatLoop;

static int get_rightmost(Float2* positions, int positions_count)
{
    int rightmost = 0;
    float max = -infinity;
    for(int i = 0; i < positions_count; i += 1)
    {
        if(positions[i].x > max)
        {
            max = positions[i].x;
            rightmost = i;
        }
    }
    return rightmost;
}

static int find_bridge_to_hole(FlatLoop* loop, FlatLoop* hole)
{
    int rightmost = hole->rightmost;
    Float2 h = hole->positions[rightmost];

    // Find the closest possible point on an edge to bridge to.
    int candidate = invalid_index;
    float min = infinity;
    for(int i = 0; i < loop->edges; i += 1)
    {
        int i_next = (i + 1) % loop->edges;
        Float2 e0 = loop->positions[i_next];
        Float2 e1 = loop->positions[i];

        if(h.y <= e0.y && h.y >= e1.y && e1.y != e0.y)
        {
            // intersection coordinate of a ray pointing in the positive-x
            // direction
            float x = e0.x + (h.y - e0.y) * (e1.x - e0.x) / (e1.y - e0.y);

            if(x >= h.x && x < min)
            {
                min = x;
                if(x == h.x)
                {
                    if(h.y == e0.y)
                    {
                        return i;
                    }
                    if(h.y == e1.y)
                    {
                        return i_next;
                    }
                }
                if(e0.x < e1.x)
                {
                    candidate = i;
                }
                else
                {
                    candidate = i_next;
                }
            }
        }
    }

    if(!is_valid_index(candidate))
    {
        return invalid_index;
    }

    if(h.x == min)
    {
        return mod(candidate - 1, loop->edges);
    }

    // Take a triangle between the intersection point, the hole vertex, and the
    // endpoint of the intersected edge of the outer polygon.
    Float2 m = loop->positions[candidate];

    Float2 v[3];
    v[1].x = m.x;
    if(h.y < m.y)
    {
        v[0].x = h.x;
        v[2].x = min;
    }
    else
    {
        v[0].x = min;
        v[2].x = h.x;
    }
    v[0].y = h.y;
    v[1].y = m.y;
    v[2].y = h.y;

    // Find vertices in the triangle of edges that would block the view between
    // the hole vertex and the outer polygon vertex. If there are points found,
    // choose the one that minimizes the angle between the x-direction ray and
    // the ray from hole vertex to connection vertex. Also, if there are
    // multiple of those, choose the closest.
    float mx = m.x;
    float max = -infinity;
    for(int i = 0; i < loop->edges; i += 1)
    {
        Float2 p = loop->positions[i];
        if(h.x >= p.x && p.x >= mx && h.x != p.x && point_in_triangle(v[0], v[1], v[2], p))
        {
            float current = fabsf(h.y - p.y) / (h.x - p.x);
            m = loop->positions[candidate];

            int i_prior = mod(i - 1, loop->edges);
            int i_next = mod(i + 1, loop->edges);
            Float2 prior = loop->positions[i_prior];
            Float2 next = loop->positions[i_next];

            if((current > max || (current == max && p.x < m.x)) && locally_inside(prior, p, next, h))
            {
                candidate = i;
                max = current;
            }
        }
    }

    return candidate;
}

static void bridge_hole(FlatLoop* loop, int bridge_index, FlatLoop* hole, Heap* heap)
{
    int original_edges = loop->edges;

    loop->edges += hole->edges + 2;
    loop->positions = HEAP_REALLOCATE(heap, loop->positions, Float2, loop->edges);
    loop->vertices = HEAP_REALLOCATE(heap, loop->vertices, VertexPNC, loop->edges);

    int count = original_edges - bridge_index;
    COPY_ARRAY(&loop->positions[bridge_index + hole->edges + 2], &loop->positions[bridge_index], count);
    COPY_ARRAY(&loop->vertices[bridge_index + hole->edges + 2], &loop->vertices[bridge_index], count);

    int to_end = hole->edges - hole->rightmost;
    COPY_ARRAY(&loop->positions[bridge_index + 1], &hole->positions[hole->rightmost], to_end);
    COPY_ARRAY(&loop->vertices[bridge_index + 1], &hole->vertices[hole->rightmost], to_end);

    COPY_ARRAY(&loop->positions[bridge_index + to_end + 1], hole->positions, hole->rightmost + 1);
    COPY_ARRAY(&loop->vertices[bridge_index + to_end + 1], hole->vertices, hole->rightmost + 1);

    REVERSE_ARRAY(Float2, &loop->positions[bridge_index + 1], hole->edges + 1);
    REVERSE_ARRAY(VertexPNC, &loop->vertices[bridge_index + 1], hole->edges + 1);
}

static bool is_right(FlatLoop l0, FlatLoop l1)
{
    return l0.positions[l0.rightmost].x > l1.positions[l1.rightmost].x;
}

DEFINE_QUICK_SORT(FlatLoop, is_right, by_rightmost);

static FlatLoop eliminate_holes(JanFace* face, Heap* heap)
{
    Matrix3 transform = matrix3_transpose(matrix3_orthogonal_basis(face->normal));

    FlatLoop* queue = HEAP_ALLOCATE(heap, FlatLoop, face->borders_count - 1);
    int added = 0;
    for(JanBorder* border = face->first_border->next; border; border = border->next)
    {
        int edges = jan_count_border_edges(border);
        Float2* projected = HEAP_ALLOCATE(heap, Float2, edges);
        VertexPNC* vertices = HEAP_ALLOCATE(heap, VertexPNC, edges);
        JanLink* link = border->first;
        for(int i = 0; i < edges; i += 1)
        {
            projected[i] = matrix3_transform(transform, link->vertex->position);
            vertices[i].position = link->vertex->position;
            vertices[i].normal = face->normal;
            vertices[i].colour = rgb_to_u32(link->colour);
            link = link->next;
        }

        if(!are_vertices_clockwise(projected, edges))
        {
            REVERSE_ARRAY(Float2, projected, edges);
            REVERSE_ARRAY(VertexPNC, vertices, edges);
        }

        int rightmost = get_rightmost(projected, edges);

        queue[added].positions = projected;
        queue[added].vertices = vertices;
        queue[added].edges = edges;
        queue[added].rightmost = rightmost;
        added += 1;
    }

    quick_sort_by_rightmost(queue, added);

    FlatLoop loop;
    loop.edges = face->edges;
    Float2* projected = HEAP_ALLOCATE(heap, Float2, loop.edges);
    VertexPNC* vertices = HEAP_ALLOCATE(heap, VertexPNC, loop.edges);
    JanLink* link = face->first_border->first;
    for(int i = 0; i < loop.edges; i += 1)
    {
        projected[i] = matrix3_transform(transform, link->vertex->position);
        vertices[i].position = link->vertex->position;
        vertices[i].normal = face->normal;
        vertices[i].colour = rgb_to_u32(link->colour);
        link = link->next;
    }
    loop.positions = projected;
    loop.vertices = vertices;

    for(int i = 0; i < added; i += 1)
    {
        int index = find_bridge_to_hole(&loop, &queue[i]);
        // If a bridge isn't found, it doesn't include the hole in the final
        // polygon. This makes sense for triangulation for display, but may not
        // be appropriate fallback if this code is reused for eliminating
        // holes on export!
        if(is_valid_index(index))
        {
            bridge_hole(&loop, index, &queue[i], heap);
        }
    }

    for(int i = 0; i < added; i += 1)
    {
        HEAP_DEALLOCATE(heap, queue[i].positions);
        HEAP_DEALLOCATE(heap, queue[i].vertices);
    }

    HEAP_DEALLOCATE(heap, queue);

    return loop;
}

static bool is_triangle_vertex(Float2 v0, Float2 v1, Float2 v2, Float2 point)
{
    return float2_exactly_equals(v0, point)
            || float2_exactly_equals(v1, point)
            || float2_exactly_equals(v2, point);
}

static void triangulate_face(JanFace* face, Heap* heap,
        Triangulation* triangulation)
{
    VertexPNC* vertices = triangulation->vertices;
    uint16_t* indices = triangulation->indices;

    FlatLoop loop;
    if(face->borders_count > 1)
    {
        loop = eliminate_holes(face, heap);
    }
    else
    {
        // The face is already a triangle.
        if(face->edges == 3)
        {
            ARRAY_RESERVE(vertices, 3, heap);
            ARRAY_RESERVE(indices, 3, heap);
            JanLink* link = face->first_border->first;
            for(int i = 0; i < 3; i += 1)
            {
                VertexPNC vertex;
                vertex.position = link->vertex->position;
                vertex.normal = face->normal;
                vertex.colour = rgb_to_u32(link->colour);
                int index = array_count(vertices);
                ARRAY_ADD(vertices, vertex, heap);
                ARRAY_ADD(indices, index, heap);
                link = link->next;
            }
            triangulation->vertices = vertices;
            triangulation->indices = indices;
            return;
        }

        // Project vertices onto a plane to produce 2D coordinates. Also, copy
        // over all the vertices.
        loop.edges = face->edges;
        loop.positions = HEAP_ALLOCATE(heap, Float2, loop.edges);
        loop.vertices = HEAP_ALLOCATE(heap, VertexPNC, loop.edges);
        Matrix3 m = matrix3_orthogonal_basis(face->normal);
        Matrix3 mi = matrix3_transpose(m);
        JanLink* link = face->first_border->first;
        for(int i = 0; i < loop.edges; i += 1)
        {
            loop.positions[i] = matrix3_transform(mi, link->vertex->position);
            loop.vertices[i].position = link->vertex->position;
            loop.vertices[i].normal = face->normal;
            loop.vertices[i].colour = rgb_to_u32(link->colour);
            link = link->next;
        }
    }

    // Save the index before adding any vertices for this face so it can be
    // used as a base for ear indexing.
    uint16_t base_index = array_count(vertices);

    // Copy all of the vertices in the face.
    ARRAY_RESERVE(vertices, loop.edges, heap);
    for(int i = 0; i < loop.edges; i += 1)
    {
        VertexPNC vertex = loop.vertices[i];
        ARRAY_ADD(vertices, vertex, heap);
    }

    // The projection may reverse the winding of the triangle. Reversing
    // the projected vertices would be the most obvious way to handle this
    // case. However, this would mean the projected and unprojected vertices
    // would no longer have the same index. So, reverse the order that the
    // chains are used to index the projected vertices later during
    // ear-finding.
    bool reverse_winding = false;
    if(!are_vertices_clockwise(loop.positions, loop.edges))
    {
        reverse_winding = true;
    }

    // Keep vertex chains to walk both ways around the polygon.
    int* l = HEAP_ALLOCATE(heap, int, loop.edges);
    int* r = HEAP_ALLOCATE(heap, int, loop.edges);
    for(int i = 0; i < loop.edges; i += 1)
    {
        l[i] = mod(i - 1, loop.edges);
        r[i] = mod(i + 1, loop.edges);
    }

    // A polygon always has exactly n - 2 triangles, where n is the number
    // of edges in the polygon.
    ARRAY_RESERVE(indices, 3 * (loop.edges - 2), heap);

    // Walk the right loop and find ears to triangulate using each of those
    // vertices.
    int j = loop.edges - 1;
    int triangles_this_face = 0;
    while(triangles_this_face < loop.edges - 2)
    {
        j = r[j];

        Float2 v[3];
        if(reverse_winding)
        {
            v[0] = loop.positions[r[j]];
            v[1] = loop.positions[j];
            v[2] = loop.positions[l[j]];
        }
        else
        {
            v[0] = loop.positions[l[j]];
            v[1] = loop.positions[j];
            v[2] = loop.positions[r[j]];
        }

        if(is_clockwise(v[0], v[1], v[2]))
        {
            continue;
        }

        bool in_triangle = false;
        for(int k = 0; k < loop.edges; k += 1)
        {
            Float2 point = loop.positions[k];
            if(!is_triangle_vertex(v[0], v[1], v[2], point)
                && point_in_triangle(v[0], v[1], v[2], point))
            {
                in_triangle = true;
                break;
            }
        }
        if(!in_triangle)
        {
            // An ear has been found.
            ARRAY_ADD(indices, base_index + l[j], heap);
            ARRAY_ADD(indices, base_index + j, heap);
            ARRAY_ADD(indices, base_index + r[j], heap);
            triangles_this_face += 1;

            l[r[j]] = l[j];
            r[l[j]] = r[j];
        }
    }

    HEAP_DEALLOCATE(heap, loop.positions);
    HEAP_DEALLOCATE(heap, loop.vertices);
    HEAP_DEALLOCATE(heap, l);
    HEAP_DEALLOCATE(heap, r);

    triangulation->vertices = vertices;
    triangulation->indices = indices;
}

Triangulation jan_triangulate(JanMesh* mesh, Heap* heap)
{
    Triangulation triangulation = {0};

    FOR_EACH_IN_POOL(JanFace, face, mesh->face_pool)
    {
        triangulate_face(face, heap, &triangulation);
    }

    return triangulation;
}

Triangulation jan_triangulate_selection(JanMesh* mesh, JanSelection* selection,
        Heap* heap)
{
    ASSERT(selection->type == JAN_SELECTION_TYPE_FACE);

    Triangulation triangulation = {0};

    FOR_ALL(JanPart, selection->parts)
    {
        triangulate_face(it->face, heap, &triangulation);
    }

    return triangulation;
}
