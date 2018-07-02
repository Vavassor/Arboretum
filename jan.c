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

void jan_create_mesh(JanMesh* mesh)
{
    pool_create(&mesh->face_pool, sizeof(JanFace), 1024);
    pool_create(&mesh->edge_pool, sizeof(JanEdge), 4096);
    pool_create(&mesh->vertex_pool, sizeof(JanVertex), 4096);
    pool_create(&mesh->link_pool, sizeof(JanLink), 8192);
    pool_create(&mesh->border_pool, sizeof(JanLink), 8192);

    mesh->faces_count = 0;
    mesh->edges_count = 0;
    mesh->vertices_count = 0;
}

void jan_destroy_mesh(JanMesh* mesh)
{
    pool_destroy(&mesh->face_pool);
    pool_destroy(&mesh->edge_pool);
    pool_destroy(&mesh->vertex_pool);
    pool_destroy(&mesh->link_pool);
    pool_destroy(&mesh->border_pool);
}

JanVertex* jan_add_vertex(JanMesh* mesh, Float3 position)
{
    JanVertex* vertex = POOL_ALLOCATE(&mesh->vertex_pool, JanVertex);
    vertex->position = position;

    mesh->vertices_count += 1;

    return vertex;
}

static JanSpoke* get_spoke(JanEdge* edge, JanVertex* vertex)
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

static void add_spoke(JanEdge* edge, JanVertex* vertex)
{
    JanEdge* existing_edge = vertex->any_edge;
    if(existing_edge)
    {
        JanSpoke* a = get_spoke(edge, vertex);
        JanSpoke* b = get_spoke(existing_edge, vertex);
        if(b->prior)
        {
            JanSpoke* c = get_spoke(b->prior, vertex);
            c->next = edge;
        }
        a->next = existing_edge;
        a->prior = b->prior;
        b->prior = edge;
    }
    else
    {
        vertex->any_edge = edge;
        JanSpoke* spoke = get_spoke(edge, vertex);
        spoke->next = edge;
        spoke->prior = edge;
    }
}

static void remove_spoke(JanEdge* edge, JanVertex* vertex)
{
    JanSpoke* spoke = get_spoke(edge, vertex);
    if(spoke->next)
    {
        JanSpoke* other = get_spoke(spoke->next, vertex);
        other->prior = spoke->prior;
    }
    if(spoke->prior)
    {
        JanSpoke* other = get_spoke(spoke->prior, vertex);
        other->next = spoke->next;
    }
    if(vertex->any_edge == edge)
    {
        if(spoke->next == edge)
        {
            vertex->any_edge = NULL;
        }
        else
        {
            vertex->any_edge = spoke->next;
        }
    }
    spoke->next = NULL;
    spoke->prior = NULL;
}

static bool edge_contains_vertices(JanEdge* edge, JanVertex* a, JanVertex* b)
{
    return (edge->vertices[0] == a && edge->vertices[1] == b)
        || (edge->vertices[1] == a && edge->vertices[0] == b);
}

static JanEdge* get_edge_spoked_from_vertex(JanVertex* hub, JanVertex* vertex)
{
    if(!hub->any_edge)
    {
        return NULL;
    }
    JanEdge* first = hub->any_edge;
    JanEdge* edge = first;
    do
    {
        if(edge_contains_vertices(edge, hub, vertex))
        {
            return edge;
        }
        edge = get_spoke(edge, hub)->next;
    } while(edge != first);
    return NULL;
}

JanEdge* jan_add_edge(JanMesh* mesh, JanVertex* start, JanVertex* end)
{
    JanEdge* edge = POOL_ALLOCATE(&mesh->edge_pool, JanEdge);
    edge->vertices[0] = start;
    edge->vertices[1] = end;

    add_spoke(edge, start);
    add_spoke(edge, end);

    mesh->edges_count += 1;

    return edge;
}

static JanEdge* add_edge_if_nonexistant(JanMesh* mesh, JanVertex* start, JanVertex* end)
{
    JanEdge* edge = get_edge_spoked_from_vertex(start, end);
    if(edge)
    {
        return edge;
    }
    else
    {
        return jan_add_edge(mesh, start, end);
    }
}

static JanLink* add_link(JanMesh* mesh, JanVertex* vertex, JanEdge* edge, JanFace* face)
{
    JanLink* link = POOL_ALLOCATE(&mesh->link_pool, JanLink);
    link->vertex = vertex;
    link->edge = edge;
    link->face = face;

    return link;
}

