#ifndef VIDEO_H_
#define VIDEO_H_

#include "camera.h"

struct Heap;
struct MoveTool;
struct ObjectLady;
struct Platform;

namespace bmfont {

struct Font;

} // namespace bmfont

namespace jan {

struct Mesh;

} // namespace jan

namespace ui {

struct Context;
struct Item;

} // namespace ui

namespace video {

struct Object;

struct UpdateState
{
    Viewport viewport;
    Camera* camera;
    MoveTool* move_tool;
    ui::Context* ui_context;
    ui::Item* main_menu;
    ui::Item* dialog_panel;
    bool dialog_enabled;
    ObjectLady* lady;
    int hovered_object_index;
    int selected_object_index;
};

bool system_start_up();
void system_shut_down(bool functions_loaded);
void system_update(UpdateState* update, Platform* platform);
void resize_viewport(int width, int height, double dots_per_millimeter, float fov);

void object_create(Object* object);
void object_destroy(Object* object);
void object_update_mesh(Object* object, jan::Mesh* mesh, Heap* heap);
void object_set_model(Object* object, Matrix4 model);

Object* add_object();
void remove_object(Object* object);
void set_up_font(bmfont::Font* font);

} // namespace video

#endif // VIDEO_H_
