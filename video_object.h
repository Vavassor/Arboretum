#ifndef VIDEO_OBJECT_H_
#define VIDEO_OBJECT_H_

#include "gl_core_3_3.h"
#include "memory.h"
#include "vector_math.h"
#include "video.h"

typedef struct VideoObject
{
    Matrix4 model;
    Matrix4 model_view_projection;
    Matrix4 normal;

    GLuint buffers[2];
    GLuint vertex_array;
    int indices_count;
    VertexLayout vertex_layout;
} VideoObject;

void video_object_create(VideoObject* object, VertexLayout vertex_layout);
void video_object_destroy(VideoObject* object);
void video_object_update_mesh(VideoObject* object, JanMesh* mesh, Heap* heap);
void video_object_update_selection(VideoObject* object, JanMesh* mesh, JanSelection* selection, Heap* heap);
void video_object_update_pointcloud(VideoObject* object, JanMesh* mesh, Heap* heap);
void video_object_update_pointcloud_selection(VideoObject* object, JanMesh* mesh, JanSelection* selection, JanVertex* hovered, Heap* heap);
void video_object_update_wireframe(VideoObject* object, JanMesh* mesh, Heap* heap);
void video_object_update_wireframe_selection(VideoObject* object, JanMesh* mesh, JanSelection* selection, JanEdge* hovered, Heap* heap);
void video_object_set_matrices(VideoObject* object, Matrix4 view, Matrix4 projection);
void video_object_generate_sky(VideoObject* object, Stack* stack);
void video_object_set_model(VideoObject* object, Matrix4 model);

#endif // VIDEO_OBJECT_H_