static bool is_boundary(JanLink* link)
{
    return link == link->next_fin;
}

static void make_boundary(JanLink* link)
{
    link->next_fin = link;
    link->prior_fin = link;
}

static void add_fin(JanLink* link, JanEdge* edge)
{
    JanLink* existing_link = edge->any_link;
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

static void remove_fin(JanLink* link, JanEdge* edge)
{
    if(is_boundary(link))
    {
        ASSERT(edge->any_link == link);
        edge->any_link = NULL;
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
    link->next_fin = NULL;
    link->prior_fin = NULL;
    link->edge = NULL;
}

static JanLink* add_border_to_face(JanMesh* mesh, JanVertex* vertex, JanEdge* edge, JanFace* face)
{
    JanLink* link = add_link(mesh, vertex, edge, face);
    add_fin(link, edge);

    JanBorder* border = POOL_ALLOCATE(&mesh->border_pool, JanBorder);
    border->first = link;
    border->last = link;
    border->next = NULL;
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

static void add_and_link_border(JanMesh* mesh, JanFace* face, JanVertex** vertices, JanEdge** edges, int edges_count)
{
    // Create each link in the face and chain it to the previous.
    JanLink* first = add_border_to_face(mesh, vertices[0], edges[0], face);
    JanLink* prior = first;
    for(int i = 1; i < edges_count; i += 1)
    {
        JanLink* link = add_link(mesh, vertices[i], edges[i], face);
        add_fin(link, edges[i]);

        prior->next = link;
        link->prior = prior;

        prior = link;
    }
    // Connect the ends to close the loop.
    first->prior = prior;
    prior->next = first;
}

JanFace* jan_add_face(JanMesh* mesh, JanVertex** vertices, JanEdge** edges, int edges_count)
{
    JanFace* face = POOL_ALLOCATE(&mesh->face_pool, JanFace);
    face->edges = edges_count;

    add_and_link_border(mesh, face, vertices, edges, edges_count);

    mesh->faces_count += 1;

    return face;
}

static JanFace* connect_vertices_and_add_face(JanMesh* mesh, JanVertex** vertices, int vertices_count, Stack* stack)
{
    JanEdge** edges = STACK_ALLOCATE(stack, JanEdge*, vertices_count);
    int end = vertices_count - 1;
    for(int i = 0; i < end; i += 1)
    {
        edges[i] = jan_add_edge(mesh, vertices[i], vertices[i + 1]);
    }
    edges[end] = jan_add_edge(mesh, vertices[end], vertices[0]);

    JanFace* face = jan_add_face(mesh, vertices, edges, vertices_count);
    STACK_DEALLOCATE(stack, edges);

    return face;
}

JanFace* jan_connect_disconnected_vertices_and_add_face(JanMesh* mesh, JanVertex** vertices, int vertices_count, Stack* stack)
{
    JanEdge** edges = STACK_ALLOCATE(stack, JanEdge*, vertices_count);
    int end = vertices_count - 1;
    for(int i = 0; i < end; i += 1)
    {
        edges[i] = add_edge_if_nonexistant(mesh, vertices[i], vertices[i + 1]);
    }
    edges[end] = add_edge_if_nonexistant(mesh, vertices[end], vertices[0]);

    JanFace* face = jan_add_face(mesh, vertices, edges, vertices_count);
    STACK_DEALLOCATE(stack, edges);

    return face;
}

static void connect_vertices_and_add_hole(JanMesh* mesh, JanFace* face, JanVertex** vertices, int vertices_count, Stack* stack)
{
    JanEdge** edges = STACK_ALLOCATE(stack, JanEdge*, vertices_count);
    int end = vertices_count - 1;
    for(int i = 0; i < end; i += 1)
    {
        edges[i] = jan_add_edge(mesh, vertices[i], vertices[i + 1]);
    }
    edges[end] = jan_add_edge(mesh, vertices[end], vertices[0]);

    add_and_link_border(mesh, face, vertices, edges, vertices_count);

    STACK_DEALLOCATE(stack, edges);
}

void jan_remove_face(JanMesh* mesh, JanFace* face)
{
    JanBorder* next_border;
    for(JanBorder* border = face->first_border; border; border = next_border)
    {
        next_border = border->next;
        JanLink* first = border->first;
        JanLink* link = first;
        do
        {
            JanLink* next = link->next;
            remove_fin(link, link->edge);
            pool_deallocate(&mesh->link_pool, link);
            link = next;
        } while(link != first);
        pool_deallocate(&mesh->border_pool, border);
    }
    pool_deallocate(&mesh->face_pool, face);
    mesh->faces_count -= 1;
}

void jan_remove_face_and_its_unlinked_edges_and_vertices(JanMesh* mesh, JanFace* face)
{
    JanBorder* next_border;
    for(JanBorder* border = face->first_border; border; border = next_border)
    {
        next_border = border->next;
        JanLink* first = border->first;
        JanLink* link = first;
        do
        {
            JanLink* next = link->next;
            JanEdge* edge = link->edge;
            remove_fin(link, edge);
            pool_deallocate(&mesh->link_pool, link);
            if(!edge->any_link)
            {
                JanVertex* vertices[2];
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

void jan_remove_edge(JanMesh* mesh, JanEdge* edge)
{
    while(edge->any_link)
    {
        jan_remove_face(mesh, edge->any_link->face);
    }
    remove_spoke(edge, edge->vertices[0]);
    remove_spoke(edge, edge->vertices[1]);
    pool_deallocate(&mesh->edge_pool, edge);
    mesh->edges_count -= 1;
}

void jan_remove_vertex(JanMesh* mesh, JanVertex* vertex)
{
    while(vertex->any_edge)
    {
        jan_remove_edge(mesh, vertex->any_edge);
    }
    pool_deallocate(&mesh->vertex_pool, vertex);
    mesh->vertices_count -= 1;
}

static void reverse_face_winding(JanFace* face)
{
    for(JanBorder* border = face->first_border; border; border = border->next)
    {
        JanLink* first = border->first;
        JanLink* link = first;
        JanLink* prior_next_fin = link->prior->next_fin;
        JanLink* prior_prior_fin = link->prior->prior_fin;
        bool boundary_prior = is_boundary(prior_next_fin);
        JanEdge* prior_edge = link->prior->edge;
        do
        {
            JanLink* next_fin = link->next_fin;
            JanLink* prior_fin = link->prior_fin;
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
            JanEdge* edge = link->edge;
            if(edge->any_link == link)
            {
                edge->any_link = link->next;
            }
            link->edge = prior_edge;
            prior_edge = edge;

            // Reverse the link itself.
            JanLink* temp = link->next;
            link->next = link->prior;
            link->prior = temp;
            link = temp;
        } while(link != first);
    }
}

static void flip_face_normal(JanFace* face)
{
    reverse_face_winding(face);
    face->normal = float3_negate(face->normal);
}

static void compute_vertex_normal(JanVertex* vertex)
{
    // This just averages them and does not take into account face area.
    JanEdge* edge = vertex->any_edge;
    if(edge)
    {
        JanEdge* first = edge;
        Float3 normal = float3_zero;
        do
        {
            JanVertex* other;
            JanSpoke spoke;
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
            normal = float3_add(normal, float3_subtract(vertex->position, other->position));
            edge = spoke.next;
        } while(edge != first);
        vertex->normal = float3_normalise(normal);
    }
}

static void compute_face_normal(JanFace* face)
{
    // This uses Newell's Method to compute the polygon normal.
    JanLink* link = face->first_border->first;
    JanLink* first = link;
    Float3 prior = link->prior->vertex->position;
    Float3 current = link->vertex->position;
    Float3 normal = float3_zero;
    do
    {
        normal.x += (prior.y - current.y) * (prior.z + current.z);
        normal.y += (prior.z - current.z) * (prior.x + current.x);
        normal.z += (prior.x - current.x) * (prior.y + current.y);
        prior = current;
        link = link->next;
        current = link->vertex->position;
    } while(link != first);
    face->normal = float3_normalise(normal);
}

void jan_update_normals(JanMesh* mesh)
{
    FOR_EACH_IN_POOL(JanFace, face, mesh->face_pool)
    {
        compute_face_normal(face);
    }
    FOR_EACH_IN_POOL(JanVertex, vertex, mesh->vertex_pool)
    {
        compute_vertex_normal(vertex);
    }
}

#define WEIRD_FACE_VERTICES_COUNT 8

void jan_make_a_weird_face(JanMesh* mesh, Stack* stack)
{
    Float3 positions[WEIRD_FACE_VERTICES_COUNT];
    positions[0] = (Float3){{-0.20842f, +0.20493f, 0.0f}};
    positions[1] = (Float3){{+0.53383f, -0.31467f, 0.0f}};
    positions[2] = (Float3){{+0.19402f, -0.55426f, 0.0f}};
    positions[3] = (Float3){{+0.86623f, -0.76310f, 0.0f}};
    positions[4] = (Float3){{+0.58252f, +0.83783f, 0.0f}};
    positions[5] = (Float3){{-0.58114f, +0.56986f, 0.0f}};
    positions[6] = (Float3){{-0.59335f, -0.28583f, 0.0f}};
    positions[7] = (Float3){{-0.05012f, -0.82722f, 0.0f}};

    JanVertex* vertices[WEIRD_FACE_VERTICES_COUNT];
    for(int i = 0; i < WEIRD_FACE_VERTICES_COUNT; i += 1)
    {
        vertices[i] = jan_add_vertex(mesh, positions[i]);
    }

    JanFace* face = connect_vertices_and_add_face(mesh, vertices, WEIRD_FACE_VERTICES_COUNT, stack);

    compute_face_normal(face);
}

void jan_make_a_face_with_holes(JanMesh* mesh, Stack* stack)
{
    Float3 positions[7] =
    {
        {{+1.016774f, -0.128711f, 0.0f}},
        {{+1.005646f, +1.246329f, 0.0f}},
        {{-0.160719f, -0.121287f, 0.0f}},
        {{-0.744234f, +1.375802f, 0.0f}},
        {{-2.254874f, +0.459116f, 0.0f}},
        {{-1.812329f, -0.432525f, 0.0f}},
        {{+0.000000f, -1.000000f, 0.0f}},
    };

    JanVertex* vertices[7];
    for(int i = 0; i < 7; i += 1)
    {
        vertices[i] = jan_add_vertex(mesh, positions[i]);
    }

    JanFace* face = connect_vertices_and_add_face(mesh, vertices, 7, stack);

    compute_face_normal(face);

    Float3 hole0_positions[5] =
    {
        {{-0.543713f, -0.318739f, 0.0f}},
        {{-0.716260f, -0.565462f, 0.0f}},
        {{-1.659353f, -0.253382f, 0.0f}},
        {{-1.602318f, +0.377146f, 0.0f}},
        {{-0.852411f, +0.512023f, 0.0f}},
    };

    for(int i = 0; i < 5; i += 1)
    {
        vertices[i] = jan_add_vertex(mesh, hole0_positions[i]);
    }

    connect_vertices_and_add_hole(mesh, face, vertices, 5, stack);

    Float3 hole1_positions[5] =
    {
        {{+0.502821f, +0.337892f, 0.0f}},
        {{+0.755197f, +0.412048f, 0.0f}},
        {{+0.717627f, +0.185694f, 0.0f}},
        {{+0.579880f, +0.063448f, 0.0f}},
        {{+0.361475f, +0.204754f, 0.0f}},
    };

    for(int i = 0; i < 5; i += 1)
    {
        vertices[i] = jan_add_vertex(mesh, hole1_positions[i]);
    }

    connect_vertices_and_add_hole(mesh, face, vertices, 5, stack);
}

static void add_to_pointcloud(JanVertex* vertex, Float4 colour, Heap* heap, PointVertex** out_vertices, uint16_t** out_indices)
{
    PointVertex* vertices = *out_vertices;
    uint16_t* indices = *out_indices;

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

    *out_vertices = vertices;
    *out_indices = indices;
}

void jan_make_pointcloud(JanMesh* mesh, Heap* heap, Float4 colour, PointVertex** vertices, uint16_t** indices)
{
    *vertices = NULL;
    *indices = NULL;

    FOR_EACH_IN_POOL(JanVertex, vertex, mesh->vertex_pool)
    {
        add_to_pointcloud(vertex, colour, heap, vertices, indices);
    }
}

void jan_make_pointcloud_selection(JanMesh* mesh, Float4 colour, JanVertex* hovered, Float4 hover_colour, JanSelection* selection, Float4 select_colour, Heap* heap, PointVertex** vertices, uint16_t** indices)
{
    *vertices = NULL;
    *indices = NULL;

    FOR_EACH_IN_POOL(JanVertex, vertex, mesh->vertex_pool)
    {
        if(vertex == hovered)
        {
            add_to_pointcloud(vertex, hover_colour, heap, vertices, indices);
        }
        else if(jan_vertex_selected(selection, vertex))
        {
            add_to_pointcloud(vertex, select_colour, heap, vertices, indices);
        }
        else
        {
            add_to_pointcloud(vertex, colour, heap, vertices, indices);
        }
    }
}

static void add_edge_to_wireframe(JanEdge* edge, Float4 colour, Heap* heap, LineVertex** out_vertices, uint16_t** out_indices)
{
    LineVertex* vertices = *out_vertices;
    uint16_t* indices = *out_indices;

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

    *out_vertices = vertices;
    *out_indices = indices;
}

void jan_make_wireframe(JanMesh* mesh, Heap* heap, Float4 colour, LineVertex** vertices, uint16_t** indices)
{
    *vertices = NULL;
    *indices = NULL;

    FOR_EACH_IN_POOL(JanEdge, edge, mesh->edge_pool)
    {
        add_edge_to_wireframe(edge, colour, heap, vertices, indices);
    }
}

void jan_make_wireframe_selection(JanMesh* mesh, Heap* heap, Float4 colour, JanEdge* hovered, Float4 hover_colour, JanSelection* selection, Float4 select_colour, LineVertex** vertices, uint16_t** indices)
{
    *vertices = NULL;
    *indices = NULL;

    FOR_EACH_IN_POOL(JanEdge, edge, mesh->edge_pool)
    {
        if(edge == hovered)
        {
            add_edge_to_wireframe(edge, hover_colour, heap, vertices, indices);
        }
        else if(jan_edge_selected(selection, edge))
        {
            add_edge_to_wireframe(edge, select_colour, heap, vertices, indices);
        }
        else
        {
            add_edge_to_wireframe(edge, colour, heap, vertices, indices);
        }
    }
}

static float signed_double_area(Float2 v0, Float2 v1, Float2 v2)
{
    return (v0.x - v2.x) * (v1.y - v2.y) - (v1.x - v2.x) * (v0.y - v2.y);
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

int jan_count_border_edges(JanBorder* border)
{
    int count = 0;
    JanLink* first = border->first;
    JanLink* link = first;
    do
    {
        count += 1;
        link = link->next;
    } while(link != first);
    return count;
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
    MOVE_ARRAY(&loop->positions[bridge_index + hole->edges + 2], &loop->positions[bridge_index], count);
    MOVE_ARRAY(&loop->vertices[bridge_index + hole->edges + 2], &loop->vertices[bridge_index], count);

    int to_end = hole->edges - hole->rightmost;
    MOVE_ARRAY(&loop->positions[bridge_index + 1], &hole->positions[hole->rightmost], to_end);
    MOVE_ARRAY(&loop->vertices[bridge_index + 1], &hole->vertices[hole->rightmost], to_end);

    MOVE_ARRAY(&loop->positions[bridge_index + to_end + 1], hole->positions, hole->rightmost + 1);
    MOVE_ARRAY(&loop->vertices[bridge_index + to_end + 1], hole->vertices, hole->rightmost + 1);

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

static void triangulate_face(JanFace* face, Heap* heap, VertexPNC** vertices_array, uint16_t** indices_array)
{
    VertexPNC* vertices = *vertices_array;
    uint16_t* indices = *indices_array;

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
            *vertices_array = vertices;
            *indices_array = indices;
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

    *vertices_array = vertices;
    *indices_array = indices;
}

void jan_triangulate(JanMesh* mesh, Heap* heap, VertexPNC** vertices, uint16_t** indices)
{
    *vertices = NULL;
    *indices = NULL;

    FOR_EACH_IN_POOL(JanFace, face, mesh->face_pool)
    {
        triangulate_face(face, heap, vertices, indices);
    }
}

void jan_triangulate_selection(JanMesh* mesh, JanSelection* selection, Heap* heap, VertexPNC** vertices, uint16_t** indices)
{
    ASSERT(selection->type == JAN_SELECTION_TYPE_FACE);

    *vertices = NULL;
    *indices = NULL;

    FOR_ALL(JanPart, selection->parts)
    {
        triangulate_face(it->face, heap, vertices, indices);
    }
}

void jan_create_selection(JanSelection* selection, Heap* heap)
{
    selection->heap = heap;
}

void jan_destroy_selection(JanSelection* selection)
{
    if(selection && selection->heap)
    {
        ARRAY_DESTROY(selection->parts, selection->heap);
    }
}

JanSelection jan_select_all(JanMesh* mesh, Heap* heap)
{
    JanSelection selection;
    jan_create_selection(&selection, heap);
    selection.type = JAN_SELECTION_TYPE_FACE;
    selection.parts = NULL;

    ARRAY_RESERVE(selection.parts, mesh->faces_count, heap);

    FOR_EACH_IN_POOL(JanFace, face, mesh->face_pool)
    {
        JanPart part;
        part.face = face;
        ARRAY_ADD(selection.parts, part, heap);
    }

    return selection;
}

bool jan_edge_selected(JanSelection* selection, JanEdge* edge)
{
    FOR_ALL(JanPart, selection->parts)
    {
        if(it->edge == edge)
        {
            return true;
        }
    }
    return false;
}

bool jan_face_selected(JanSelection* selection, JanFace* face)
{
    FOR_ALL(JanPart, selection->parts)
    {
        if(it->face == face)
        {
            return true;
        }
    }
    return false;
}

bool jan_vertex_selected(JanSelection* selection, JanVertex* vertex)
{
    FOR_ALL(JanPart, selection->parts)
    {
        if(it->vertex == vertex)
        {
            return true;
        }
    }
    return false;
}

static int find_edge(JanSelection* selection, JanEdge* edge)
{
    int found_index = invalid_index;
    for(int i = 0; i < array_count(selection->parts); i += 1)
    {
        if(selection->parts[i].edge == edge)
        {
            found_index = i;
            break;
        }
    }
    return found_index;
}

static int find_face(JanSelection* selection, JanFace* face)
{
    int found_index = invalid_index;
    for(int i = 0; i < array_count(selection->parts); i += 1)
    {
        if(selection->parts[i].face == face)
        {
            found_index = i;
            break;
        }
    }
    return found_index;
}

static int find_vertex(JanSelection* selection, JanVertex* vertex)
{
    int found_index = invalid_index;
    for(int i = 0; i < array_count(selection->parts); i += 1)
    {
        if(selection->parts[i].vertex == vertex)
        {
            found_index = i;
            break;
        }
    }
    return found_index;
}

void jan_toggle_edge_in_selection(JanSelection* selection, JanEdge* edge)
{
    int found_index = find_edge(selection, edge);
    if(is_valid_index(found_index))
    {
        ARRAY_REMOVE(selection->parts, &selection->parts[found_index]);
    }
    else
    {
        selection->type = JAN_SELECTION_TYPE_EDGE;
        JanPart part;
        part.edge = edge;
        ARRAY_ADD(selection->parts, part, selection->heap);
    }
}

void jan_toggle_face_in_selection(JanSelection* selection, JanFace* face)
{
    int found_index = find_face(selection, face);
    if(is_valid_index(found_index))
    {
        ARRAY_REMOVE(selection->parts, &selection->parts[found_index]);
    }
    else
    {
        selection->type = JAN_SELECTION_TYPE_FACE;
        JanPart part;
        part.face = face;
        ARRAY_ADD(selection->parts, part, selection->heap);
    }
}

void jan_toggle_vertex_in_selection(JanSelection* selection, JanVertex* vertex)
{
    int found_index = find_vertex(selection, vertex);
    if(is_valid_index(found_index))
    {
        ARRAY_REMOVE(selection->parts, &selection->parts[found_index]);
    }
    else
    {
        selection->type = JAN_SELECTION_TYPE_VERTEX;
        JanPart part;
        part.vertex = vertex;
        ARRAY_ADD(selection->parts, part, selection->heap);
    }
}

void jan_move_faces(JanMesh* mesh, JanSelection* selection, Float3 translation)
{
    ASSERT(selection->type == JAN_SELECTION_TYPE_FACE);

    FOR_ALL(JanPart, selection->parts)
    {
        JanFace* face = it->face;
        for(JanBorder* border = face->first_border; border; border = border->next)
        {
            JanLink* first = border->first;
            JanLink* link = first;
            do
            {
                link->vertex->position = float3_add(link->vertex->position, translation);
                link = link->next;
            } while(link != first);
        }
    }

    jan_update_normals(mesh);
}

void jan_flip_face_normals(JanMesh* mesh, JanSelection* selection)
{
    ASSERT(selection->type == JAN_SELECTION_TYPE_FACE);

    FOR_ALL(JanPart, selection->parts)
    {
        flip_face_normal(it->face);
    }
}

static bool is_edge_on_selection_boundary(JanSelection* selection, JanLink* link)
{
    for(JanLink* fin = link->next_fin; fin != link; fin = fin->next_fin)
    {
        if(jan_face_selected(selection, fin->face))
        {
            return false;
        }
    }
    return true;
}

void jan_extrude(JanMesh* mesh, JanSelection* selection, float distance, Heap* heap, Stack* stack)
{
    ASSERT(selection->type == JAN_SELECTION_TYPE_FACE);

    // Calculate the vector to extrude all the vertices along.
    Float3 average_direction = float3_zero;
    FOR_ALL(JanPart, selection->parts)
    {
        JanFace* face = it->face;
        average_direction = float3_add(average_direction, face->normal);
    }
    Float3 extrusion = float3_multiply(distance, float3_normalise(average_direction));

    // Use a map to redirect vertices to their extruded double when it's
    // already been added.
    Map map;
    map_create(&map, mesh->vertices_count, heap);

    FOR_ALL(JanPart, selection->parts)
    {
        JanFace* face = it->face;

        // @Incomplete: Holes in faces aren't yet supported!
        ASSERT(!face->first_border->next);

        JanLink* first = face->first_border->first;
        JanLink* link = first;
        do
        {
            if(is_edge_on_selection_boundary(selection, link))
            {
                // Add vertices only where they haven't been added already.
                JanVertex* start = link->vertex;
                JanVertex* end = link->next->vertex;
                if(!map_get(&map, start, NULL))
                {
                    Float3 position = float3_add(start->position, extrusion);
                    JanVertex* vertex = jan_add_vertex(mesh, position);
                    jan_add_edge(mesh, start, vertex);
                    map_add(&map, start, vertex, heap);
                }
                if(!map_get(&map, end, NULL))
                {
                    Float3 position = float3_add(end->position, extrusion);
                    JanVertex* vertex = jan_add_vertex(mesh, position);
                    jan_add_edge(mesh, end, vertex);
                    map_add(&map, end, vertex, heap);
                }

                // Add the extruded side face for this edge.
                JanVertex* vertices[4];
                vertices[0] = start;
                vertices[1] = end;
                map_get(&map, end, (void**) &vertices[2]);
                map_get(&map, start, (void**) &vertices[3]);
                JanEdge* edges[4];
                edges[0] = link->edge;
                edges[1] = vertices[2]->any_edge;
                edges[2] = jan_add_edge(mesh, vertices[2], vertices[3]);
                edges[3] = vertices[3]->any_edge;
                jan_add_face(mesh, vertices, edges, 4);
            }
            link = link->next;
        } while(link != first);
    }

    FOR_ALL(JanPart, selection->parts)
    {
        JanFace* face = it->face;

        // @Incomplete: Holes in faces aren't yet supported!
        ASSERT(!face->first_border->next);

        const int vertices_count = face->edges;
        JanVertex** vertices = STACK_ALLOCATE(stack, JanVertex*, vertices_count);
        JanLink* link = face->first_border->first;
        for(int j = 0; j < vertices_count; j += 1)
        {
            map_get(&map, link->vertex, (void**) &vertices[j]);
            link = link->next;
        }
        jan_connect_disconnected_vertices_and_add_face(mesh, vertices, vertices_count, stack);
        jan_remove_face_and_its_unlinked_edges_and_vertices(mesh, face);
        STACK_DEALLOCATE(stack, vertices);
    }

    map_destroy(&map, heap);

    jan_update_normals(mesh);
}

void jan_colour_just_the_one_face(JanFace* face, Float3 colour)
{
    for(JanBorder* border = face->first_border; border; border = border->next)
    {
        JanLink* first = border->first;
        JanLink* link = first;
        do
        {
            link->colour = colour;
            link = link->next;
        } while(link != first);
    }
}

void jan_colour_all_faces(JanMesh* mesh, Float3 colour)
{
    FOR_EACH_IN_POOL(JanLink, link, mesh->link_pool)
    {
        link->colour = colour;
    }
}

void jan_colour_selection(JanMesh* mesh, JanSelection* selection, Float3 colour)
{
    if(selection->type == JAN_SELECTION_TYPE_FACE)
    {
        FOR_ALL(JanPart, selection->parts)
        {
            jan_colour_just_the_one_face(it->face, colour);
        }
    }
}
