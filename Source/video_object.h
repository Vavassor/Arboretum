#ifndef VIDEO_OBJECT_H_
#define VIDEO_OBJECT_H_

#include "log.h"
#include "memory.h"
#include "vector_math.h"
#include "video.h"
#include "video_internal.h"

typedef struct MeshUpdate
{
    JanMesh* mesh;
    JanSelection* selection;
    Backend* backend;
    Log* logger;
    Heap* heap;
} MeshUpdate;

typedef struct PointcloudUpdate
{
    JanMesh* mesh;
    JanSelection* selection;
    JanVertex* hovered;
    Backend* backend;
    Log* logger;
    Heap* heap;
} PointcloudUpdate;

typedef struct VideoObject
{
    Matrix4 model;
    Matrix4 normal;
    BufferId buffers[2];
    int vertices_count;
    int indices_count;
    VertexLayout vertex_layout;
} VideoObject;

typedef struct WireframeUpdate
{
    JanMesh* mesh;
    JanSelection* selection;
    JanEdge* hovered;
    Backend* backend;
    Log* logger;
    Heap* heap;
} WireframeUpdate;

void video_object_create(VideoObject* object, VertexLayout vertex_layout);
void video_object_destroy(VideoObject* object, Backend* backend);
void video_object_update_mesh(VideoObject* object, MeshUpdate* update);
void video_object_update_pointcloud(VideoObject* object, PointcloudUpdate* update);
void video_object_update_wireframe(VideoObject* object, WireframeUpdate* update);
void video_object_set_matrices(VideoObject* object, Matrix4 view, Matrix4 projection);
void video_object_generate_sky(VideoObject* object, Backend* backend, Log* logger, Stack* stack);
void video_object_set_model(VideoObject* object, Matrix4 model);

#endif // VIDEO_OBJECT_H_
