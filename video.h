#ifndef VIDEO_H_
#define VIDEO_H_

#include "camera.h"
#include "dense_map.h"
#include "int2.h"

struct Heap;
struct MoveTool;
struct ObjectLady;
struct Platform;

namespace bmfont {

struct Font;

} // namespace bmfont

namespace jan {

struct Mesh;
struct Selection;

} // namespace jan

namespace ui {

struct Context;
struct Item;

} // namespace ui

namespace video {

struct Object;

struct UpdateState
{
    Int2 viewport;
    Camera* camera;
    MoveTool* move_tool;
    ui::Context* ui_context;
    ui::Item* main_menu;
    ui::Item* dialog_panel;
    bool dialog_enabled;
    ObjectLady* lady;
    int hovered_object_index;
    int selected_object_index;
    DenseMapId selection_id;
    DenseMapId selection_wireframe_id;
};

enum class VertexLayout
{
    PNC,
    Line,
};

bool system_start_up();
void system_shut_down(bool functions_loaded);
void system_update(UpdateState* update, Platform* platform);
void resize_viewport(Int2 dimensions, double dots_per_millimeter, float fov);

DenseMapId add_object(VertexLayout vertex_layout);
void remove_object(DenseMapId id);
Object* get_object(DenseMapId id);
void set_up_font(bmfont::Font* font);

} // namespace video

#endif // VIDEO_H_
