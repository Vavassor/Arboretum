#include "editor.h"

#include "array2.h"
#include "assert.h"
#include "asset_paths.h"
#include "camera.h"
#include "closest_point_of_approach.h"
#include "colours.h"
#include "dense_map.h"
#include "file_pick_dialog.h"
#include "float_utilities.h"
#include "history.h"
#include "input.h"
#include "int_utilities.h"
#include "logging.h"
#include "math_basics.h"
#include "tools.h"
#include "obj.h"
#include "object_lady.h"
#include "platform.h"
#include "string_build.h"
#include "string_utilities.h"
#include "ui.h"
#include "vector_math.h"
#include "video_object.h"

enum class Mode
{
    Object,
    Face,
    Edge,
    Vertex,
};

enum class Action
{
    None,
    Zoom_Camera,
    Orbit_Camera,
    Pan_Camera,
    Select,
    Move,
    Rotate,
    Scale,
};

namespace
{
    Heap heap;
    Stack scratch;

    ObjectLady lady;

    Mode mode;
    Action action_in_progress;
    bool translating;
    MoveTool move_tool;
    RotateTool rotate_tool;
    History history;

    int hovered_object_index;
    int selected_object_index;
    JanSelection selection;
    DenseMapId selection_id;
    DenseMapId selection_pointcloud_id;
    DenseMapId selection_wireframe_id;

    BmfFont font;

    ui::Context ui_context;
    ui::Item* main_menu;
    ui::Id import_button_id;
    ui::Id export_button_id;
    ui::Id object_mode_button_id;
    ui::Id vertex_mode_button_id;
    ui::Id edge_mode_button_id;
    ui::Id face_mode_button_id;

    struct
    {
        Float2 position;
        Float2 velocity;
        float scroll_velocity_y;
        MouseButton button;
        bool drag;
    } mouse;

    Camera camera;
    Int2 viewport;
    FilePickDialog dialog;
}

// Action.......................................................................

static bool action_allowed(Action action)
{
    return action_in_progress == Action::None || action_in_progress == action;
}

static void action_perform(Action action)
{
    action_in_progress = action;
}

static void action_stop(Action action)
{
    if(action_in_progress == action)
    {
        action_in_progress = Action::None;
    }
}

// Selection State..............................................................

static void select_mesh(int index)
{
    jan_destroy_selection(&selection);

    if(is_valid_index(index))
    {
        jan_create_selection(&selection, &heap);
        selection.type = JAN_SELECTION_TYPE_FACE;
    }

    selected_object_index = index;
}

void clear_object_from_hover_and_selection(ObjectId id, Platform* platform)
{
    Object* object = &lady.objects[selected_object_index];
    if(object->id == id)
    {
        select_mesh(invalid_index);
    }

    object = &lady.objects[hovered_object_index];
    if(object->id == id)
    {
        change_cursor(platform, CURSOR_TYPE_ARROW);
        hovered_object_index = invalid_index;
    }
}

// Whole System.................................................................

