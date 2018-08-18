#include "jan_validate.h"

#include "jan_internal.h"

static bool test_vertex_not_in_its_edge(JanVertex* vertex, Log* logger)
{
    JanEdge* edge = vertex->any_edge;
    if(edge)
    {
        if(!jan_edge_contains_vertex(edge, vertex))
        {
            log_error(logger, "Vertex %p has an edge %p that doesn't contain it.", vertex, edge);
            return true;
        }
    }
    return false;
}

static int validate_vertices(JanMesh* mesh, Log* logger)
{
    int failures = 0;

    FOR_EACH_IN_POOL(JanVertex, vertex, mesh->vertex_pool)
    {
        failures += test_vertex_not_in_its_edge(vertex, logger);
    }

    return failures;
}

static bool test_edge_vertices_are_equal(JanEdge* edge, Log* logger)
{
    if(edge->vertices[0] == edge->vertices[1])
    {
        log_error(logger, "Both vertices of edge %p are the same.", edge);
        return true;
    }
    return false;
}

static bool test_edge_not_in_its_link(JanEdge* edge, Log* logger)
{
    JanLink* link = edge->any_link;
    if(link)
    {
        if(link->edge != edge)
        {
            log_error(logger, "Edge %p has a link %p that doesn't contain it.", edge, link);
            return true;
        }
    }
    return false;
}

static bool is_spoke_singular(JanEdge* edge, JanSpoke* spoke)
{
    return edge == spoke->next && edge == spoke->prior;
}

static bool test_spoke_and_link_inconsistent(JanEdge* edge, int index, Log* logger)
{
    JanSpoke* spoke = &edge->spokes[index];
    if(is_spoke_singular(edge, spoke) && edge->any_link)
    {
        log_error(logger, "Edge %p is part of a face, but has a spoke that's singular.", edge);
        return true;
    }
    return false;
}

static bool test_spoke_forward_disconnected(JanEdge* edge, int index, Log* logger)
{
    JanSpoke* spoke = &edge->spokes[index];
    if(!is_spoke_singular(edge, spoke))
    {
        JanVertex* hub = edge->vertices[index];
        JanSpoke* adjacent = jan_get_spoke(spoke->next, hub);
        if(adjacent->prior != edge)
        {
            log_error(logger, "A non-singular spoke in edge %p is disconnected from its next spoke.", edge);
            return true;
        }
    }
    return false;
}

static bool test_spoke_backward_disconnected(JanEdge* edge, int index, Log* logger)
{
    JanSpoke* spoke = &edge->spokes[index];
    if(!is_spoke_singular(edge, spoke))
    {
        JanVertex* hub = edge->vertices[index];
        JanSpoke* adjacent = jan_get_spoke(spoke->prior, hub);
        if(adjacent->next != edge)
        {
            log_error(logger, "A non-singular spoke in edge %p is disconnected from its prior spoke.", edge);
            return true;
        }
    }
    return false;
}

static int validate_edges(JanMesh* mesh, Log* logger)
{
    int failures = 0;

    FOR_EACH_IN_POOL(JanEdge, edge, mesh->edge_pool)
    {
        failures += test_edge_vertices_are_equal(edge, logger);
        failures += test_edge_not_in_its_link(edge, logger);

        for(int side = 0; side < 2; side += 1)
        {
            failures += test_spoke_and_link_inconsistent(edge, side, logger);
            failures += test_spoke_forward_disconnected(edge, side, logger);
            failures += test_spoke_backward_disconnected(edge, side, logger);
        }
    }

    return failures;
}

static bool test_edge_not_in_fin(JanLink* link, JanEdge* edge, Log* logger)
{
    if(link->edge != edge)
    {
        log_error(logger, "Edge %p has fin %p that doesn't contain it.", link, edge);
        return true;
    }
    return false;
}

static bool test_link_vertex_not_in_edge(JanLink* link, JanEdge* edge, Log* logger)
{
    if(!jan_edge_contains_vertex(edge, link->vertex))
    {
        log_error(logger, "Link %p has a vertex %p not in its edge %p.", link, link->vertex, edge);
        return true;
    }
    return false;
}

