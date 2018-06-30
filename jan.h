#ifndef JAN_H_
#define JAN_H_

#if defined(__cplusplus)
extern "C" {
#endif

#include "memory.h"
#include "vector_math.h"
#include "vertex_layout.h"

// Editable Polygon Mesh

typedef struct JanBorder JanBorder;
typedef struct JanEdge JanEdge;
typedef struct JanFace JanFace;
typedef struct JanLink JanLink;
typedef struct JanMesh JanMesh;
typedef struct JanSpoke JanSpoke;
typedef struct JanVertex JanVertex;

struct JanVertex
{
    Float3 position;
    Float3 normal;
    JanEdge* any_edge;
};

// Spokes are for navigating edges that all meet at the same vertex "hub".

struct JanSpoke
{
    JanEdge* next;
    JanEdge* prior;
};

struct JanEdge
{
    JanSpoke spokes[2];
    JanVertex* vertices[2];
    JanLink* any_link;
    bool sharp;
};

// Links are also known as half-edges. These have the additional responsibility
// of storing vertex attributes that are specific to a face.
struct JanLink
{
    Float3 colour;
    JanLink* next;
    JanLink* prior;
    JanLink* next_fin;
    JanLink* prior_fin;
    JanVertex* vertex;
    JanEdge* edge;
    JanFace* face;
};

// A Border corresponds to a boundary edge. So, either an outer edge of a face
// or an edge of a hole in a face.
struct JanBorder
{
    JanBorder* next;
    JanBorder* prior;
    JanLink* first;
    JanLink* last;
};

struct JanFace
{
    Float3 normal;
    JanBorder* first_border;
    JanBorder* last_border;
    int edges;
    int borders_count;
};

struct JanMesh
{
    Pool face_pool;
    Pool edge_pool;
    Pool vertex_pool;
    Pool link_pool;
    Pool border_pool;
    int faces_count;
    int edges_count;
    int vertices_count;
};

typedef struct JanPart
{
    union
    {
        JanVertex* vertex;
        JanEdge* edge;
        JanFace* face;
    };
} JanPart;

typedef enum JanSelectionType
{
    JAN_SELECTION_TYPE_VERTEX,
    JAN_SELECTION_TYPE_EDGE,
    JAN_SELECTION_TYPE_FACE,
} JanSelectionType;

typedef struct JanSelection
{
    Heap* heap;
    JanPart* parts;
    JanSelectionType type;
} JanSelection;

void jan_create_mesh(JanMesh* mesh);
void jan_destroy_mesh(JanMesh* mesh);
JanVertex* jan_add_vertex(JanMesh* mesh, Float3 position);
JanEdge* jan_add_edge(JanMesh* mesh, JanVertex* start, JanVertex* end);
JanFace* jan_add_face(JanMesh* mesh, JanVertex** vertices, JanEdge** edges, int edges_count);
JanFace* jan_connect_disconnected_vertices_and_add_face(JanMesh* mesh, JanVertex** vertices, int vertices_count, Stack* stack);
void jan_remove_vertex(JanMesh* mesh, JanVertex* vertex);
void jan_remove_edge(JanMesh* mesh, JanEdge* edge);
void jan_remove_face(JanMesh* mesh, JanFace* face);
void jan_remove_face_and_its_unlinked_edges_and_vertices(JanMesh* mesh, JanFace* face);
void jan_update_normals(JanMesh* mesh);
void jan_make_a_weird_face(JanMesh* mesh, Stack* stack);
void jan_make_a_face_with_holes(JanMesh* mesh, Stack* stack);
void jan_colour_just_the_one_face(JanFace* face, Float3 colour);
void jan_colour_all_faces(JanMesh* mesh, Float3 colour);
void jan_colour_selection(JanMesh* mesh, JanSelection* selection, Float3 colour);
void jan_move_faces(JanMesh* mesh, JanSelection* selection, Float3 translation);
void jan_flip_face_normals(JanMesh* mesh, JanSelection* selection);
void jan_extrude(JanMesh* mesh, JanSelection* selection, float distance, Heap* heap, Stack* stack);
void jan_make_pointcloud(JanMesh* mesh, Heap* heap, Float4 colour, PointVertex** vertices, uint16_t** indices);
void jan_make_pointcloud_selection(JanMesh* mesh, Float4 colour, JanVertex* hovered, Float4 hover_colour, JanSelection* selection, Float4 select_colour, Heap* heap, PointVertex** vertices, uint16_t** indices);
void jan_make_wireframe(JanMesh* mesh, Heap* heap, Float4 colour, LineVertex** vertices, uint16_t** indices);
void jan_make_wireframe_selection(JanMesh* mesh, Heap* heap, Float4 colour, JanEdge* hovered, Float4 hover_colour, JanSelection* selection, Float4 select_colour, LineVertex** vertices, uint16_t** indices);
int jan_count_border_edges(JanBorder* border);
void jan_triangulate(JanMesh* mesh, Heap* heap, VertexPNC** vertices, uint16_t** indices);
void jan_triangulate_selection(JanMesh* mesh, JanSelection* selection, Heap* heap, VertexPNC** vertices, uint16_t** indices);

void jan_create_selection(JanSelection* selection, Heap* heap);
void jan_destroy_selection(JanSelection* selection);
JanSelection jan_select_all(JanMesh* mesh, Heap* heap);
void jan_toggle_edge_in_selection(JanSelection* selection, JanEdge* edge);
void jan_toggle_face_in_selection(JanSelection* selection, JanFace* face);
void jan_toggle_vertex_in_selection(JanSelection* selection, JanVertex* vertex);
bool jan_edge_selected(JanSelection* selection, JanEdge* edge);
bool jan_face_selected(JanSelection* selection, JanFace* face);
bool jan_vertex_selected(JanSelection* selection, JanVertex* vertex);

#if defined(__cplusplus)
} // extern "C"
#endif

#endif // JAN_H_
