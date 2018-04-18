#include "jan.h"

#include "array2.h"
#include "assert.h"
#include "int_utilities.h"
#include "math_basics.h"

namespace jan {

void create_mesh(Mesh* mesh)
{
    pool_create(&mesh->face_pool, sizeof(Face), 1024);
    pool_create(&mesh->edge_pool, sizeof(Edge), 4096);
    pool_create(&mesh->vertex_pool, sizeof(Vertex), 4096);
    pool_create(&mesh->link_pool, sizeof(Link), 8192);

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
    return
        (edge->vertices[0] == a && edge->vertices[1] == b) ||
        (edge->vertices[1] == a && edge->vertices[0] == b);
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

static bool is_boundary(Link* link)
{
    return link == link->twin;
}

static void make_boundary(Link* link)
{
    link->twin = link;
}

static void add_twin(Link* link, Edge* edge)
{
    Link* existing_link = edge->any_link;
    if(existing_link)
    {
        link->twin = edge->any_link;
        existing_link->twin = link;
    }
    else
    {
        make_boundary(link);
    }
    edge->any_link = link;
    link->edge = edge;
}

static void remove_twin(Link* link, Edge* edge)
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
            edge->any_link = link->twin;
        }
        make_boundary(link->twin);
    }
    link->twin = nullptr;
    link->edge = nullptr;
}

