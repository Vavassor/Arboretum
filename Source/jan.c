#include "jan.h"

#include "array2.h"
#include "assert.h"
#include "float_utilities.h"
#include "int_utilities.h"
#include "invalid_index.h"
#include "jan_internal.h"
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

static void add_spoke(JanEdge* edge, JanVertex* vertex)
{
    JanEdge* existing_edge = vertex->any_edge;
    if(existing_edge)
    {
        JanSpoke* a = jan_get_spoke(edge, vertex);
        JanSpoke* b = jan_get_spoke(existing_edge, vertex);
        if(b->prior)
        {
            JanSpoke* c = jan_get_spoke(b->prior, vertex);
            c->next = edge;
        }
        a->next = existing_edge;
        a->prior = b->prior;
        b->prior = edge;
    }
    else
    {
        vertex->any_edge = edge;
        JanSpoke* spoke = jan_get_spoke(edge, vertex);
        spoke->next = edge;
        spoke->prior = edge;
    }
}

static void remove_spoke(JanEdge* edge, JanVertex* vertex)
{
    JanSpoke* spoke = jan_get_spoke(edge, vertex);
    if(spoke->next)
    {
        JanSpoke* other = jan_get_spoke(spoke->next, vertex);
        other->prior = spoke->prior;
    }
    if(spoke->prior)
    {
        JanSpoke* other = jan_get_spoke(spoke->prior, vertex);
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
        edge = jan_get_spoke(edge, hub)->next;
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

void jan_add_and_link_border(JanMesh* mesh, JanFace* face, JanVertex** vertices, JanEdge** edges, int edges_count)
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

    jan_add_and_link_border(mesh, face, vertices, edges, edges_count);

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

    jan_add_and_link_border(mesh, face, vertices, edges, vertices_count);

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
                MapResult start_result = map_get(&map, start);
                if(!start_result.found)
                {
                    Float3 position = float3_add(start->position, extrusion);
                    JanVertex* vertex = jan_add_vertex(mesh, position);
                    jan_add_edge(mesh, start, vertex);
                    map_add(&map, start, vertex, heap);
                }
                JanVertex* end = link->next->vertex;
                MapResult end_result = map_get(&map, end);
                if(!end_result.found)
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
                MapResult result2 = map_get(&map, end);
                vertices[2] = (JanVertex*) result2.value;
                MapResult result3 = map_get(&map, start);
                vertices[3] = (JanVertex*) result3.value;
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
            MapResult result = map_get(&map, link->vertex);
            vertices[j] = (JanVertex*) result.value;
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
