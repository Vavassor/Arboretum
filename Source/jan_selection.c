#include "jan.h"

#include "array2.h"
#include "invalid_index.h"

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