bool editor_start_up(Platform* platform)
{
    stack_create(&scratch, uptibytes(1));
    heap_create(&heap, uptibytes(1));

    hovered_object_index = invalid_index;
    selected_object_index = invalid_index;

    history_create(&history, &heap);
    create_object_lady(&lady, &heap);

    // Camera
    {
        camera.position = {-4.0f, -4.0f, 2.0f};
        camera.target = float3_zero;
        camera.field_of_view =  pi_over_2 * (2.0f / 3.0f);
        camera.near_plane = 0.001f;
        camera.far_plane = 100.0f;
    }

    // Dodecahedron
    {
        Object* dodecahedron = add_object(&lady, &heap);
        JanMesh* mesh = &dodecahedron->mesh;

        jan_make_a_weird_face(mesh, &scratch);

        JanSelection selection = jan_select_all(mesh, &heap);
        jan_extrude(mesh, &selection, 0.6f, &heap, &scratch);
        jan_destroy_selection(&selection);

        jan_colour_all_faces(mesh, float3_cyan);

        video::Object* video_object = video::get_object(dodecahedron->video_object);
        video::object_update_mesh(video_object, mesh, &heap);

        Float3 position = {2.0f, 0.0f, 0.0f};
        Quaternion orientation = quaternion_axis_angle(float3_unit_x, pi / 4.0f);
        dodecahedron->orientation = orientation;
        object_set_position(dodecahedron, position);

        add_object_to_history(&history, dodecahedron, &heap);
    }

    // Cheese
    {
        Object* cheese = add_object(&lady, &heap);
        JanMesh* mesh = &cheese->mesh;

        jan_make_a_face_with_holes(mesh, &scratch);

        video::Object* video_object = video::get_object(cheese->video_object);
        video::object_update_mesh(video_object, mesh, &heap);

        Float3 position = {0.0f, -2.0f, 0.0f};
        object_set_position(cheese, position);

        add_object_to_history(&history, cheese, &heap);
    }

    // Test Model
    {
        Object* test_model = add_object(&lady, &heap);
        JanMesh* mesh = &test_model->mesh;

        char* path = get_model_path_by_name("test.obj", &scratch);
        obj_load_file(path, mesh, &heap, &scratch);
        STACK_DEALLOCATE(&scratch, path);
        jan_colour_all_faces(mesh, float3_yellow);

        Float3 position = {-2.0f, 0.0f, 0.0f};
        object_set_position(test_model, position);

        video::Object* video_object = video::get_object(test_model->video_object);
        video::object_update_mesh(video_object, mesh, &heap);

        add_object_to_history(&history, test_model, &heap);
    }

    ui::create_context(&ui_context, &heap, &scratch);

    // Fonts
    {
        char* path = get_font_path_by_name("droid_12.fnt", &scratch);
        bmf_load_font(&font, path, &heap, &scratch);
        STACK_DEALLOCATE(&scratch, path);
        video::set_up_font(&font);
        ui_context.theme.font = &font;
    }

    // Setup the UI theme.
    {
        ui::Theme* theme = &ui_context.theme;

        theme->colours.button_cap_disabled = {0.318f, 0.318f, 0.318f, 1.0f};
        theme->colours.button_cap_enabled = {0.145f, 0.145f, 0.145f, 1.0f};
        theme->colours.button_cap_hovered_disabled = {0.382f, 0.386f, 0.418f, 1.0f};
        theme->colours.button_cap_hovered_enabled = {0.247f, 0.251f, 0.271f, 1.0f};
        theme->colours.button_label_disabled = {0.782f, 0.786f, 0.818f};
        theme->colours.button_label_enabled = float3_white;
        theme->colours.focus_indicator = {1.0f, 1.0f, 0.0f, 0.6f};
        theme->colours.list_item_background_hovered = {1.0f, 1.0f, 1.0f, 0.3f};
        theme->colours.list_item_background_selected = {1.0f, 1.0f, 1.0f, 0.5f};
        theme->colours.text_input_cursor = float4_white;
        theme->colours.text_input_selection = {1.0f, 1.0f, 1.0f, 0.4f};

        theme->styles[0].background = float4_black;
        theme->styles[1].background = float4_blue;
        theme->styles[2].background = {0.145f, 0.145f, 0.145f, 1.0f};
        theme->styles[3].background = float4_yellow;
    }

    // Setup the main menu.
    {
        main_menu = ui::create_toplevel_container(&ui_context, &heap);
        main_menu->type = ui::ItemType::Container;
        main_menu->growable = true;
        main_menu->container.style_type = ui::StyleType::Menu_Bar;
        main_menu->container.padding = {1.0f, 1.0f, 1.0f, 1.0f};
        main_menu->container.direction = ui::Direction::Left_To_Right;
        main_menu->container.alignment = ui::Alignment::Start;
        main_menu->container.justification = ui::Justification::Start;

        const int items_in_row = 6;
        ui::add_row(&main_menu->container, items_in_row, &ui_context, &heap);

        main_menu->container.items[0].type = ui::ItemType::Button;
        main_menu->container.items[0].button.enabled = true;
        main_menu->container.items[0].button.text_block.padding = {4.0f, 4.0f, 4.0f, 4.0f};
        ui::set_text(&main_menu->container.items[0].button.text_block, platform->localized_text.main_menu_import_file, &heap);
        import_button_id = main_menu->container.items[0].id;

        main_menu->container.items[1].type = ui::ItemType::Button;
        main_menu->container.items[1].button.enabled = true;
        main_menu->container.items[1].button.text_block.padding = {4.0f, 4.0f, 4.0f, 4.0f};
        ui::set_text(&main_menu->container.items[1].button.text_block, platform->localized_text.main_menu_export_file, &heap);
        export_button_id = main_menu->container.items[1].id;

        main_menu->container.items[2].type = ui::ItemType::Button;
        main_menu->container.items[2].button.enabled = true;
        main_menu->container.items[2].button.text_block.padding = {4.0f, 4.0f, 4.0f, 4.0f};
        ui::set_text(&main_menu->container.items[2].button.text_block, platform->localized_text.main_menu_enter_object_mode, &heap);
        object_mode_button_id = main_menu->container.items[2].id;

        main_menu->container.items[3].type = ui::ItemType::Button;
        main_menu->container.items[3].button.enabled = true;
        main_menu->container.items[3].button.text_block.padding = {4.0f, 4.0f, 4.0f, 4.0f};
        ui::set_text(&main_menu->container.items[3].button.text_block, platform->localized_text.main_menu_enter_face_mode, &heap);
        face_mode_button_id = main_menu->container.items[3].id;

        main_menu->container.items[4].type = ui::ItemType::Button;
        main_menu->container.items[4].button.enabled = true;
        main_menu->container.items[4].button.text_block.padding = {4.0f, 4.0f, 4.0f, 4.0f};
        ui::set_text(&main_menu->container.items[4].button.text_block, platform->localized_text.main_menu_enter_edge_mode, &heap);
        edge_mode_button_id = main_menu->container.items[4].id;

        main_menu->container.items[5].type = ui::ItemType::Button;
        main_menu->container.items[5].button.enabled = true;
        main_menu->container.items[5].button.text_block.padding = {4.0f, 4.0f, 4.0f, 4.0f};
        ui::set_text(&main_menu->container.items[5].button.text_block, platform->localized_text.main_menu_enter_vertex_mode, &heap);
        vertex_mode_button_id = main_menu->container.items[5].id;
    }

    // Move tool
    {
        move_tool.orientation = quaternion_identity;
        move_tool.position = float3_zero;
        move_tool.reference_position = float3_zero;
        move_tool.scale = 1.0f;
        move_tool.shaft_length = 2.0f * sqrtf(3.0f);
        move_tool.shaft_radius = 0.125f;
        move_tool.head_height = sqrtf(3.0f) / 2.0f;
        move_tool.head_radius = 0.5f;
        move_tool.plane_extent = 0.6f;
        move_tool.plane_thickness = 0.05f;
        move_tool.hovered_axis = invalid_index;
        move_tool.hovered_plane = invalid_index;
        move_tool.selected_axis = invalid_index;
        move_tool.selected_plane = invalid_index;
    }

    // Rotate tool
    {
        rotate_tool.position = float3_zero;
        rotate_tool.radius = 1.0f;
    }

    return true;
}

