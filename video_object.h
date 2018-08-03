#ifndef VIDEO_OBJECT_H_
#define VIDEO_OBJECT_H_

#include "log.h"
#include "memory.h"
#include "vector_math.h"
#include "video.h"
#include "video_internal.h"

typedef struct VideoObject
{
    Matrix4 model;
    Matrix4 prior_model;
    Matrix4 normal;
    BufferId buffers[2];
    int vertices_count;
    int indices_count;
    VertexLayout vertex_layout;
} VideoObject;

void video_object_create(VideoObject* object, VertexLayout vertex_layout);
void video_object_destroy(VideoObject* object, Backend* backend);
void video_object_update_mesh(VideoObject* object, JanMesh* mesh, Backend* backend, Log* logger, Heap* heap);
void video_object_update_selection(VideoObject* object, JanMesh* mesh, JanSelection* selection, Backend* backend, Log* logger, Heap* heap);
void video_object_update_pointcloud(VideoObject* object, JanMesh* mesh, Backend* backend, Log* logger, Heap* heap);
void video_object_update_pointcloud_selection(VideoObject* object, JanMesh* mesh, JanSelection* selection, JanVertex* hovered, Backend* backend, Log* logger, Heap* heap);
void video_object_update_wireframe(VideoObject* object, JanMesh* mesh, Backend* backend, Log* logger, Heap* heap);
void video_object_update_wireframe_selection(VideoObject* object, JanMesh* mesh, JanSelection* selection, JanEdge* hovered, Backend* backend, Log* logger, Heap* heap);
void video_object_set_matrices(VideoObject* object, Matrix4 view, Matrix4 projection);
void video_object_generate_sky(VideoObject* object, Backend* backend, Log* logger, Stack* stack);
void video_object_set_model(VideoObject* object, Matrix4 model);
void video_object_store_past_model(VideoObject* object);

#endif // VIDEO_OBJECT_H_
