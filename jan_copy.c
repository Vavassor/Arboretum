#include "jan.h"

#include "assert.h"
#include "jan_internal.h"
#include "map.h"

static JanFace* copy_face_and_first_border(JanMesh* copy, JanFace* face, Map* vertex_map, Map* edge_map, Stack* stack)
{
    JanBorder* border = face->first_border;

    int count = jan_count_border_edges(border);
    JanVertex** vertices = STACK_ALLOCATE(stack, JanVertex*, count);
    JanEdge** edges = STACK_ALLOCATE(stack, JanEdge*, count);

    int i = 0;
    JanLink* first = border->first;
    JanLink* link = first;
    do
    {
        map_get(vertex_map, link->vertex, (void**) &vertices[i]);
        map_get(edge_map, link->edge, (void**) &edges[i]);
        i += 1;
        link = link->next;
    } while(link != first);

    JanFace* added = jan_add_face(copy, vertices, edges, count);

    STACK_DEALLOCATE(stack, edges);
    STACK_DEALLOCATE(stack, vertices);

    return added;
}

static void copy_border(JanMesh* copy, JanFace* added, JanBorder* border, Map* vertex_map, Map* edge_map, Stack* stack)
{
    int count = jan_count_border_edges(border);
    JanVertex** vertices = STACK_ALLOCATE(stack, JanVertex*, count);
    JanEdge** edges = STACK_ALLOCATE(stack, JanEdge*, count);

    int i = 0;
    JanLink* first = border->first;
    JanLink* link = first;
    do
    {
        map_get(vertex_map, link->vertex, (void**) &vertices[i]);
        map_get(edge_map, link->edge, (void**) &edges[i]);
        i += 1;
        link = link->next;
    } while(link != first);

    jan_add_and_link_border(copy, added, vertices, edges, count);

    STACK_DEALLOCATE(stack, edges);
    STACK_DEALLOCATE(stack, vertices);
}

static void copy_link(JanLink* link, JanLink* from)
{
    link->colour = from->colour;
}

static void copy_face_links(JanFace* added, JanFace* face)
{
    JanBorder* added_border = added->first_border;
    JanBorder* face_border = face->first_border;
    while(face_border)
    {
        JanLink* added_link = added_border->first;
        JanLink* face_first = face_border->first;
        JanLink* face_link = face_first;
        do
        {
            copy_link(added_link, face_link);
            added_link = added_link->next;
            face_link = face_link->next;
        } while(face_link != face_first);

        added_border = added_border->next;
        face_border = face_border->next;
    }
}

void jan_copy_mesh(JanMesh* copy, JanMesh* original, Heap* heap, Stack* stack)
{
    jan_create_mesh(copy);

    Map vertex_map;
    map_create(&vertex_map, original->vertices_count, heap);
    Map edge_map;
    map_create(&edge_map, original->edges_count, heap);

    FOR_EACH_IN_POOL(JanVertex, vertex, original->vertex_pool)
    {
        JanVertex* added = jan_add_vertex(copy, vertex->position);
        MAP_ADD(&vertex_map, vertex, added, heap);
    }

    FOR_EACH_IN_POOL(JanEdge, edge, original->edge_pool)
    {
        JanVertex* vertices[2];
        map_get(&vertex_map, edge->vertices[0], (void**) &vertices[0]);
        map_get(&vertex_map, edge->vertices[1], (void**) &vertices[1]);
        JanEdge* added = jan_add_edge(copy, vertices[0], vertices[1]);
        MAP_ADD(&edge_map, edge, added, heap);
    }

    FOR_EACH_IN_POOL(JanFace, face, original->face_pool)
    {
        JanFace* added = copy_face_and_first_border(copy, face, &vertex_map, &edge_map, stack);

        JanBorder* border = face->first_border;
        for(border = border->next; border; border = border->next)
        {
            copy_border(copy, added, border, &vertex_map, &edge_map, stack);
        }

        copy_face_links(added, face);
    }

    map_destroy(&vertex_map, heap);
    map_destroy(&edge_map, heap);
}