void editor_shut_down()
{
    destroy_object_lady(&lady, &heap);

    close_dialog(&dialog, &ui_context, &heap);

    jan_destroy_selection(&selection);

    bmf_destroy_font(&font, &heap);
    ui::destroy_context(&ui_context, &heap);

    history_destroy(&history, &heap);

    stack_destroy(&scratch);
    heap_destroy(&heap);
}

static void update_camera_controls()
{
    Float3 forward = float3_subtract(camera.position, camera.target);
    Float3 right = float3_normalise(float3_cross(float3_unit_z, forward));
    Matrix4 projection = matrix4_perspective_projection(camera.field_of_view, viewport.x, viewport.y, camera.near_plane, camera.far_plane);

    if(mouse.scroll_velocity_y != 0.0f && action_allowed(Action::Zoom_Camera))
    {
        Float3 moved = float3_add(float3_multiply(-mouse.scroll_velocity_y, forward), camera.position);
        Float3 forward_moved = float3_subtract(moved, camera.target);
        const float too_close = 0.1f;
        const float too_far = 40.0f;
        if(float3_dot(forward_moved, forward) < 0.0f)
        {
            // If this would move the camera past the target, instead place
            // it at a close threshold.
            camera.position = float3_add(float3_multiply(too_close, float3_normalise(forward)), camera.target);
        }
        else if(float3_length(forward_moved) > too_far)
        {
            camera.position = float3_add(float3_multiply(too_far, float3_normalise(forward)), camera.target);
        }
        else
        {
            camera.position = moved;
        }
        action_in_progress = Action::Zoom_Camera;
    }
    else
    {
        action_stop(Action::Zoom_Camera);
    }

    if(mouse.drag && (action_allowed(Action::Orbit_Camera) || action_allowed(Action::Pan_Camera)))
    {
        switch(mouse.button)
        {
            case MOUSE_BUTTON_LEFT:
            {
                const Float2 orbit_speed = {0.01f, 0.01f};
                Float2 angular_velocity = float2_pointwise_multiply(orbit_speed, float2_negate(mouse.velocity));
                Quaternion orbit_x = quaternion_axis_angle(float3_unit_z, angular_velocity.x);
                Quaternion orbit_y = quaternion_axis_angle(right, angular_velocity.y);
                Float3 turned = quaternion_rotate(orbit_y, forward);
                if(float3_dot(float3_cross(forward, float3_unit_z), float3_cross(turned, float3_unit_z)) < 0.0f)
                {
                    // Prevent from orbiting over the pole (Z axis), because
                    // it creates an issue where next frame it would orient
                    // the camera relative to the axis it had just passed
                    // and flip the camera view over. Furthermore, since
                    // you're usually still moving the mouse it'll move back
                    // toward the pole next frame, and flip again over and
                    // over every frame.
                    orbit_y = quaternion_identity;
                }
                Quaternion orbit = quaternion_multiply(orbit_y, orbit_x);
                camera.position = float3_add(quaternion_rotate(orbit, forward), camera.target);

                action_perform(Action::Orbit_Camera);
                break;
            }
            case MOUSE_BUTTON_RIGHT:
            {
                // Cast rays into world space corresponding to the mouse
                // position for this frame and the frame prior.
                Float2 prior_position = float2_add(mouse.position, mouse.velocity);
                Matrix4 view = matrix4_look_at(camera.position, camera.target, float3_unit_z);
                Ray prior = ray_from_viewport_point(prior_position, viewport, view, projection, false);
                Ray current = ray_from_viewport_point(mouse.position, viewport, view, projection, false);

                // Project the rays onto the plane containing the camera
                // target and facing the camera.
                Float3 n = float3_normalise(float3_subtract(camera.position, camera.target));
                float d0 = float3_dot(float3_subtract(camera.target, current.origin), n) / float3_dot(n, current.direction);
                float d1 = float3_dot(float3_subtract(camera.target, prior.origin), n) / float3_dot(n, prior.direction);
                Float3 p0 = float3_add(float3_multiply(d0, current.direction), current.origin);
                Float3 p1 = float3_add(float3_multiply(d1, prior.direction), prior.origin);

                // Pan by the amount moved across that plane.
                Float3 pan = float3_subtract(p0, p1);
                camera.position = float3_add(camera.position, pan);
                camera.target = float3_add(camera.target, pan);

                action_perform(Action::Pan_Camera);
                break;
            }
            default:
            {
                break;
            }
        }
    }
    else
    {
        action_stop(Action::Orbit_Camera);
        action_stop(Action::Pan_Camera);
    }
}

