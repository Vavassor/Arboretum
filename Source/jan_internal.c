#include "jan.h"
#include "jan_internal.h"

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

int jan_count_face_borders(JanFace* face)
{
    int count = 0;
    for(JanBorder* border = face->first_border; border; border = border->next)
    {
        count += 1;
    }
    return count;
}

bool jan_edge_contains_vertex(JanEdge* edge, JanVertex* vertex)
{
    return edge->vertices[0] == vertex
        || edge->vertices[1] == vertex;
}

JanSpoke* jan_get_spoke(JanEdge* edge, JanVertex* vertex)
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
