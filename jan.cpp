#include "jan.h"

#include "array2.h"
#include "assert.h"
#include "float_utilities.h"
#include "int_utilities.h"
#include "invalid_index.h"
#include "logging.h"
#include "map.h"
#include "math_basics.h"
#include "sorting.h"

namespace jan {

void create_mesh(Mesh* mesh)
{
    pool_create(&mesh->face_pool, sizeof(Face), 1024);
    pool_create(&mesh->edge_pool, sizeof(Edge), 4096);
    pool_create(&mesh->vertex_pool, sizeof(Vertex), 4096);
    pool_create(&mesh->link_pool, sizeof(Link), 8192);
    pool_create(&mesh->border_pool, sizeof(Link), 8192);

    mesh->faces_count = 0;
    mesh->edges_count = 0;
    mesh->vertices_count = 0;
}

void destroy_mesh(Mesh* mesh)
{
    pool_destroy(&mesh->face_pool);
    pool_destroy(&mesh->edge_pool);
    pool_destroy(&mesh->vertex_pool);
    pool_destroy(&mesh->link_pool);
    pool_destroy(&mesh->border_pool);
}

Vertex* add_vertex(Mesh* mesh, Vector3 position)
{
    Vertex* vertex = POOL_ALLOCATE(&mesh->vertex_pool, Vertex);
    vertex->position = position;

    mesh->vertices_count += 1;

    return vertex;
}

static Spoke* get_spoke(Edge* edge, Vertex* vertex)
{
    if(edge->vertices[0] == vertex)
    {
        return &edge->spokes[0];
    }
    else
    {
        return &edge->spokes[1];
    }
}

static void add_spoke(Edge* edge, Vertex* vertex)
{
    Edge* existing_edge = vertex->any_edge;
    if(existing_edge)
    {
        Spoke* a = get_spoke(edge, vertex);
        Spoke* b = get_spoke(existing_edge, vertex);
        if(b->prior)
        {
            Spoke* c = get_spoke(b->prior, vertex);
            c->next = edge;
        }
        a->next = existing_edge;
        a->prior = b->prior;
        b->prior = edge;
    }
    else
    {
        vertex->any_edge = edge;
        Spoke* spoke = get_spoke(edge, vertex);
        spoke->next = edge;
        spoke->prior = edge;
    }
}

static void remove_spoke(Edge* edge, Vertex* vertex)
{
    Spoke* spoke = get_spoke(edge, vertex);
    if(spoke->next)
    {
        Spoke* other = get_spoke(spoke->next, vertex);
        other->prior = spoke->prior;
    }
    if(spoke->prior)
    {
        Spoke* other = get_spoke(spoke->prior, vertex);
        other->next = spoke->next;
    }
    if(vertex->any_edge == edge)
    {
        if(spoke->next == edge)
        {
            vertex->any_edge = nullptr;
        }
        else
        {
            vertex->any_edge = spoke->next;
        }
    }
    spoke->next = nullptr;
    spoke->prior = nullptr;
}

static bool edge_contains_vertices(Edge* edge, Vertex* a, Vertex* b)
{
    return (edge->vertices[0] == a && edge->vertices[1] == b)
        || (edge->vertices[1] == a && edge->vertices[0] == b);
}

static Edge* get_edge_spoked_from_vertex(Vertex* hub, Vertex* vertex)
{
    if(!hub->any_edge)
    {
        return nullptr;
    }
    Edge* first = hub->any_edge;
    Edge* edge = first;
    do
    {
        if(edge_contains_vertices(edge, hub, vertex))
        {
            return edge;
        }
        edge = get_spoke(edge, hub)->next;
    } while(edge != first);
    return nullptr;
}

Edge* add_edge(Mesh* mesh, Vertex* start, Vertex* end)
{
    Edge* edge = POOL_ALLOCATE(&mesh->edge_pool, Edge);
    edge->vertices[0] = start;
    edge->vertices[1] = end;

    add_spoke(edge, start);
    add_spoke(edge, end);

    mesh->edges_count += 1;

    return edge;
}

static Edge* add_edge_if_nonexistant(Mesh* mesh, Vertex* start, Vertex* end)
{
    Edge* edge = get_edge_spoked_from_vertex(start, end);
    if(edge)
    {
        return edge;
    }
    else
    {
        return add_edge(mesh, start, end);
    }
}

static Link* add_link(Mesh* mesh, Vertex* vertex, Edge* edge, Face* face)
{
    Link* link = POOL_ALLOCATE(&mesh->link_pool, Link);
    link->vertex = vertex;
    link->edge = edge;
    link->face = face;

    return link;
}

static bool is_boundary(Link* link)
{
    return link == link->next_fin;
}

static void make_boundary(Link* link)
{
    link->next_fin = link;
    link->prior_fin = link;
}

static void add_fin(Link* link, Edge* edge)
{
    Link* existing_link = edge->any_link;
    if(existing_link)
    {
        link->prior_fin = existing_link;
        link->next_fin = existing_link->next_fin;

        existing_link->next_fin->prior_fin = link;
        existing_link->next_fin = link;
    }
    else
    {
        make_boundary(link);
    }
    edge->any_link = link;
    link->edge = edge;
}

static void remove_fin(Link* link, Edge* edge)
{
    if(is_boundary(link))
    {
        ASSERT(edge->any_link == link);
        edge->any_link = nullptr;
    }
    else
    {
        if(edge->any_link == link)
        {
            edge->any_link = link->next_fin;
        }
        link->next_fin->prior_fin = link->prior_fin;
        link->prior_fin->next_fin = link->next_fin;
    }
    link->next_fin = nullptr;
    link->prior_fin = nullptr;
    link->edge = nullptr;
}

static Link* add_border_to_face(Mesh* mesh, Vertex* vertex, Edge* edge, Face* face)
{
    Link* link = add_link(mesh, vertex, edge, face);
    add_fin(link, edge);

    Border* border = POOL_ALLOCATE(&mesh->border_pool, Border);
    border->first = link;
    border->last = link;
    border->next = nullptr;
    border->prior = face->last_border;
    if(!face->first_border)
    {
        face->first_border = border;
    }
    if(face->last_border)
    {
        face->last_border->next = border;
    }
    face->last_border = border;

    face->borders_count += 1;

    return link;
}

static void add_and_link_border(Mesh* mesh, Face* face, Vertex** vertices, Edge** edges, int edges_count)
{
    // Create each link in the face and chain it to the previous.
    Link* first = add_border_to_face(mesh, vertices[0], edges[0], face);
    Link* prior = first;
    for(int i = 1; i < edges_count; i += 1)
    {
        Link* link = add_link(mesh, vertices[i], edges[i], face);
        add_fin(link, edges[i]);

        prior->next = link;
        link->prior = prior;

        prior = link;
    }
    // Connect the ends to close the loop.
    first->prior = prior;
    prior->next = first;
}

Face* add_face(Mesh* mesh, Vertex** vertices, Edge** edges, int edges_count)
{
    Face* face = POOL_ALLOCATE(&mesh->face_pool, Face);
    face->edges = edges_count;

    add_and_link_border(mesh, face, vertices, edges, edges_count);

    mesh->faces_count += 1;

    return face;
}

static Face* connect_vertices_and_add_face(Mesh* mesh, Vertex** vertices, int vertices_count, Stack* stack)
{
    Edge** edges = STACK_ALLOCATE(stack, Edge*, vertices_count);
    int end = vertices_count - 1;
    for(int i = 0; i < end; i += 1)
    {
        edges[i] = add_edge(mesh, vertices[i], vertices[i + 1]);
    }
    edges[end] = add_edge(mesh, vertices[end], vertices[0]);

    Face* face = add_face(mesh, vertices, edges, vertices_count);
    STACK_DEALLOCATE(stack, edges);

    return face;
}

Face* connect_disconnected_vertices_and_add_face(Mesh* mesh, Vertex** vertices, int vertices_count, Stack* stack)
{
    Edge** edges = STACK_ALLOCATE(stack, Edge*, vertices_count);
    int end = vertices_count - 1;
    for(int i = 0; i < end; i += 1)
    {
        edges[i] = add_edge_if_nonexistant(mesh, vertices[i], vertices[i + 1]);
    }
    edges[end] = add_edge_if_nonexistant(mesh, vertices[end], vertices[0]);

    Face* face = add_face(mesh, vertices, edges, vertices_count);
    STACK_DEALLOCATE(stack, edges);

    return face;
}

static void connect_vertices_and_add_hole(Mesh* mesh, Face* face, Vertex** vertices, int vertices_count, Stack* stack)
{
    Edge** edges = STACK_ALLOCATE(stack, Edge*, vertices_count);
    int end = vertices_count - 1;
    for(int i = 0; i < end; i += 1)
    {
        edges[i] = add_edge(mesh, vertices[i], vertices[i + 1]);
    }
    edges[end] = add_edge(mesh, vertices[end], vertices[0]);

    add_and_link_border(mesh, face, vertices, edges, vertices_count);

    STACK_DEALLOCATE(stack, edges);
}

void remove_face(Mesh* mesh, Face* face)
{
    Border* next_border;
    for(Border* border = face->first_border; border; border = next_border)
    {
        next_border = border->next;
        Link* first = border->first;
        Link* link = first;
        do
        {
            Link* next = link->next;
            remove_fin(link, link->edge);
            pool_deallocate(&mesh->link_pool, link);
            link = next;
        } while(link != first);
        pool_deallocate(&mesh->border_pool, border);
    }
    pool_deallocate(&mesh->face_pool, face);
    mesh->faces_count -= 1;
}

void remove_face_and_its_unlinked_edges_and_vertices(Mesh* mesh, Face* face)
{
    Border* next_border;
    for(Border* border = face->first_border; border; border = next_border)
    {
        next_border = border->next;
        Link* first = border->first;
        Link* link = first;
        do
        {
            Link* next = link->next;
            Edge* edge = link->edge;
            remove_fin(link, edge);
            pool_deallocate(&mesh->link_pool, link);
            if(!edge->any_link)
            {
                Vertex* vertices[2];
                vertices[0] = edge->vertices[0];
                vertices[1] = edge->vertices[1];
                remove_spoke(edge, vertices[0]);
                remove_spoke(edge, vertices[1]);
                pool_deallocate(&mesh->edge_pool, edge);
                mesh->edges_count -= 1;
                if(!vertices[0]->any_edge)
                {
                    pool_deallocate(&mesh->vertex_pool, vertices[0]);
                    mesh->vertices_count -= 1;
                }
                if(!vertices[1]->any_edge)
                {
                    pool_deallocate(&mesh->vertex_pool, vertices[1]);
                    mesh->vertices_count -= 1;
                }
            }
            link = next;
        } while(link != first);
        pool_deallocate(&mesh->border_pool, border);
    }
    pool_deallocate(&mesh->face_pool, face);
    mesh->faces_count -= 1;
}

void remove_edge(Mesh* mesh, Edge* edge)
{
    while(edge->any_link)
    {
        remove_face(mesh, edge->any_link->face);
    }
    remove_spoke(edge, edge->vertices[0]);
    remove_spoke(edge, edge->vertices[1]);
    pool_deallocate(&mesh->edge_pool, edge);
    mesh->edges_count -= 1;
}

void remove_vertex(Mesh* mesh, Vertex* vertex)
{
    while(vertex->any_edge)
    {
        remove_edge(mesh, vertex->any_edge);
    }
    pool_deallocate(&mesh->vertex_pool, vertex);
    mesh->vertices_count -= 1;
}

static void reverse_face_winding(Face* face)
{
    for(Border* border = face->first_border; border; border = border->next)
    {
        Link* first = border->first;
        Link* link = first;
        Link* prior_next_fin = link->prior->next_fin;
        Link* prior_prior_fin = link->prior->prior_fin;
        bool boundary_prior = is_boundary(prior_next_fin);
        Edge* prior_edge = link->prior->edge;
        do
        {
            Link* next_fin = link->next_fin;
            Link* prior_fin = link->prior_fin;
            bool boundary = is_boundary(next_fin);

            // Reverse the fins.
            if(boundary_prior)
            {
                make_boundary(link);
            }
            else
            {
                link->next_fin = prior_next_fin;
                link->prior_fin = prior_prior_fin;
                prior_next_fin->prior_fin = link;
                prior_prior_fin->next_fin = link;
            }
            prior_next_fin = next_fin;
            prior_prior_fin = prior_fin;
            boundary_prior = boundary;

            // Rotate the edge's reference to the link loop forward one link
            // and rotate the link's reference backward one edge.
            Edge* edge = link->edge;
            if(edge->any_link == link)
            {
                edge->any_link = link->next;
            }
            link->edge = prior_edge;
            prior_edge = edge;

            // Reverse the link itself.
            Link* temp = link->next;
            link->next = link->prior;
            link->prior = temp;
            link = temp;
        } while(link != first);
    }
}

static void flip_face_normal(Face* face)
{
    reverse_face_winding(face);
    face->normal = -face->normal;
}

static void compute_vertex_normal(Vertex* vertex)
{
    // This just averages them and does not take into account face area.
    Edge* edge = vertex->any_edge;
    if(edge)
    {
        Edge* first = edge;
        Vector3 normal = vector3_zero;
        do
        {
            Vertex* other;
            Spoke spoke;
            if(edge->vertices[0] == vertex)
            {
                other = edge->vertices[1];
                spoke = edge->spokes[0];
            }
            else
            {
                other = edge->vertices[0];
                spoke = edge->spokes[1];
            }
            normal += vertex->position - other->position;
            edge = spoke.next;
        } while(edge != first);
        vertex->normal = normalise(normal);
    }
}

static void compute_face_normal(Face* face)
{
    // This uses Newell's Method to compute the polygon normal.
    Link* link = face->first_border->first;
    Link* first = link;
    Vector3 prior = link->prior->vertex->position;
    Vector3 current = link->vertex->position;
    Vector3 normal = vector3_zero;
    do
    {
        normal.x += (prior.y - current.y) * (prior.z + current.z);
        normal.y += (prior.z - current.z) * (prior.x + current.x);
        normal.z += (prior.x - current.x) * (prior.y + current.y);
        prior = current;
        link = link->next;
        current = link->vertex->position;
    } while(link != first);
    face->normal = normalise(normal);
}

void update_normals(Mesh* mesh)
{
    FOR_EACH_IN_POOL(Face, face, mesh->face_pool)
    {
        compute_face_normal(face);
    }
    FOR_EACH_IN_POOL(Vertex, vertex, mesh->vertex_pool)
    {
        compute_vertex_normal(vertex);
    }
}

void make_a_weird_face(Mesh* mesh, Stack* stack)
{
    const int vertices_count = 8;
    Vector3 positions[vertices_count];
    positions[0] = {-0.20842f, +0.20493f, 0.0f};
    positions[1] = {+0.53383f, -0.31467f, 0.0f};
    positions[2] = {+0.19402f, -0.55426f, 0.0f};
    positions[3] = {+0.86623f, -0.76310f, 0.0f};
    positions[4] = {+0.58252f, +0.83783f, 0.0f};
    positions[5] = {-0.58114f, +0.56986f, 0.0f};
    positions[6] = {-0.59335f, -0.28583f, 0.0f};
    positions[7] = {-0.05012f, -0.82722f, 0.0f};

    Vertex* vertices[vertices_count];
    for(int i = 0; i < vertices_count; i += 1)
    {
        vertices[i] = add_vertex(mesh, positions[i]);
    }

    Face* face = connect_vertices_and_add_face(mesh, vertices, vertices_count, stack);

    compute_face_normal(face);
}

void make_a_face_with_holes(Mesh* mesh, Stack* stack)
{
    const int vertices_count = 7;
    Vector3 positions[vertices_count] =
    {
        {+1.016774f, -0.128711f, 0.0f},
        {+1.005646f, +1.246329f, 0.0f},
        {-0.160719f, -0.121287f, 0.0f},
        {-0.744234f, +1.375802f, 0.0f},
        {-2.254874f, +0.459116f, 0.0f},
        {-1.812329f, -0.432525f, 0.0f},
        {+0.000000f, -1.000000f, 0.0f},
    };

    Vertex* vertices[vertices_count];
    for(int i = 0; i < vertices_count; i += 1)
    {
        vertices[i] = add_vertex(mesh, positions[i]);
    }

    Face* face = connect_vertices_and_add_face(mesh, vertices, vertices_count, stack);

    compute_face_normal(face);

    Vector3 hole0_positions[5] =
    {
        {-0.543713f, -0.318739f, 0.0f},
        {-0.716260f, -0.565462f, 0.0f},
        {-1.659353f, -0.253382f, 0.0f},
        {-1.602318f, +0.377146f, 0.0f},
        {-0.852411f, +0.512023f, 0.0f},
    };

    for(int i = 0; i < 5; i += 1)
    {
        vertices[i] = add_vertex(mesh, hole0_positions[i]);
    }

    connect_vertices_and_add_hole(mesh, face, vertices, 5, stack);

    Vector3 hole1_positions[5] =
    {
        {+0.502821f, +0.337892f, 0.0f},
        {+0.755197f, +0.412048f, 0.0f},
        {+0.717627f, +0.185694f, 0.0f},
        {+0.579880f, +0.063448f, 0.0f},
        {+0.361475f, +0.204754f, 0.0f},
    };

    for(int i = 0; i < 5; i += 1)
    {
        vertices[i] = add_vertex(mesh, hole1_positions[i]);
    }

    connect_vertices_and_add_hole(mesh, face, vertices, 5, stack);
}

void make_wireframe(Mesh* mesh, Heap* heap, Vector4 colour, LineVertex** out_vertices, u16** out_indices)
{
    LineVertex* vertices = nullptr;
    u16* indices = nullptr;

    const u32 texcoords[4] =
    {
        texcoord_to_u32({0.0f, 0.0f}),
        texcoord_to_u32({0.0f, 1.0f}),
        texcoord_to_u32({1.0f, 1.0f}),
        texcoord_to_u32({1.0f, 0.0f}),
    };

    FOR_EACH_IN_POOL(Edge, edge, mesh->edge_pool)
    {
        Vertex* vertex = edge->vertices[0];
        Vertex* other = edge->vertices[1];

        Vector3 start = vertex->position;
        Vector3 end = other->position;
        Vector3 direction = end - start;

        u32 colour_value = rgba_to_u32(colour);
        float left = -1.0f;
        float right = 1.0f;

        u16 base = ARRAY_COUNT(vertices);

        LineVertex v0 = {end, -direction, colour_value, texcoords[0], right};
        LineVertex v1 = {start, direction, colour_value, texcoords[1], left};
        LineVertex v2 = {start, direction, colour_value, texcoords[2], right};
        LineVertex v3 = {end, -direction, colour_value, texcoords[3], left};
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
    }

    *out_vertices = vertices;
    *out_indices = indices;
}

static float signed_double_area(Vector2 v0, Vector2 v1, Vector2 v2)
{
    return (v0.x - v2.x) * (v1.y - v2.y) - (v1.x - v2.x) * (v0.y - v2.y);
}

static bool is_clockwise(Vector2 v0, Vector2 v1, Vector2 v2)
{
    return signed_double_area(v0, v1, v2) < 0.0f;
}

static bool point_in_triangle(Vector2 v0, Vector2 v1, Vector2 v2, Vector2 p)
{
    bool f0 = signed_double_area(p, v0, v1) < 0.0f;
    bool f1 = signed_double_area(p, v1, v2) < 0.0f;
    bool f2 = signed_double_area(p, v2, v0) < 0.0f;
    return (f0 == f1) && (f1 == f2);
}

static bool is_clockwise(Vector2* vertices, int vertices_count)
{
    float d = 0.0f;
    for(int i = 0; i < vertices_count; i += 1)
    {
        Vector2 v0 = vertices[i];
        Vector2 v1 = vertices[(i + 1) % vertices_count];
        d += (v1.x - v0.x) * (v1.y + v0.y);
    }
    return d < 0.0f;
}

// Diagonal from ⟨a1,b⟩ is between ⟨a1,a0⟩ and ⟨a1,a2⟩
static bool locally_inside(Vector2 a0, Vector2 a1, Vector2 a2, Vector2 b)
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

static int count_border_edges(Border* border)
{
    int count = 0;
    Link* first = border->first;
    Link* link = first;
    do
    {
        count += 1;
        link = link->next;
    } while(link != first);
    return count;
}

struct FlatLoop
{
    VertexPNC* vertices;
    Vector2* positions;
    int edges;
    int rightmost;
};

static int get_rightmost(Vector2* positions, int positions_count)
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
    Vector2 h = hole->positions[rightmost];