static void begin_move(Ray mouse_ray)
{
    ASSERT(is_valid_index(selected_object_index));
    Object object = lady.objects[selected_object_index];
    move_tool.reference_position = object.position;

    Float3 point = closest_ray_point(mouse_ray, object.position);
    move_tool.reference_offset = float3_subtract(object.position, point);
}

static void end_move()
{
    ASSERT(is_valid_index(selected_object_index));
    int index = selected_object_index;
    Object object = lady.objects[index];

    Change change;
    change.type = ChangeType::Move;
    change.move.object_id = object.id;
    change.move.position = object.position;
    history_add(&history, change);
}

static void move(Ray mouse_ray)
{
    ASSERT(is_valid_index(selected_object_index));
    ASSERT(is_valid_index(move_tool.selected_axis) || is_valid_index(move_tool.selected_plane));

    Object* object = &lady.objects[selected_object_index];

    Float3 point;
    if(is_valid_index(move_tool.selected_axis))
    {
        Float3 axis = float3_zero;
        axis.e[move_tool.selected_axis] = 1.0f;
        axis = quaternion_rotate(quaternion_conjugate(object->orientation), axis);

        Float3 line_point = float3_subtract(object->position, move_tool.reference_offset);
        point = closest_point_on_line(mouse_ray, line_point, float3_add(line_point, axis));
    }
    else if(is_valid_index(move_tool.selected_plane))
    {
        Float3 normal = float3_zero;
        normal.e[move_tool.selected_plane] = 1.0f;
        normal = float3_normalise(quaternion_rotate(quaternion_conjugate(object->orientation), normal));

        Float3 origin = float3_subtract(object->position, move_tool.reference_offset);
        Float3 intersection;
        bool intersected = intersect_ray_plane(mouse_ray, origin, normal, &intersection);
        if(intersected)
        {
            point = intersection;
        }
        else
        {
            point = origin;
        }
    }

    object_set_position(object, float3_add(point, move_tool.reference_offset));
}

static void delete_object(Platform* platform)
{
    Object* object = &lady.objects[selected_object_index];

    Change change;
    change.type = ChangeType::Delete_Object;
    change.delete_object.object_id = object->id;
    history_add(&history, change);

    clear_object_from_hover_and_selection(object->id, platform);

    store_object(&lady, object->id, &heap);
}

