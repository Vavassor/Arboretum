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
} VideoUpdate;

typedef enum VertexLayout
{
    VERTEX_LAYOUT_PC,
    VERTEX_LAYOUT_PNC,
    VERTEX_LAYOUT_POINT,
    VERTEX_LAYOUT_LINE,
} VertexLayout;

typedef struct VideoContext VideoContext;

bool video_system_start_up();
void video_system_shut_down(bool functions_loaded);
void video_system_update(VideoUpdate* update, Platform* platform);
void video_resize_viewport(Int2 dimensions, double dots_per_millimeter, float fov);

DenseMapId video_add_object(VertexLayout vertex_layout);
void video_remove_object(DenseMapId id);
void video_set_up_font(BmfFont* font);
int get_vertex_layout_size(VertexLayout vertex_layout);

void video_set_model(DenseMapId id, Matrix4 model);
void video_update_mesh(DenseMapId id, JanMesh* mesh, Heap* heap);
void video_update_wireframe(DenseMapId id, JanMesh* mesh, Heap* heap);
void video_update_selection(DenseMapId id, JanMesh* mesh, JanSelection* selection, Heap* heap);
void video_update_pointcloud_selection(DenseMapId id, JanMesh* mesh, JanSelection* selection, JanVertex* hovered, Heap* heap);
void video_update_wireframe_selection(DenseMapId id, JanMesh* mesh, JanSelection* selection, JanEdge* hovered, Heap* heap);

#endif // VIDEO_H_