    // Find the closest possible point on an edge to bridge to.
    int candidate = invalid_index;
    float min = infinity;
    for(int i = 0; i < loop->edges; i += 1)
    {
        int i_next = (i + 1) % loop->edges;
        Vector2 e0 = loop->positions[i_next];
        Vector2 e1 = loop->positions[i];

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
    Vector2 m = loop->positions[candidate];

    Vector2 v[3];
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
        Vector2 p = loop->positions[i];
        if(h.x >= p.x && p.x >= mx && h.x != p.x && point_in_triangle(v[0], v[1], v[2], p))
        {
            float current = abs(h.y - p.y) / (h.x - p.x);
            m = loop->positions[candidate];

            int i_prior = mod(i - 1, loop->edges);
            int i_next = mod(i + 1, loop->edges);
            Vector2 prior = loop->positions[i_prior];
            Vector2 next = loop->positions[i_next];

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
    loop->positions = HEAP_REALLOCATE(heap, loop->positions, loop->edges);
    loop->vertices = HEAP_REALLOCATE(heap, loop->vertices, loop->edges);

    int count = original_edges - bridge_index;
    move_memory(&loop->positions[bridge_index + hole->edges + 2], &loop->positions[bridge_index], sizeof(*loop->positions) * count);
    move_memory(&loop->vertices[bridge_index + hole->edges + 2], &loop->vertices[bridge_index], sizeof(*loop->vertices) * count);

    int to_end = hole->edges - hole->rightmost;
    copy_memory(&loop->positions[bridge_index + 1], &hole->positions[hole->rightmost], sizeof(*hole->positions) * to_end);
    copy_memory(&loop->vertices[bridge_index + 1], &hole->vertices[hole->rightmost], sizeof(*hole->vertices) * to_end);

    copy_memory(&loop->positions[bridge_index + to_end + 1], hole->positions, sizeof(*hole->positions) * (hole->rightmost + 1));
    copy_memory(&loop->vertices[bridge_index + to_end + 1], hole->vertices, sizeof(*hole->vertices) * (hole->rightmost + 1));

    for(int start = bridge_index + 1, end = bridge_index + 1 + hole->edges + 1- 1; start < end; start += 1, end -= 1)
    {
        SWAP(loop->positions[start], loop->positions[end]);
        SWAP(loop->vertices[start], loop->vertices[end]);
    }
}

static bool is_right(FlatLoop l0, FlatLoop l1)
{
    return l0.positions[l0.rightmost].x > l1.positions[l1.rightmost].x;
}

DEFINE_QUICK_SORT(FlatLoop, is_right, by_rightmost);

static FlatLoop eliminate_holes(Face* face, Heap* heap)
{
    Matrix3 transform = transpose(orthogonal_basis(face->normal));

    FlatLoop* queue = HEAP_ALLOCATE(heap, FlatLoop, face->borders_count - 1);
    int added = 0;
    for(Border* border = face->first_border->next; border; border = border->next)
    {
        int edges = count_border_edges(border);
        Vector2* projected = HEAP_ALLOCATE(heap, Vector2, edges);
        VertexPNC* vertices = HEAP_ALLOCATE(heap, VertexPNC, edges);
        Link* link = border->first;
        for(int i = 0; i < edges; i += 1)
        {
            projected[i] = transform * link->vertex->position;
            vertices[i].position = link->vertex->position;
            vertices[i].normal = face->normal;
            vertices[i].colour = rgb_to_u32(link->colour);
            link = link->next;
        }

        if(!is_clockwise(projected, edges))
        {
            for(int start = 0, end = edges - 1; start < end; start += 1, end -= 1)
            {
                SWAP(projected[start], projected[end]);
                SWAP(vertices[start], vertices[end]);
            }
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
    Vector2* projected = HEAP_ALLOCATE(heap, Vector2, loop.edges);
    VertexPNC* vertices = HEAP_ALLOCATE(heap, VertexPNC, loop.edges);
    Link* link = face->first_border->first;
    for(int i = 0; i < loop.edges; i += 1)
    {
        projected[i] = transform * link->vertex->position;
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
        if(is_valid_index(index))
        {
            // This gives up and doesn't include the hole in the final polygon.
            // It makes sense for triangulation for display, but may not be
            // appropriate fallback if this code is reused for eliminating
            // holes on export!
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

static bool is_triangle_vertex(Vector2 v0, Vector2 v1, Vector2 v2, Vector2 point)
{
    return exactly_equals(v0, point) || exactly_equals(v1, point) || exactly_equals(v2, point);
}

static void triangulate_face(Face* face, Heap* heap, VertexPNC** vertices_array, u16** indices_array)
{
    VertexPNC* vertices = *vertices_array;
    u16* indices = *indices_array;

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
            Link* link = face->first_border->first;
            for(int i = 0; i < 3; i += 1)
            {
                VertexPNC vertex;
                vertex.position = link->vertex->position;
                vertex.normal = face->normal;
                vertex.colour = rgb_to_u32(link->colour);
                int index = ARRAY_COUNT(vertices);
                ARRAY_ADD(vertices, vertex, heap);
                ARRAY_ADD(indices, index, heap);
                link = link->next;
            }
            *vertices_array = vertices;
            *indices_array = indices;
            return;
        }

        // Project vertices onto a plane to produce 2D coordinates. Also, copy
        // over all the vertices.
        loop.edges = face->edges;
        loop.positions = HEAP_ALLOCATE(heap, Vector2, loop.edges);
        loop.vertices = HEAP_ALLOCATE(heap, VertexPNC, loop.edges);
        Matrix3 m = orthogonal_basis(face->normal);
        Matrix3 mi = transpose(m);
        Link* link = face->first_border->first;
        for(int i = 0; i < loop.edges; i += 1)
        {
            loop.positions[i] = mi * link->vertex->position;
            loop.vertices[i].position = link->vertex->position;
            loop.vertices[i].normal = face->normal;
            loop.vertices[i].colour = rgb_to_u32(link->colour);
            link = link->next;
        }
    }

    // Save the index before adding any vertices for this face so it can be
    // used as a base for ear indexing.
    u16 base_index = ARRAY_COUNT(vertices);

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
    if(!is_clockwise(loop.positions, loop.edges))
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

        Vector2 v[3];
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
            Vector2 point = loop.positions[k];
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

    *vertices_array = vertices;
    *indices_array = indices;
}

void triangulate(Mesh* mesh, Heap* heap, VertexPNC** vertices, u16** indices)
{
    *vertices = nullptr;
    *indices = nullptr;

    FOR_EACH_IN_POOL(Face, face, mesh->face_pool)
    {
        triangulate_face(face, heap, vertices, indices);
    }
}

void triangulate_selection(Mesh* mesh, Selection* selection, Heap* heap, VertexPNC** vertices, u16** indices)
{
    ASSERT(selection->type == Selection::Type::Face);

    *vertices = nullptr;
    *indices = nullptr;

    for(int i = 0; i < selection->parts_count; i += 1)
    {
        Face* face = selection->parts[i].face;
        triangulate_face(face, heap, vertices, indices);
    }
}

void create_selection(Selection* selection, Heap* heap)
{
    selection->heap = heap;
}

void destroy_selection(Selection* selection)
{
    if(selection && selection->heap)
    {
        SAFE_HEAP_DEALLOCATE(selection->heap, selection->parts);
        selection->parts_count = 0;
    }
}

static bool face_selected(Selection* selection, Face* face)
{
    for(int i = 0; i < selection->parts_count; i += 1)
    {
        if(selection->parts[i].face == face)
        {
            return true;
        }
    }
    return false;
}

Selection select_all(Mesh* mesh, Heap* heap)
{
    Selection selection;
    create_selection(&selection, heap);
    selection.type = Selection::Type::Face;
    selection.parts_count = mesh->faces_count;
    selection.parts = HEAP_ALLOCATE(selection.heap, Part, selection.parts_count);

    int i = 0;
    FOR_EACH_IN_POOL(Face, face, mesh->face_pool)
    {
        selection.parts[i].face = face;
        i += 1;
    }

    return selection;
}

static void add_face_to_selection(Selection* selection, Face* face)
{
    selection->type = Selection::Type::Face;
    selection->parts = HEAP_REALLOCATE(selection->heap, selection->parts, selection->parts_count + 1);
    selection->parts[selection->parts_count].face = face;
    selection->parts_count += 1;
}

static void remove_face_from_selection(Selection* selection, Face* face)
{
    int found_index = -1;
    for(int i = 0; i < selection->parts_count; i += 1)
    {
        if(selection->parts[i].face == face)
        {
            found_index = i;
            break;
        }
    }
    if(found_index != -1)
    {
        selection->parts[found_index] = selection->parts[selection->parts_count - 1];
        selection->parts_count -= 1;
    }
}

void toggle_face_in_selection(Selection* selection, Face* face)
{
    if(face_selected(selection, face))
    {
        remove_face_from_selection(selection, face);
    }
    else
    {
        add_face_to_selection(selection, face);
    }
}

void move_faces(Mesh* mesh, Selection* selection, Vector3 translation)
{
    ASSERT(selection->type == Selection::Type::Face);

    for(int i = 0; i < selection->parts_count; i += 1)
    {
        Face* face = selection->parts[i].face;
        for(Border* border = face->first_border; border; border = border->next)
        {
            Link* first = border->first;
            Link* link = first;
            do
            {
                link->vertex->position += translation;
                link = link->next;
            } while(link != first);
        }
    }

    update_normals(mesh);
}

void flip_face_normals(Mesh* mesh, Selection* selection)
{
    ASSERT(selection->type == Selection::Type::Face);

    for(int i = 0; i < selection->parts_count; i += 1)
    {
        Face* face = selection->parts[i].face;
        flip_face_normal(face);
    }
}

static bool is_edge_on_selection_boundary(Selection* selection, Link* link)
{
    for(Link* fin = link->next_fin; fin != link; fin = fin->next_fin)
    {
        if(face_selected(selection, fin->face))
        {
            return false;
        }
    }
    return true;
}

void extrude(Mesh* mesh, Selection* selection, float distance, Heap* heap, Stack* stack)
{
    ASSERT(selection->type == Selection::Type::Face);

    // Calculate the vector to extrude all the vertices along.
    Vector3 average_direction = vector3_zero;
    for(int i = 0; i < selection->parts_count; i += 1)
    {
        Face* face = selection->parts[i].face;
        average_direction += face->normal;
    }
    Vector3 extrusion = distance * normalise(average_direction);

    // Use a map to redirect vertices to their extruded double when it's
    // already been added.
    Map map;
    map_create(&map, mesh->vertices_count, heap);

    for(int i = 0; i < selection->parts_count; i += 1)
    {
        Face* face = selection->parts[i].face;

        // @Incomplete: Holes in faces aren't yet supported!
        ASSERT(!face->first_border->next);

        Link* first = face->first_border->first;
        Link* link = first;
        do
        {
            if(is_edge_on_selection_boundary(selection, link))
            {
                // Add vertices only where they haven't been added already.
                Vertex* start = link->vertex;
                Vertex* end = link->next->vertex;
                if(!map_get(&map, start, nullptr))
                {
                    Vector3 position = start->position + extrusion;
                    Vertex* vertex = add_vertex(mesh, position);
                    add_edge(mesh, start, vertex);
                    map_add(&map, start, vertex, heap);
                }
                if(!map_get(&map, end, nullptr))
                {
                    Vector3 position = end->position + extrusion;
                    Vertex* vertex = add_vertex(mesh, position);
                    add_edge(mesh, end, vertex);
                    map_add(&map, end, vertex, heap);
                }

                // Add the extruded side face for this edge.
                Vertex* vertices[4];
                vertices[0] = start;
                vertices[1] = end;
                map_get(&map, end, reinterpret_cast<void**>(&vertices[2]));
                map_get(&map, start, reinterpret_cast<void**>(&vertices[3]));
                Edge* edges[4];
                edges[0] = link->edge;
                edges[1] = vertices[2]->any_edge;
                edges[2] = add_edge(mesh, vertices[2], vertices[3]);
                edges[3] = vertices[3]->any_edge;
                add_face(mesh, vertices, edges, 4);
            }
            link = link->next;
        } while(link != first);
    }

    for(int i = 0; i < selection->parts_count; i += 1)
    {
        Face* face = selection->parts[i].face;

        // @Incomplete: Holes in faces aren't yet supported!
        ASSERT(!face->first_border->next);

        const int vertices_count = face->edges;
        Vertex** vertices = STACK_ALLOCATE(stack, Vertex*, vertices_count);
        Link* link = face->first_border->first;
        for(int j = 0; j < vertices_count; j += 1)
        {
            map_get(&map, link->vertex, reinterpret_cast<void**>(&vertices[j]));
            link = link->next;
        }
        connect_disconnected_vertices_and_add_face(mesh, vertices, vertices_count, stack);
        remove_face_and_its_unlinked_edges_and_vertices(mesh, face);
        STACK_DEALLOCATE(stack, vertices);
    }

    map_destroy(&map, heap);

    update_normals(mesh);
}

void colour_just_the_one_face(Face* face, Vector3 colour)
{
    for(Border* border = face->first_border; border; border = border->next)
    {
        Link* first = border->first;
        Link* link = first;
        do
        {
            link->colour = colour;
            link = link->next;
        } while(link != first);
    }
}

void colour_all_faces(Mesh* mesh, Vector3 colour)
{
    FOR_EACH_IN_POOL(Link, link, mesh->link_pool)
    {
        link->colour = colour;
    }
}

void colour_selection(Mesh* mesh, Selection* selection, Vector3 colour)
{
    if(selection->type == Selection::Type::Face)
    {
        for(int i = 0; i < selection->parts_count; i += 1)
        {
            Face* face = selection->parts[i].face;
            colour_just_the_one_face(face, colour);
        }
    }
}

} // namespace jan