static void update_object_mode(Platform* platform)
{
    Matrix4 projection = matrix4_perspective_projection(camera.field_of_view, viewport.x, viewport.y, camera.near_plane, camera.far_plane);

    // Update the move tool.
    if(is_valid_index(selected_object_index) && action_allowed(Action::Move))
    {
        Float3 scale = float3_set_all(move_tool.scale);
        Matrix4 model = matrix4_compose_transform(move_tool.position, move_tool.orientation, scale);
        Matrix4 view = matrix4_look_at(camera.position, camera.target, float3_unit_z);
        Ray ray = ray_from_viewport_point(mouse.position, viewport, view, projection, false);

        int hovered_axis = invalid_index;
        float closest = infinity;
        for(int i = 0; i < 3; i += 1)
        {
            Float3 shaft_axis = float3_zero;
            shaft_axis.e[i] = move_tool.shaft_length;
            Float3 head_axis = float3_zero;
            head_axis.e[i] = move_tool.head_height;

            Cylinder cylinder;
            cylinder.start = matrix4_transform_point(model, float3_zero);
            cylinder.end = matrix4_transform_point(model, shaft_axis);
            cylinder.radius = move_tool.scale * move_tool.shaft_radius;

            Float3 intersection;
            bool intersected = intersect_ray_cylinder(ray, cylinder, &intersection);
            float distance = float3_squared_distance(ray.origin, intersection);
            if(intersected && distance < closest)
            {
                closest = distance;
                hovered_axis = i;
            }

            Cone cone;
            cone.base_center = matrix4_transform_point(model, shaft_axis);
            cone.axis = float3_multiply(move_tool.scale, head_axis);
            cone.radius = move_tool.scale * move_tool.head_radius;

            intersected = intersect_ray_cone(ray, cone, &intersection);
            distance = float3_squared_distance(ray.origin, intersection);
            if(intersected && distance < closest)
            {
                closest = distance;
                hovered_axis = i;
            }
        }
        move_tool.hovered_axis = hovered_axis;

        int hovered_plane = invalid_index;
        for(int i = 0; i < 3; i += 1)
        {
            Float3 center = float3_set_all(move_tool.shaft_length);
            center.e[i] = 0.0f;

            Float3 extents = float3_set_all(move_tool.scale * move_tool.plane_extent);
            extents.e[i] = move_tool.scale * move_tool.plane_thickness;

            Box box;
            box.center = matrix4_transform_point(model, center);
            box.extents = extents;
            box.orientation = move_tool.orientation;

            Float3 intersection;
            bool intersected = intersect_ray_box(ray, box, &intersection);
            if(intersected)
            {
                float distance = float3_squared_distance(ray.origin, intersection);
                if(distance < closest)
                {
                    closest = distance;
                    hovered_plane = i;
                }
            }
        }
        move_tool.hovered_plane = hovered_plane;
        if(is_valid_index(hovered_plane) || is_valid_index(move_tool.selected_plane))
        {
            move_tool.hovered_axis = invalid_index;
        }

        if(input_get_mouse_clicked(MOUSE_BUTTON_LEFT))
        {
            if(is_valid_index(hovered_axis))
            {
                move_tool.selected_axis = hovered_axis;
                begin_move(ray);
                action_perform(Action::Move);
            }
            else if(is_valid_index(hovered_plane))
            {
                move_tool.selected_plane = hovered_plane;
                begin_move(ray);
                action_perform(Action::Move);
            }
        }
        if(mouse.drag && mouse.button == MOUSE_BUTTON_LEFT && (is_valid_index(move_tool.selected_axis) || is_valid_index(move_tool.selected_plane)))
        {
            move(ray);
        }
        else
        {
            if(action_in_progress == Action::Move)
            {
                end_move();
            }

            move_tool.selected_axis = invalid_index;
            move_tool.selected_plane = invalid_index;
            action_stop(Action::Move);
        }
    }

    if(action_allowed(Action::Select))
    {
        // Update pointer hover detection.
        // Casually raycast against every triangle in the scene.
        float closest = infinity;
        hovered_object_index = invalid_index;
        for(int i = 0; i < array_count(lady.objects); i += 1)
        {
            Object* object = &lady.objects[i];
            JanMesh* mesh = &object->mesh;

            Matrix4 model = matrix4_compose_transform(object->position, object->orientation, float3_one);
            Matrix4 view = matrix4_look_at(camera.position, camera.target, float3_unit_z);
            Ray ray = ray_from_viewport_point(mouse.position, viewport, view, projection, false);
            ray = transform_ray(ray, matrix4_inverse_transform(model));
            float distance;
            JanFace* face = jan_first_face_hit_by_ray(mesh, ray, &distance, &scratch);
            if(face && distance < closest)
            {
                closest = distance;
                hovered_object_index = i;
            }
        }

        // Update the mouse cursor based on the hover status.
        if(is_valid_index(hovered_object_index))
        {
            change_cursor(platform, CURSOR_TYPE_HAND_POINTING);
        }

        // Detect selection.
        if(input_get_mouse_clicked(MOUSE_BUTTON_LEFT) && is_valid_index(hovered_object_index))
        {
            if(selected_object_index == hovered_object_index)
            {
                select_mesh(invalid_index);
            }
            else
            {
                select_mesh(hovered_object_index);
            }
            action_perform(Action::Select);
        }
        else
        {
            action_stop(Action::Select);
        }
    }

    // Center the move tool in the selected object, if needed.
    if(is_valid_index(selected_object_index))
    {
        Object object = lady.objects[selected_object_index];
        Float3 position = object.position;
        Float3 normal = float3_normalise(float3_subtract(camera.target, camera.position));
        float scale = 0.05f * distance_point_plane(position, camera.position, normal);
        move_tool.position = object.position;
        move_tool.orientation = object.orientation;
        move_tool.scale = scale;
    }

    // Update the rotate tool.
    {
        Float3 center = {0.0f, -5.0f, 0.0f};
        Float3 normal = float3_normalise(float3_subtract(camera.target, camera.position));
        float radius = 0.2f * distance_point_plane(center, camera.position, normal);

        Disk disks[3] =
        {
            {center, float3_unit_x, radius},
            {center, float3_unit_y, radius},
            {center, float3_unit_z, radius},
        };

        Float3 camera_forward = float3_subtract(camera.target, camera.position);
        Float3 axes[3] =
        {
            float3_subtract(closest_disk_plane(disks[0], camera.position, camera_forward), disks[0].center),
            float3_subtract(closest_disk_plane(disks[1], camera.position, camera_forward), disks[1].center),
            float3_subtract(closest_disk_plane(disks[2], camera.position, camera_forward), disks[2].center),
        };

        float angles[3] =
        {
            float3_angle_between(axes[0], float3_negate(float3_unit_z)),
            float3_angle_between(axes[1], float3_negate(float3_unit_z)),
            float3_angle_between(axes[2], float3_negate(float3_unit_y)),
        };

        if(axes[0].y < 0.0f)
        {
            angles[0] = tau - angles[0];
        }
        if(axes[1].x > 0.0f)
        {
            angles[1] = tau - angles[1];
        }
        if(axes[2].x < 0.0f)
        {
            angles[2] = tau - angles[2];
        }

        rotate_tool.position = center;
        rotate_tool.radius = radius;
        rotate_tool.angles[0] = angles[0];
        rotate_tool.angles[1] = angles[1];
        rotate_tool.angles[2] = angles[2];
    }

    update_camera_controls();

    if(action_in_progress == Action::None)
    {
        if(input_get_hotkey_tapped(INPUT_FUNCTION_UNDO))
        {
            undo(&history, &lady, &heap, platform);
        }
        if(input_get_hotkey_tapped(INPUT_FUNCTION_REDO))
        {
            redo(&history, &lady, &heap);
        }
        if(is_valid_index(selected_object_index) && input_get_hotkey_tapped(INPUT_FUNCTION_DELETE))
        {
            delete_object(platform);
        }
    }
}