Face* add_face(Mesh* mesh, Vertex** vertices, Edge** edges, int edges_count)
{
    Face* face = POOL_ALLOCATE(&mesh->face_pool, Face);
    face->edges = edges_count;

    // Create each link in the face and chain it to the previous.
    Link* first = POOL_ALLOCATE(&mesh->link_pool, Link);
    first->vertex = vertices[0];
    first->face = face;
    add_twin(first, edges[0]);
    face->link = first;

    Link* prior = first;
    for(int i = 1; i < face->edges; ++i)
    {
        Link* link = POOL_ALLOCATE(&mesh->link_pool, Link);
        link->vertex = vertices[i];
        link->face = face;
        add_twin(link, edges[i]);

        prior->next = link;
        link->prior = prior;

        prior = link;
    }
    // Connect the ends to close the loop.
    first->prior = prior;
    prior->next = first;

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

static void remove_face(Mesh* mesh, Face* face)
{
    Link* link = face->link;
    Link* first = link;
    do
    {
        remove_twin(link, link->edge);
        Link* next = link->next;
        pool_deallocate(&mesh->link_pool, link);
        link = next;
    } while(link != first);
    pool_deallocate(&mesh->face_pool, face);
    mesh->faces_count -= 1;
}

static void remove_face_and_its_unlinked_edges_and_vertices(Mesh* mesh, Face* face)
{
    Link* first = face->link;
    Link* link = first;
    do
    {
        Edge* edge = link->edge;
        remove_twin(link, edge);
        Link* next = link->next;
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
    pool_deallocate(&mesh->face_pool, face);
    mesh->faces_count -= 1;
}

static void remove_edge(Mesh* mesh, Edge* edge)
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

static void remove_vertex(Mesh* mesh, Vertex* vertex)
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
    Link* first = face->link;
    Link* link = first;
    Link* prior_twin = link->prior->twin;
    bool boundary_prior = is_boundary(prior_twin);
    Edge* prior_edge = link->prior->edge;
    do
    {
        Link* twin = link->twin;
        bool boundary = is_boundary(twin);
        if(boundary_prior)
        {
            make_boundary(link);
        }
        else
        {
            link->twin = prior_twin;
            prior_twin->twin = link;
        }
        prior_twin = twin;
        boundary_prior = boundary;

        Edge* edge = link->edge;
        if(edge->any_link == link)
        {
            edge->any_link = link->next;
        }
        link->edge = prior_edge;
        prior_edge = edge;

        Link* temp = link->next;
        link->next = link->prior;
        link->prior = temp;
        link = temp;
    } while(link != first);
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
    Link* link = face->link;
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

void triangulate(Mesh* mesh, Heap* heap, VertexPNC** out_vertices, int* out_vertices_count, u16** out_indices, int* out_indices_count)
{
    VertexPNC* vertices = nullptr;
    u16* indices = nullptr;

    FOR_EACH_IN_POOL(Face, face, mesh->face_pool)
    {
        // The face is already a triangle.
        if(face->edges == 3)
        {
            ARRAY_RESERVE(vertices, 3, heap);
            ARRAY_RESERVE(indices, 3, heap);
            Link* link = face->link;
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
            continue;
        }

        // Save the index before adding any vertices for this face so it can be
        // used as a base for ear indexing.
        u16 base_index = ARRAY_COUNT(vertices);

        // Copy all of the vertices in the face.
        ARRAY_RESERVE(vertices, face->edges, heap);
        Link* link = face->link;
        for(int i = 0; i < face->edges; i += 1)
        {
            VertexPNC vertex;
            vertex.position = link->vertex->position;
            vertex.normal = face->normal;
            vertex.colour = rgb_to_u32(link->colour);
            ARRAY_ADD(vertices, vertex, heap);
            link = link->next;
        }

        // Project vertices onto a plane to produce 2D coordinates.
        const int max_vertices_per_face = 8;
        Vector2 projected[max_vertices_per_face];
        ASSERT(face->edges <= max_vertices_per_face);
        Matrix3 m = orthogonal_basis(face->normal);
        Matrix3 mi = transpose(m);
        link = face->link;
        for(int i = 0; i < face->edges; i += 1)
        {
            Vector3 p = link->vertex->position;
            projected[i] = mi * p;
            link = link->next;
        }

        // The projection may reverse the winding of the triangle. Reversing
        // the projected vertices would be the most obvious way to handle this
        // case. However, this would mean the projected and unprojected vertices
        // would no longer have the same index. So, reverse the order that the
        // chains are used to index the projected vertices later during
        // ear-finding.
        bool reverse_winding = false;
        if(!is_clockwise(projected, face->edges))
        {
            reverse_winding = true;
        }

        // Keep vertex chains to walk both ways around the polygon.
        int l[max_vertices_per_face];
        int r[max_vertices_per_face];
        for(int i = 0; i < face->edges; i += 1)
        {
            l[i] = mod(i - 1, face->edges);
            r[i] = mod(i + 1, face->edges);
        }

        // A polygon always has exactly n - 2 triangles, where n is the number
        // of edges in the polygon.
        ARRAY_RESERVE(indices, 3 * (face->edges - 2), heap);

        // Walk the right loop and find ears to triangulate using each of those
        // vertices.
        int j = face->edges - 1;
        int triangles_this_face = 0;
        while(triangles_this_face < face->edges - 2)
        {
            j = r[j];

            Vector2 v[3];
            if(reverse_winding)
            {
                v[0] = projected[r[j]];
                v[1] = projected[j];
                v[2] = projected[l[j]];
            }
            else
            {
                v[0] = projected[l[j]];
                v[1] = projected[j];
                v[2] = projected[r[j]];
            }

            if(is_clockwise(v[0], v[1], v[2]))
            {
                continue;
            }

            bool in_triangle = false;
            for(int k = 0; k < face->edges; k += 1)
            {
                Vector2 point = projected[k];
                if(k != l[j] && k != j && k != r[j] && point_in_triangle(v[0], v[1], v[2], point))
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
    }

    *out_vertices = vertices;
    *out_vertices_count = ARRAY_COUNT(vertices);
    *out_indices = indices;
    *out_indices_count = ARRAY_COUNT(indices);
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
        Face* selected = static_cast<Face*>(selection->parts[i]);
        if(selected == face)
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
    selection.parts = HEAP_ALLOCATE(selection.heap, void*, selection.parts_count);

    int i = 0;
    FOR_EACH_IN_POOL(Face, face, mesh->face_pool)
    {
        selection.parts[i] = face;
        i += 1;
    }

    return selection;
}

static void add_face_to_selection(Selection* selection, Face* face)
{
    selection->type = Selection::Type::Face;
    selection->parts = HEAP_REALLOCATE(selection->heap, selection->parts, selection->parts_count + 1);
    selection->parts[selection->parts_count] = face;
    selection->parts_count += 1;
}

static void remove_face_from_selection(Selection* selection, Face* face)
{
    int found_index = -1;
    for(int i = 0; i < selection->parts_count; i += 1)
    {
        if(static_cast<Face*>(selection->parts[i]) == face)
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
    for(int i = 0; i < selection->parts_count; i += 1)
    {
        Face* face = static_cast<Face*>(selection->parts[i]);
        Link* first = face->link;
        Link* link = first;
        do
        {
            link->vertex->position += translation;
            link = link->next;
        } while(link != first);
    }

    update_normals(mesh);
}

// This is just a fixed-size linearly-probed hash map.
struct VertexMap
{
    struct Pair
    {
        Vertex* key;
        Vertex* value;
    };
    Pair* pairs;
    int count;
    int cap;
};

static void map_create(VertexMap* map, int cap, Stack* stack)
{
    map->pairs = STACK_ALLOCATE(stack, VertexMap::Pair, cap);
    map->count = 0;
    map->cap = cap;
}

static void map_destroy(VertexMap* map, Stack* stack)
{
    STACK_DEALLOCATE(stack, map->pairs);
}

static upointer hash_pointer(void* pointer)
{
    upointer shift = log2(static_cast<double>(1 + sizeof(pointer)));
    return reinterpret_cast<upointer>(pointer) >> shift;
}

static Vertex* map_find(VertexMap* map, Vertex* key)
{
    int first = hash_pointer(key) % map->cap;
    int index = first;
    while(map->pairs[index].value && map->pairs[index].key != key)
    {
        index = (index + 1) % map->cap;
        if(index == first)
        {
            return nullptr;
        }
    }
    return map->pairs[index].value;
}

static void map_add(VertexMap* map, Vertex* key, Vertex* value)
{
    ASSERT(map->count < map->cap);
    int index = hash_pointer(key) % map->cap;
    while(map->pairs[index].value)
    {
        index = (index + 1) % map->cap;
    }
    map->pairs[index].key = key;
    map->pairs[index].value = value;
}

void extrude(Mesh* mesh, Selection* selection, float distance, Stack* stack)
{
    ASSERT(selection->type == Selection::Type::Face);

    // Calculate the vector to extrude all the vertices along.
    Vector3 average_direction = vector3_zero;
    for(int i = 0; i < selection->parts_count; i += 1)
    {
        Face* face = static_cast<Face*>(selection->parts[i]);
        average_direction += face->normal;
    }
    Vector3 extrusion = distance * normalise(average_direction);

    // Use a map to redirect vertices to their extruded double when it's
    // already been added.
    VertexMap map;
    map_create(&map, mesh->vertices_count, stack);

    for(int i = 0; i < selection->parts_count; i += 1)
    {
        Face* face = static_cast<Face*>(selection->parts[i]);
        Link* first = face->link;
        Link* link = first;
        do
        {
            if(is_boundary(link) || !face_selected(selection, link->twin->face))
            {
                // Add vertices only where they haven't been added already.
                Vertex* start = link->vertex;
                Vertex* end = link->next->vertex;
                if(!map_find(&map, start))
                {
                    Vector3 position = start->position + extrusion;
                    Vertex* vertex = add_vertex(mesh, position);
                    add_edge(mesh, start, vertex);
                    map_add(&map, start, vertex);
                }
                if(!map_find(&map, end))
                {
                    Vector3 position = end->position + extrusion;
                    Vertex* vertex = add_vertex(mesh, position);
                    add_edge(mesh, end, vertex);
                    map_add(&map, end, vertex);
                }

                // Add the extruded side face for this edge.
                Vertex* vertices[4];
                vertices[0] = start;
                vertices[1] = end;
                vertices[2] = map_find(&map, end);
                vertices[3] = map_find(&map, start);
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
        Face* face = static_cast<Face*>(selection->parts[i]);
        const int vertices_count = face->edges;
        Vertex** vertices = STACK_ALLOCATE(stack, Vertex*, vertices_count);
        Link* link = face->link;
        for(int j = 0; j < vertices_count; j += 1)
        {
            vertices[j] = map_find(&map, link->vertex);
            link = link->next;
        }
        connect_disconnected_vertices_and_add_face(mesh, vertices, vertices_count, stack);
        remove_face_and_its_unlinked_edges_and_vertices(mesh, face);
		STACK_DEALLOCATE(stack, vertices);
    }

    map_destroy(&map, stack);

    update_normals(mesh);
}

void colour_just_the_one_face(Face* face, Vector3 colour)
{
    Link* first = face->link;
    Link* link = first;
    do
    {
        link->colour = colour;
        link = link->next;
    } while(link != first);
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
            Face* face = static_cast<Face*>(selection->parts[i]);
            colour_just_the_one_face(face, colour);
        }
    }
}

} // namespace jan
