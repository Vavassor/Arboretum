#ifndef VIDEO_OBJECT_H_
#define VIDEO_OBJECT_H_

#include "gl_core_3_3.h"
#include "vector_math.h"
#include "video.h"

struct Heap;
struct Stack;

namespace video {

struct Object
{
    Matrix4 model;
    Matrix4 model_view_projection;
    Matrix4 normal;

    GLuint buffers[2];
    GLuint vertex_array;
    int indices_count;
    VertexLayout vertex_layout;
};

void object_create(Object* object, VertexLayout vertex_layout);
void object_destroy(Object* object);
void object_update_mesh(Object* object, jan::Mesh* mesh, Heap* heap);
void object_update_selection(Object* object, jan::Mesh* mesh, jan::Selection* selection, Heap* heap);
void object_update_pointcloud(Object* object, jan::Mesh* mesh, Heap* heap);
void object_update_pointcloud_selection(Object* object, jan::Mesh* mesh, jan::Vertex* hovered, Heap* heap);
void object_update_wireframe(Object* object, jan::Mesh* mesh, Heap* heap);
void object_update_wireframe_selection(Object* object, jan::Mesh* mesh, jan::Edge* hovered, Heap* heap);
void object_set_matrices(Object* object, Matrix4 view, Matrix4 projection);
void object_generate_sky(Object* object, Stack* stack);
void object_set_model(Object* object, Matrix4 model);

} // namespace video

#endif // VIDEO_OBJECT_H_