static void enter_edge_mode()
{
    selection_wireframe_id = video::add_object(video::VertexLayout::Line);

    Object* object = &lady.objects[selected_object_index];
    Matrix4 model = matrix4_compose_transform(object->position, object->orientation, float3_one);

    video::Object* video_object = video::get_object(selection_wireframe_id);
    video::object_set_model(video_object, model);
}

static void exit_edge_mode()
{
    jan_destroy_selection(&selection);

    video::remove_object(selection_wireframe_id);
    selection_wireframe_id = 0;
}

static void update_edge_mode()
{
    ASSERT(is_valid_index(selected_object_index));
    ASSERT(selected_object_index >= 0 && selected_object_index < array_count(lady.objects));

    const float touch_radius = 30.0f;

    Object* object = &lady.objects[selected_object_index];
    JanMesh* mesh = &object->mesh;

    update_camera_controls();

    Matrix4 model = matrix4_compose_transform(object->position, object->orientation, float3_one);
    Matrix4 view = matrix4_look_at(camera.position, camera.target, float3_unit_z);
    Matrix4 projection = matrix4_perspective_projection(camera.field_of_view, viewport.x, viewport.y, camera.near_plane, camera.far_plane);
    Matrix4 model_view_projection = matrix4_multiply(projection, matrix4_multiply(view, model));
    Matrix4 inverse_model = matrix4_inverse_transform(model);
    Matrix4 inverse = matrix4_multiply(matrix4_inverse_view(view), matrix4_inverse_perspective(projection));

    Ray ray = ray_from_viewport_point(mouse.position, viewport, view, projection, false);
    ray = transform_ray(ray, inverse_model);

    float distance_to_edge;
    JanEdge* edge = jan_first_edge_under_point(mesh, mouse.position, touch_radius, model_view_projection, inverse, viewport, ray.origin, ray.direction, &distance_to_edge);
    if(edge)
    {
        float distance_to_face = infinity;
        jan_first_face_hit_by_ray(mesh, ray, &distance_to_face, &scratch);

        if(distance_to_edge < distance_to_face && input_get_mouse_clicked(MOUSE_BUTTON_LEFT))
        {
            jan_toggle_edge_in_selection(&selection, edge);
        }
    }

    video::Object* video_object = video::get_object(selection_wireframe_id);
    video::object_update_wireframe_selection(video_object, mesh, &selection, edge, &heap);
}

static void enter_face_mode()
{
    selection_id = video::add_object(video::VertexLayout::PNC);
    selection_wireframe_id = video::add_object(video::VertexLayout::Line);

    // Set the selection's transform to match the selected mesh.
    Object* object = &lady.objects[selected_object_index];
    Matrix4 model = matrix4_compose_transform(object->position, object->orientation, float3_one);

    video::Object* video_object = video::get_object(selection_id);
    video::object_set_model(video_object, model);

    video_object = video::get_object(selection_wireframe_id);
    video::object_set_model(video_object, model);
}

static void exit_face_mode()
{
    jan_destroy_selection(&selection);

    video::remove_object(selection_id);
    video::remove_object(selection_wireframe_id);
    selection_id = 0;
    selection_wireframe_id = 0;
}

static void update_face_mode()
{
    ASSERT(selected_object_index != invalid_index);
    ASSERT(selected_object_index >= 0 && selected_object_index < array_count(lady.objects));

    Object* object = &lady.objects[selected_object_index];
    JanMesh* mesh = &object->mesh;
    Matrix4 projection = matrix4_perspective_projection(camera.field_of_view, viewport.x, viewport.y, camera.near_plane, camera.far_plane);

    if(input_get_key_tapped(INPUT_KEY_G))
    {
        translating = !translating;
    }

    if(!translating)
    {
        update_camera_controls();
    }

    // Translation of faces.
    if(translating)
    {
        Int2 velocity = input_get_mouse_velocity();
        mouse.velocity.x = velocity.x;
        mouse.velocity.y = velocity.y;

        Float3 forward = float3_subtract(camera.position, camera.target);
        Float3 right = float3_normalise(float3_cross(float3_unit_z, forward));
        Float3 up = float3_normalise(float3_cross(right, forward));

        const Float2 move_speed = {0.007f, 0.007f};
        Float2 move_velocity = float2_pointwise_multiply(move_speed, mouse.velocity);
        Float3 move = float3_add(float3_multiply(move_velocity.x, right), float3_multiply(move_velocity.y, up));
        jan_move_faces(mesh, &selection, move);

        video::Object* video_object = video::get_object(object->video_object);
        video::object_update_mesh(video_object, mesh, &heap);
    }

    // Cast a ray from the mouse to the test model.
    {
        Matrix4 model = matrix4_compose_transform(object->position, object->orientation, float3_one);
        Matrix4 view = matrix4_look_at(camera.position, camera.target, float3_unit_z);
        Ray ray = ray_from_viewport_point(mouse.position, viewport, view, projection, false);
        ray = transform_ray(ray, matrix4_inverse_transform(model));
        JanFace* face = jan_first_face_hit_by_ray(mesh, ray, nullptr, &scratch);
        if(face && input_get_mouse_clicked(MOUSE_BUTTON_LEFT))
        {
            jan_toggle_face_in_selection(&selection, face);
        }

        video::Object* video_object = video::get_object(selection_id);
        video::object_update_selection(video_object, mesh, &selection, &heap);

        video_object = video::get_object(selection_wireframe_id);
        video::object_update_wireframe(video_object, mesh, &heap);
    }
}

