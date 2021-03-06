#ifndef VIDEO_H_
#define VIDEO_H_

#include "bmfont.h"
#include "camera.h"
#include "dense_map.h"
#include "int2.h"
#include "jan.h"
#include "log.h"
#include "memory.h"
#include "platform.h"
#include "tools.h"
#include "ui.h"

typedef enum VertexLayout
{
    VERTEX_LAYOUT_PC,
    VERTEX_LAYOUT_PNC,
    VERTEX_LAYOUT_POINT,
    VERTEX_LAYOUT_LINE,
} VertexLayout;

typedef struct VideoMeshUpdate
{
    JanMesh* mesh;
    JanSelection* selection;
    DenseMapId object;
} VideoMeshUpdate;

typedef struct VideoPointcloudUpdate
{
    JanVertex* hovered;
    JanMesh* mesh;
    JanSelection* selection;
    DenseMapId object;
} VideoPointcloudUpdate;

typedef struct VideoUpdate
{
    Int2 viewport;
    Camera* camera;
    MoveTool* move_tool;
    RotateTool* rotate_tool;
    UiContext* ui_context;
    UiItem* dialog_panel;
    UiItem* main_menu;
    UiItem* debug_readout;
    bool dialog_enabled;
    struct ObjectLady* lady;
    int hovered_object_index;
    int selected_object_index;
    DenseMapId selection_id;
    DenseMapId selection_pointcloud_id;
    DenseMapId selection_wireframe_id;
    DenseMapId hover_halo;
    DenseMapId selection_halo;
} VideoUpdate;

typedef struct VideoWireframeUpdate
{
    JanEdge* hovered;
    JanMesh* mesh;
    JanSelection* selection;
    DenseMapId object;
} VideoWireframeUpdate;

typedef struct VideoContext VideoContext;

VideoContext* video_create_context(Heap* heap, Platform* platform);
void video_destroy_context(VideoContext* context, Heap* heap);
void video_update_context(VideoContext* context, VideoUpdate* update, Platform* platform);
void video_resize_viewport(VideoContext* context, Int2 dimensions, double dots_per_millimeter, float fov);

DenseMapId video_add_object(VideoContext* context, VertexLayout vertex_layout);
void video_remove_object(VideoContext* context, DenseMapId id);
void video_set_up_font(VideoContext* context, BmfFont* font);

void video_set_model(VideoContext* context, DenseMapId id, Matrix4 model);
void video_update_mesh(VideoContext* context, VideoMeshUpdate* update);
void video_update_wireframe(VideoContext* context, VideoWireframeUpdate* update);
void video_update_pointcloud(VideoContext* context, VideoPointcloudUpdate* update);

int get_vertex_layout_size(VertexLayout vertex_layout);

#endif // VIDEO_H_
