#ifndef VIDEO_H_
#define VIDEO_H_

#include "bmfont.h"
#include "camera.h"
#include "dense_map.h"
#include "int2.h"
#include "jan.h"
#include "memory.h"
#include "platform.h"
#include "tools.h"
#include "ui.h"

typedef struct VideoUpdate
{
    Int2 viewport;
    Camera* camera;
    MoveTool* move_tool;
    RotateTool* rotate_tool;
    UiContext* ui_context;
    UiItem* main_menu;
    UiItem* dialog_panel;
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

typedef enum VertexLayout
{
    VERTEX_LAYOUT_PC,
    VERTEX_LAYOUT_PNC,
    VERTEX_LAYOUT_POINT,
    VERTEX_LAYOUT_LINE,
} VertexLayout;

typedef struct VideoContext VideoContext;

VideoContext* video_create_context(Heap* heap);
void video_destroy_context(VideoContext* context, Heap* heap, bool functions_loaded);
void video_update_context(VideoContext* context, VideoUpdate* update, Platform* platform);
void video_resize_viewport(Int2 dimensions, double dots_per_millimeter, float fov);

DenseMapId video_add_object(VideoContext* context, VertexLayout vertex_layout);
void video_remove_object(VideoContext* context, DenseMapId id);
void video_set_up_font(VideoContext* context, BmfFont* font);

void video_set_model(VideoContext* context, DenseMapId id, Matrix4 model);
void video_update_mesh(VideoContext* context, DenseMapId id, JanMesh* mesh, Heap* heap);
void video_update_wireframe(VideoContext* context, DenseMapId id, JanMesh* mesh, Heap* heap);
void video_update_selection(VideoContext* context, DenseMapId id, JanMesh* mesh, JanSelection* selection, Heap* heap);
void video_update_pointcloud_selection(VideoContext* context, DenseMapId id, JanMesh* mesh, JanSelection* selection, JanVertex* hovered, Heap* heap);
void video_update_wireframe_selection(VideoContext* context, DenseMapId id, JanMesh* mesh, JanSelection* selection, JanEdge* hovered, Heap* heap);

int get_vertex_layout_size(VertexLayout vertex_layout);

#endif // VIDEO_H_