static void enter_vertex_mode()
{
    selection_pointcloud_id = video::add_object(video::VertexLayout::Point);

    Object* object = &lady.objects[selected_object_index];
    Matrix4 model = matrix4_compose_transform(object->position, object->orientation, float3_one);

    video::Object* video_object = video::get_object(selection_pointcloud_id);
    video::object_set_model(video_object, model);
}

static void exit_vertex_mode()
{
    jan_destroy_selection(&selection);

    video::remove_object(selection_pointcloud_id);
    selection_pointcloud_id = 0;
}

static void update_vertex_mode()
{
    ASSERT(selected_object_index != invalid_index);
    ASSERT(selected_object_index >= 0 && selected_object_index < array_count(lady.objects));

    const float touch_radius = 30.0f;

    Object* object = &lady.objects[selected_object_index];
    JanMesh* mesh = &object->mesh;

    update_camera_controls();

    Matrix4 model = matrix4_compose_transform(object->position, object->orientation, float3_one);
    Matrix4 view = matrix4_look_at(camera.position, camera.target, float3_unit_z);
    Matrix4 projection = matrix4_perspective_projection(camera.field_of_view, viewport.x, viewport.y, camera.near_plane, camera.far_plane);

    Ray ray = ray_from_viewport_point(mouse.position, viewport, view, projection, false);
    ray = transform_ray(ray, matrix4_inverse_transform(model));

    float distance_to_vertex;
    JanVertex* vertex = jan_first_vertex_hit_by_ray(mesh, ray, touch_radius, viewport.x, &distance_to_vertex);
    if(vertex)
    {
        float distance_to_face;
        jan_first_face_hit_by_ray(mesh, ray, &distance_to_face, &scratch);

        if(distance_to_vertex <= distance_to_face && input_get_mouse_clicked(MOUSE_BUTTON_LEFT))
        {
            jan_toggle_vertex_in_selection(&selection, vertex);
        }
    }

    video::Object* video_object = video::get_object(selection_pointcloud_id);
    video::object_update_pointcloud_selection(video_object, mesh, &selection, vertex, &heap);
}

static void request_mode_change(Mode requested_mode)
{
    switch(requested_mode)
    {
        case Mode::Edge:
        {
            if(is_valid_index(selected_object_index))
            {
                switch(mode)
                {
                    case Mode::Edge:
                    {
                        break;
                    }
                    case Mode::Face:
                    {
                        exit_face_mode();
                        break;
                    }
                    case Mode::Vertex:
                    {
                        exit_vertex_mode();
                        break;
                    }
                    case Mode::Object:
                    {
                        break;
                    }
                }
                enter_edge_mode();
                mode = requested_mode;
            }
            break;
        }
        case Mode::Face:
        {
            if(is_valid_index(selected_object_index))
            {
                switch(mode)
                {
                    case Mode::Edge:
                    {
                        exit_edge_mode();
                        break;
                    }
                    case Mode::Face:
                    {
                        break;
                    }
                    case Mode::Vertex:
                    {
                        exit_vertex_mode();
                        break;
                    }
                    case Mode::Object:
                    {
                        break;
                    }
                }
                enter_face_mode();
                mode = requested_mode;
            }
            break;
        }
        case Mode::Object:
        {
            switch(mode)
            {
                case Mode::Edge:
                {
                    exit_edge_mode();
                    break;
                }
                case Mode::Face:
                {
                    exit_face_mode();
                    break;
                }
                case Mode::Vertex:
                {
                    exit_vertex_mode();
                    break;
                }
                case Mode::Object:
                {
                    break;
                }
            }
            mode = requested_mode;
            break;
        }
        case Mode::Vertex:
        {
            if(is_valid_index(selected_object_index))
            {
                switch(mode)
                {
                    case Mode::Edge:
                    {
                        exit_edge_mode();
                        break;
                    }
                    case Mode::Face:
                    {
                        exit_face_mode();
                        break;
                    }
                    case Mode::Vertex:
                    {
                        break;
                    }
                    case Mode::Object:
                    {
                        break;
                    }
                }
                enter_vertex_mode();
                mode = requested_mode;
            }
            break;
        }
    }
}

