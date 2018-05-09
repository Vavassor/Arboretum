#include "jan.h"

#include "array2.h"
#include "assert.h"
#include "int_utilities.h"
#include "map.h"
#include "math_basics.h"

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

    return link;
}

Face* add_face(Mesh* mesh, Vertex** vertices, Edge** edges, int edges_count)
{
    Face* face = POOL_ALLOCATE(&mesh->face_pool, Face);
    face->edges = edges_count;

    // Create each link in the face and chain it to the previous.
    Link* first = add_border_to_face(mesh, vertices[0], edges[0], face);
    Link* prior = first;
    for(int i = 1; i < face->edges; ++i)
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

void make_wireframe(Mesh* mesh, Heap* heap, Vector4 colour, LineVertex** out_vertices, u16** out_indices)
{
    LineVertex* vertices = nullptr;
    u16* indices = nullptr;

    FOR_EACH_IN_POOL(Edge, edge, mesh->edge_pool)
    {
        Vertex* vertex = edge->vertices[0];
        Vertex* other = edge->vertices[1];

        Vector3 start = vertex->position;
        Vector3 end = other->position;
        Vector3 behind = end + (end - start);

        u32 colour_value = rgba_to_u32(colour);
        float left = -1.0f;
        float right = 1.0f;

        u16 base = ARRAY_COUNT(vertices);

        LineVertex v0 = {start, end, colour_value, left};
        LineVertex v1 = {start, end, colour_value, right};
        LineVertex v2 = {end, behind, colour_value, left};
        LineVertex v3 = {end, behind, colour_value, right};
        ARRAY_ADD(vertices, v0, heap);
        ARRAY_ADD(vertices, v1, heap);
        ARRAY_ADD(vertices, v2, heap);
        ARRAY_ADD(vertices, v3, heap);

        ARRAY_ADD(indices, base, heap);
        ARRAY_ADD(indices, base + 1, heap);
        ARRAY_ADD(indices, base + 2, heap);
        ARRAY_ADD(indices, base + 2, heap);
        ARRAY_ADD(indices, base + 1, heap);
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

static void triangulate_face(Face* face, Heap* heap, VertexPNC** vertices_array, u16** indices_array)
{
    VertexPNC* vertices = *vertices_array;
    u16* indices = *indices_array;

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

    // Save the index before adding any vertices for this face so it can be
    // used as a base for ear indexing.
    u16 base_index = ARRAY_COUNT(vertices);

    // @Incomplete: Holes in faces aren't yet supported!
    ASSERT(!face->first_border->next);

    // Copy all of the vertices in the face.
    ARRAY_RESERVE(vertices, face->edges, heap);
    Link* link = face->first_border->first;
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
    link = face->first_border->first;
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
