#ifndef JAN_H_
#define JAN_H_

#include "memory.h"
#include "vector_math.h"
#include "vertex_layout.h"

// Editable Polygon Mesh
namespace jan {

struct Edge;
struct Link;
struct Face;

struct Vertex
{
    Vector3 position;
    Vector3 normal;
    Edge* any_edge;
};

// Spokes are for navigating edges that all meet at the same vertex "hub".
struct Spoke
{
    Edge* next;
    Edge* prior;
};

struct Edge
{
    Spoke spokes[2];
    Vertex* vertices[2];
    Link* any_link;
    bool sharp;
};

// Links are also known as half-edges. These have the additional responsibility
// of storing vertex attributes that are specific to a face.
struct Link
{
    Vector3 colour;
    Link* next;
    Link* prior;
    Link* next_fin;
    Link* prior_fin;
    Vertex* vertex;
    Edge* edge;
    Face* face;
};

// A Border corresponds to a boundary edge. So, either an outer edge of a face
// or an edge of a hole in a face.
struct Border
{
    Border* next;
    Border* prior;
    Link* first;
    Link* last;
};

struct Face
{
    Vector3 normal;
    Border* first_border;
    Border* last_border;
    int edges;
    int borders_count;
};

struct Mesh
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

struct Part
{
    union
    {
        Vertex* vertex;
        Edge* edge;
        Face* face;
    };
};

struct Selection
{
    enum class Type
    {
        Vertex,
        Edge,
        Face,
    };

    Heap* heap;
    Part* parts;
    int parts_count;
    Type type;
};

void create_mesh(Mesh* mesh);
void destroy_mesh(Mesh* mesh);
Vertex* add_vertex(Mesh* mesh, Vector3 position);
Edge* add_edge(Mesh* mesh, Vertex* start, Vertex* end);
Face* add_face(Mesh* mesh, Vertex** vertices, Edge** edges, int edges_count);
Face* connect_disconnected_vertices_and_add_face(Mesh* mesh, Vertex** vertices, int vertices_count, Stack* stack);
void remove_vertex(Mesh* mesh, Vertex* vertex);
void remove_edge(Mesh* mesh, Edge* edge);
void remove_face(Mesh* mesh, Face* face);
void remove_face_and_its_unlinked_edges_and_vertices(Mesh* mesh, Face* face);
void update_normals(Mesh* mesh);
void make_a_weird_face(Mesh* mesh, Stack* stack);
void make_a_face_with_holes(Mesh* mesh, Stack* stack);
void colour_just_the_one_face(Face* face, Vector3 colour);
void colour_all_faces(Mesh* mesh, Vector3 colour);
void colour_selection(Mesh* mesh, Selection* selection, Vector3 colour);
void move_faces(Mesh* mesh, Selection* selection, Vector3 translation);
void flip_face_normals(Mesh* mesh, Selection* selection);
void extrude(Mesh* mesh, Selection* selection, float distance, Heap* heap, Stack* stack);
void make_pointcloud(Mesh* mesh, Heap* heap, Vector4 colour, PointVertex** vertices, u16** indices);
void make_pointcloud_selection(Mesh* mesh, Vector4 colour, Vertex* hovered, Vector4 hover_colour, Heap* heap, PointVertex** vertices, u16** indices);
void make_wireframe(Mesh* mesh, Heap* heap, Vector4 colour, LineVertex** vertices, u16** indices);
void make_wireframe_selection(Mesh* mesh, Heap* heap, Vector4 colour, Edge* hovered, Vector4 hover_colour, LineVertex** vertices, u16** indices);
int count_border_edges(Border* border);
void triangulate(Mesh* mesh, Heap* heap, VertexPNC** vertices, u16** indices);
void triangulate_selection(Mesh* mesh, Selection* selection, Heap* heap, VertexPNC** vertices, u16** indices);

void create_selection(Selection* selection, Heap* heap);
void destroy_selection(Selection* selection);
Selection select_all(Mesh* mesh, Heap* heap);
void toggle_face_in_selection(Selection* selection, Face* face);

} // namespace jan

#endif // JAN_H_