void editor_update(Platform* platform)
{
    // Interpret user input.
    if(input_get_mouse_clicked(MOUSE_BUTTON_LEFT))
    {
        mouse.drag = true;
        mouse.button = MOUSE_BUTTON_LEFT;
    }
    if(input_get_mouse_clicked(MOUSE_BUTTON_RIGHT))
    {
        mouse.drag = true;
        mouse.button = MOUSE_BUTTON_RIGHT;
    }
    if(mouse.drag)
    {
        if(input_get_mouse_pressed(MOUSE_BUTTON_LEFT)
                || input_get_mouse_pressed(MOUSE_BUTTON_RIGHT))
        {
            Int2 velocity = input_get_mouse_velocity();
            mouse.velocity.x = velocity.x;
            mouse.velocity.y = velocity.y;
        }
        else
        {
            mouse.drag = false;
        }
    }
    {
        Int2 position = input_get_mouse_position();
        mouse.position.x = position.x;
        mouse.position.y = position.y;

        const float scroll_speed = 0.15f;
        Int2 scroll_velocity = input_get_mouse_scroll_velocity();
        mouse.scroll_velocity_y = scroll_speed * scroll_velocity.y;
    }

    // Update the UI system and respond to any events that occurred.
    {
        ui::update(&ui_context, platform);
        ui::Event event;
        while(ui::dequeue(&ui_context.queue, &event))
        {
            if(dialog.enabled)
            {
                handle_input(&dialog, event, &lady, selected_object_index, &history, &ui_context, platform, &heap, &scratch);
            }

            switch(event.type)
            {
                case ui::EventType::Button:
                {
                    ui::Id id = event.button.id;
                    if(id == import_button_id)
                    {
                        dialog.type = DialogType::Import;
                        open_dialog(&dialog, &ui_context, platform, &heap);
                    }
                    else if(id == export_button_id)
                    {
                        dialog.type = DialogType::Export;
                        open_dialog(&dialog, &ui_context, platform, &heap);
                    }
                    else if(id == edge_mode_button_id)
                    {
                        request_mode_change(Mode::Edge);
                    }
                    else if(id == face_mode_button_id)
                    {
                        request_mode_change(Mode::Face);
                    }
                    else if(id == object_mode_button_id)
                    {
                        request_mode_change(Mode::Object);
                    }
                    else if(id == vertex_mode_button_id)
                    {
                        request_mode_change(Mode::Vertex);
                    }
                    break;
                }
                case ui::EventType::Focus_Change:
                case ui::EventType::List_Selection:
                case ui::EventType::Text_Change:
                {
                    break;
                }
            }
        }

        // The export button is only enabled if an object is selected.
        if(!dialog.enabled)
        {
            main_menu->container.items[1].button.enabled = is_valid_index(selected_object_index);
        }
    }

    if(!ui_context.focused_item)
    {
        switch(mode)
        {
            case Mode::Edge:
            {
                update_edge_mode();
                break;
            }
            case Mode::Face:
            {
                update_face_mode();
                break;
            }
            case Mode::Object:
            {
                update_object_mode(platform);
                break;
            }
            case Mode::Vertex:
            {
                update_vertex_mode();
                break;
            }
        }
    }

    // Reset the mouse cursor if nothing is hovered.
    if(!ui_context.anything_hovered && hovered_object_index == invalid_index)
    {
        change_cursor(platform, CURSOR_TYPE_ARROW);
    }

    // Clean up the history.
    {
        for(int i = 0; i < history.changes_to_clean_up_count; i += 1)
        {
            Change change = history.changes_to_clean_up[i];
            switch(change.type)
            {
                case ChangeType::Invalid:
                {
                    ASSERT(false);
                    break;
                }
                case ChangeType::Create_Object:
                case ChangeType::Move:
                {
                    break;
                }
                case ChangeType::Delete_Object:
                {
                    ObjectId id = change.delete_object.object_id;
                    remove_from_storage(&lady, id);
                    history_remove_base_state(&history, id);
                    break;
                }
            }
        }
    }

    video::UpdateState update;
    update.viewport = viewport;
    update.camera = &camera;
    update.move_tool = &move_tool;
    update.rotate_tool = &rotate_tool;
    update.ui_context = &ui_context;
    update.main_menu = main_menu;
    update.dialog_panel = dialog.panel;
    update.dialog_enabled = dialog.enabled;
    update.lady = &lady;
    update.hovered_object_index = hovered_object_index;
    update.selected_object_index = selected_object_index;
    update.selection_id = selection_id;
    update.selection_pointcloud_id = selection_pointcloud_id;
    update.selection_wireframe_id = selection_wireframe_id;

    video::system_update(&update, platform);
}

void editor_destroy_clipboard_copy(char* clipboard)
{
    HEAP_DEALLOCATE(&heap, clipboard);
}

void editor_paste_from_clipboard(Platform* platform, char* clipboard)
{
    ui::accept_paste_from_clipboard(&ui_context, clipboard, platform);
}

void resize_viewport(Int2 dimensions, double dots_per_millimeter)
{
    viewport = dimensions;

    ui_context.viewport.x = dimensions.x;
    ui_context.viewport.y = dimensions.y;

    video::resize_viewport(dimensions, dots_per_millimeter, camera.field_of_view);
}