static bool test_forward_fin_disconnected(JanLink* link, Log* logger)
{
    if(link != link->next_fin->prior_fin)
    {
        log_error(logger, "Fin %p is disconnected from its next fin.", link);
        return true;
    }
    return false;
}

static bool test_backward_fin_disconnected(JanLink* link, Log* logger)
{
    if(link != link->prior_fin->next_fin)
    {
        log_error(logger, "Fin %p is disconnected from its prior fin.", link);
        return true;
    }
    return false;
}

static int validate_fins(JanMesh* mesh, Log* logger)
{
    int failures = 0;

    FOR_EACH_IN_POOL(JanEdge, edge, mesh->edge_pool)
    {
        JanLink* first = edge->any_link;
        JanLink* link = first;
        do
        {
            failures += test_edge_not_in_fin(link, edge, logger);
            failures += test_link_vertex_not_in_edge(link, edge, logger);
            failures += test_link_vertex_not_in_edge(link->next_fin, edge, logger);
            failures += test_forward_fin_disconnected(link, logger);
            failures += test_backward_fin_disconnected(link, logger);
            link = link->next_fin;
        } while(link != first);
    }

    return failures;
}

static bool test_edge_count_of_face_incorrect(JanFace* face, Log* logger)
{
    int count = jan_count_border_edges(face->first_border);
    if(face->edges != count)
    {
        log_error(logger, "Face %p has a different number of edges than it indicates.", face);
        return true;
    }
    return false;
}

static bool test_edge_count_of_face_invalid(JanFace* face, Log* logger)
{
    if(face->edges < 3)
    {
        log_error(logger, "Face %p has fewer than 3 edges.", face);
        return true;
    }
    return false;
}

static bool test_border_count_of_face_incorrect(JanFace* face, Log* logger)
{
    int count = jan_count_face_borders(face);
    if(face->borders_count != count)
    {
        log_error(logger, "Face %p has a different number of borders than it indicates.", face);
        return true;
    }
    return false;
}

static bool test_border_count_of_face_invalid(JanFace* face, Log* logger)
{
    if(face->borders_count < 1)
    {
        log_error(logger, "Face %p has no borders.", face);
        return true;
    }
    return false;
}

static bool test_face_not_in_link(JanLink* link, JanFace* face, Log* logger)
{
    if(link->face != face)
    {
        log_error(logger, "Face %p has link %p that has the wrong face %p.", face, link, link->face);
        return true;
    }
    return false;
}

static bool test_forward_link_disconnected(JanFace* face, JanLink* link, Log* logger)
{
    if(link != link->next->prior)
    {
        log_error(logger, "Face %p has link %p that's disconnected from the next link.", face, link);
        return true;
    }
    return false;
}

static bool test_backward_link_disconnected(JanFace* face, JanLink* link, Log* logger)
{
    if(link != link->prior->next)
    {
        log_error(logger, "Face %p has link %p that's disconnected from the prior link.", face, link);
        return true;
    }
    return false;
}

static int validate_faces(JanMesh* mesh, Log* logger)
{
    int failures = 0;

    FOR_EACH_IN_POOL(JanFace, face, mesh->face_pool)
    {
        failures += test_edge_count_of_face_incorrect(face, logger);
        failures += test_edge_count_of_face_invalid(face, logger);
        failures += test_border_count_of_face_incorrect(face, logger);
        failures += test_border_count_of_face_invalid(face, logger);

        for(JanBorder* border = face->first_border; border; border = border->next)
        {
            JanLink* first = border->first;
            JanLink* link = first;
            do
            {
                failures += test_face_not_in_link(link, face, logger);
                failures += test_forward_link_disconnected(face, link, logger);
                failures += test_backward_link_disconnected(face, link, logger);
                link = link->next;
            } while(link != first);
        }
    }

    return failures;
}

bool jan_validate_mesh(JanMesh* mesh, Log* logger)
{
    int failures = 0;

    failures += validate_vertices(mesh, logger);
    failures += validate_edges(mesh, logger);
    failures += validate_fins(mesh, logger);
    failures += validate_faces(mesh, logger);

    return failures == 0;
}
