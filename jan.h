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
    Link* twin;
    Vertex* vertex;
    Edge* edge;
    Face* face;
};

struct Face
{
    Link* link;
    Vector3 normal;
    int edges;
};

struct Mesh
{
    Pool face_pool;
    Pool edge_pool;
    Pool vertex_pool;
    Pool link_pool;
    int faces_count;
    int edges_count;
    int vertices_count;
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
    void** parts;
    int parts_count;
    Type type;
};

void create_mesh(Mesh* mesh);
void destroy_mesh(Mesh* mesh);
Vertex* add_vertex(Mesh* mesh, Vector3 position);
Edge* add_edge(Mesh* mesh, Vertex* start, Vertex* end);
Face* add_face(Mesh* mesh, Vertex** vertices, Edge** edges, int edges_count);
Face* connect_disconnected_vertices_and_add_face(Mesh* mesh, Vertex** vertices, int vertices_count);
void update_normals(Mesh* mesh);
void make_a_weird_face(Mesh* mesh);
void colour_just_the_one_face(Face* face, Vector3 colour);
void colour_all_faces(Mesh* mesh, Vector3 colour);
void colour_selection(Mesh* mesh, Selection* selection, Vector3 colour);
void move_faces(Mesh* mesh, Selection* selection, Vector3 translation);
void extrude(Mesh* mesh, Selection* selection, float distance, Stack* stack);
void triangulate(Mesh* mesh, Heap* heap, VertexPNC** out_vertices, int* out_vertices_count, u16** out_indices, int* out_indices_count);

void create_selection(Selection* selection, Heap* heap);
void destroy_selection(Selection* selection);
Selection select_all(Mesh* mesh, Heap* heap);
void toggle_face_in_selection(Selection* selection, Face* face);

} // namespace jan

#endif // JAN_H_
