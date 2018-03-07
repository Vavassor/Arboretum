#include "editor.h"

#include "assert.h"
#include "camera.h"
#include "colours.h"
#include "file_pick_dialog.h"
#include "float_utilities.h"
#include "history.h"
#include "input.h"
#include "int_utilities.h"
#include "logging.h"
#include "loop_macros.h"
#include "math_basics.h"
#include "move_tool.h"
#include "obj.h"
#include "object_lady.h"
#include "platform.h"
#include "string_build.h"
#include "string_utilities.h"
#include "ui.h"
#include "vector_math.h"

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
    const int invalid_index = -1;

    Heap heap;
    Stack scratch;

    ObjectLady lady;

    Mode mode;
    Action action_in_progress;
    bool translating;
    MoveTool move_tool;
    History history;

    int hovered_object_index;
    int selected_object_index;
    jan::Selection selection;

    bmfont::Font font;

    ui::Context ui_context;
    ui::Item* main_menu;
    ui::Item* test_anime;
    ui::Id import_button_id;
    ui::Id export_button_id;
    ui::Id object_mode_button_id;
    ui::Id face_mode_button_id;

    struct
    {
        Vector2 position;
        Vector2 velocity;
        float scroll_velocity_y;
        input::MouseButton button;
        bool drag;
    } mouse;

    Camera camera;
    Viewport viewport;
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

static const char* action_as_string(Action action)
{
    switch(action)
    {
        default:
        case Action::None:         return "None";
        case Action::Zoom_Camera:  return "Zoom_Camera";
        case Action::Orbit_Camera: return "Orbit_Camera";
        case Action::Pan_Camera:   return "Pan_Camera";
        case Action::Select:       return "Select";
        case Action::Move:         return "Move";
        case Action::Rotate:       return "Rotate";
        case Action::Scale:        return "Scale";
    }
}

// Selection State..............................................................

static void select_mesh(int index)
{
    jan::destroy_selection(&selection);

    if(index != invalid_index)
    {
        jan::create_selection(&selection, &heap);
        selection.type = jan::Selection::Type::Face;
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
        change_cursor(platform, CursorType::Arrow);
        hovered_object_index = invalid_index;
    }
}

// Whole System.................................................................

bool editor_start_up()
{
    stack_create(&scratch, MEBIBYTES(16));
    heap_create(&heap, MEBIBYTES(16));

    hovered_object_index = invalid_index;
    selected_object_index = invalid_index;

    history_create(&history, &heap);
    create_object_lady(&lady, &heap);

    // Camera
    {
        camera.position = {-4.0f, -4.0f, 2.0f};
        camera.target = vector3_zero;
        camera.field_of_view =  pi_over_2 * (2.0f / 3.0f);
        camera.near_plane = 0.001f;
        camera.far_plane = 100.0f;
    }

    // Dodecahedron
    {
        Object* dodecahedron = add_object(&lady, &heap);
        jan::Mesh* mesh = &dodecahedron->mesh;

        jan::make_a_weird_face(mesh);

        jan::Selection selection = jan::select_all(mesh, &heap);
        jan::extrude(mesh, &selection, 0.6f, &scratch);
        jan::destroy_selection(&selection);

        jan::colour_all_faces(mesh, vector3_cyan);

        video::Object* video_object = video::get_object(dodecahedron->video_object);
        video::object_update_mesh(video_object, mesh, &heap);

        obj::save_file("weird.obj", mesh, &heap);

        Vector3 position = {2.0f, 0.0f, 0.0f};
        Quaternion orientation = axis_angle_rotation(vector3_unit_x, pi / 4.0f);
        dodecahedron->orientation = orientation;
        object_set_position(dodecahedron, position);

        add_object_to_history(&history, dodecahedron, &heap);
    }

    // Test Model
    {
        Object* test_model = add_object(&lady, &heap);
        jan::Mesh* mesh = &test_model->mesh;

        obj::load_file("test.obj", mesh, &heap, &scratch);
        jan::colour_all_faces(mesh, vector3_yellow);

        Vector3 position = {-2.0f, 0.0f, 0.0f};
        object_set_position(test_model, position);

        video::Object* video_object = video::get_object(test_model->video_object);
        video::object_update_mesh(video_object, mesh, &heap);

        add_object_to_history(&history, test_model, &heap);
    }

    ui::create_context(&ui_context, &heap, &scratch);

    // Fonts
    {
        bmfont::load_font(&font, "droid_12.fnt", &heap, &scratch);
        video::set_up_font(&font);
    }

    // Setup the main menu.
    {
        main_menu = ui::create_toplevel_container(&ui_context, &heap);
        main_menu->type = ui::ItemType::Container;
        main_menu->growable = true;
        main_menu->container.background_colour = {0.145f, 0.145f, 0.145f, 1.0f};
        main_menu->container.padding = {1.0f, 1.0f, 1.0f, 1.0f};
        main_menu->container.direction = ui::Direction::Left_To_Right;
        main_menu->container.alignment = ui::Alignment::Start;
        main_menu->container.justification = ui::Justification::Start;

        const int items_in_row = 4;
        ui::add_row(&main_menu->container, items_in_row, &ui_context, &heap);

        main_menu->container.items[0].type = ui::ItemType::Button;
        main_menu->container.items[0].button.text_block.padding = {4.0f, 4.0f, 4.0f, 4.0f};
        main_menu->container.items[0].button.text_block.text = copy_string_onto_heap("Import .obj", &heap);
        main_menu->container.items[0].button.text_block.font = &font;
        import_button_id = main_menu->container.items[0].id;

        main_menu->container.items[1].type = ui::ItemType::Button;
        main_menu->container.items[1].button.text_block.padding = {4.0f, 4.0f, 4.0f, 4.0f};
        main_menu->container.items[1].button.text_block.text = copy_string_onto_heap("Export .obj", &heap);
        main_menu->container.items[1].button.text_block.font = &font;
        export_button_id = main_menu->container.items[1].id;

        main_menu->container.items[2].type = ui::ItemType::Button;
        main_menu->container.items[2].button.text_block.padding = {4.0f, 4.0f, 4.0f, 4.0f};
        main_menu->container.items[2].button.text_block.text = copy_string_onto_heap("Object Mode", &heap);
        main_menu->container.items[2].button.text_block.font = &font;
        object_mode_button_id = main_menu->container.items[2].id;

        main_menu->container.items[3].type = ui::ItemType::Button;
        main_menu->container.items[3].button.text_block.padding = {4.0f, 4.0f, 4.0f, 4.0f};
        main_menu->container.items[3].button.text_block.text = copy_string_onto_heap("Face Mode", &heap);
        main_menu->container.items[3].button.text_block.font = &font;
        face_mode_button_id = main_menu->container.items[3].id;
    }

    // Set up the test anime.
    {
        test_anime = ui::create_toplevel_container(&ui_context, &heap);
        test_anime->type = ui::ItemType::Container;
        test_anime->growable = true;
        test_anime->container.background_colour = {0.145f, 0.145f, 0.145f, 1.0f};
        test_anime->container.padding = {1.0f, 1.0f, 1.0f, 1.0f};
        test_anime->container.direction = ui::Direction::Left_To_Right;
        test_anime->container.alignment = ui::Alignment::Start;
        test_anime->container.justification = ui::Justification::Start;

        ui::add_row(&test_anime->container, 1, &ui_context, &heap);

        test_anime->container.items[0].type = ui::ItemType::Text_Input;
        test_anime->container.items[0].text_input.text_block.padding = {4.0f, 4.0f, 4.0f, 4.0f};
        test_anime->container.items[0].text_input.text_block.text = copy_string_onto_heap("-", &heap);
        test_anime->container.items[0].text_input.text_block.font = &font;
    }

    // Move tool
    {
        move_tool.orientation = quaternion_identity;
        move_tool.position = vector3_zero;
        move_tool.reference_position = vector3_zero;
        move_tool.scale = 1.0f;
        move_tool.shaft_length = 2.0f * sqrt(3.0f);
        move_tool.shaft_radius = 0.125f;
        move_tool.head_height = sqrt(3.0f) / 2.0f;
        move_tool.head_radius = 0.5f;
        move_tool.plane_extent = 0.4f;
        move_tool.plane_thickness = 0.05f;
        move_tool.hovered_axis = invalid_index;
        move_tool.hovered_plane = invalid_index;
        move_tool.selected_axis = invalid_index;
        move_tool.selected_plane = invalid_index;
    }

    return true;
}

void editor_shut_down()
{
    destroy_object_lady(&lady, &heap);

    close_dialog(&dialog, &ui_context, &heap);

    jan::destroy_selection(&selection);

    bmfont::destroy_font(&font, &heap);
    ui::destroy_context(&ui_context, &heap);

    history_destroy(&history, &heap);

    stack_destroy(&scratch);
    heap_destroy(&heap);
}

static void update_camera_controls()
{
    Vector3 forward = camera.position - camera.target;
    Vector3 right = normalise(cross(vector3_unit_z, forward));
    Matrix4 projection = perspective_projection_matrix(camera.field_of_view, viewport.width, viewport.height, camera.near_plane, camera.far_plane);

    if(mouse.scroll_velocity_y != 0.0f && action_allowed(Action::Zoom_Camera))
    {
        Vector3 moved = (mouse.scroll_velocity_y * -forward) + camera.position;
        Vector3 forward_moved = moved - camera.target;
        const float too_close = 0.1f;
        const float too_far = 40.0f;
        if(dot(forward_moved, forward) < 0.0f)
        {
            // If this would move the camera past the target, instead place
            // it at a close threshold.
            camera.position = (too_close * normalise(forward)) + camera.target;
        }
        else if(length(forward_moved) > too_far)
        {
            camera.position = (too_far * normalise(forward)) + camera.target;
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
            case input::MouseButton::Left:
            {
                const Vector2 orbit_speed = {0.01f, 0.01f};
                Vector2 angular_velocity = pointwise_multiply(orbit_speed, -mouse.velocity);
                Quaternion orbit_x = axis_angle_rotation(vector3_unit_z, angular_velocity.x);
                Quaternion orbit_y = axis_angle_rotation(right, angular_velocity.y);
                if(dot(cross(forward, vector3_unit_z), cross(orbit_y * forward, vector3_unit_z)) < 0.0f)
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
                camera.position = (orbit_y * orbit_x * forward) + camera.target;

                action_perform(Action::Orbit_Camera);
                break;
            }
            case input::MouseButton::Right:
            {
                // Cast rays into world space corresponding to the mouse
                // position for this frame and the frame prior.
                Vector2 prior_position = mouse.position + mouse.velocity;
                Matrix4 view = look_at_matrix(camera.position, camera.target, vector3_unit_z);
                Ray prior = ray_from_viewport_point(prior_position, viewport, view, projection, false);
                Ray current = ray_from_viewport_point(mouse.position, viewport, view, projection, false);

                // Project the rays onto the plane containing the camera
                // target and facing the camera.
                Vector3 n = normalise(camera.position - camera.target);
                float d0 = dot(camera.target - current.origin, n) / dot(n, current.direction);
                float d1 = dot(camera.target - prior.origin, n) / dot(n, prior.direction);
                Vector3 p0 = (d0 * current.direction) + current.origin;
                Vector3 p1 = (d1 * prior.direction) + prior.origin;

                // Pan by the amount moved across that plane.
                Vector3 pan = p0 - p1;
                camera.position += pan;
                camera.target += pan;

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
    ASSERT(selected_object_index != invalid_index);
    Object object = lady.objects[selected_object_index];
    move_tool.reference_position = object.position;

    Vector3 point = closest_ray_point(mouse_ray, object.position);
    move_tool.reference_offset = object.position - point;
}

static void end_move()
{
    ASSERT(selected_object_index != invalid_index);
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
    ASSERT(selected_object_index != invalid_index);
    ASSERT(move_tool.selected_axis != invalid_index || move_tool.selected_plane != invalid_index);

    Object* object = &lady.objects[selected_object_index];

    Vector3 point;
    if(move_tool.selected_axis != invalid_index)
    {
        Vector3 axis = vector3_zero;
        axis[move_tool.selected_axis] = 1.0f;
        axis = conjugate(object->orientation) * axis;

        Vector3 line_point = object->position - move_tool.reference_offset;
        point = closest_point_on_line(mouse_ray, line_point, line_point + axis);
    }
    else if(move_tool.selected_plane != invalid_index)
    {
        Vector3 normal = vector3_zero;
        normal[move_tool.selected_plane] = 1.0f;
        normal = normalise(conjugate(object->orientation) * normal);

        Vector3 origin = object->position - move_tool.reference_offset;
        Vector3 intersection;
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

    object_set_position(object, point + move_tool.reference_offset);
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
    Matrix4 projection = perspective_projection_matrix(camera.field_of_view, viewport.width, viewport.height, camera.near_plane, camera.far_plane);

    // Update the move tool.
    if(selected_object_index != invalid_index && action_allowed(Action::Move))
    {
        Vector3 scale = set_all_vector3(move_tool.scale);
        Matrix4 model = compose_transform(move_tool.position, move_tool.orientation, scale);
        Matrix4 view = look_at_matrix(camera.position, camera.target, vector3_unit_z);
        Ray ray = ray_from_viewport_point(mouse.position, viewport, view, projection, false);

        int hovered_axis = invalid_index;
        float closest = infinity;
        FOR_N(i, 3)
        {
            Vector3 shaft_axis = vector3_zero;
            shaft_axis[i] = move_tool.shaft_length;
            Vector3 head_axis = vector3_zero;
            head_axis[i] = move_tool.head_height;

            Cylinder cylinder;
            cylinder.start = transform_point(model, vector3_zero);
            cylinder.end = transform_point(model, shaft_axis);
            cylinder.radius = move_tool.scale * move_tool.shaft_radius;

            Vector3 intersection;
            bool intersected = intersect_ray_cylinder(ray, cylinder, &intersection);
            float distance = squared_distance(ray.origin, intersection);
            if(intersected && distance < closest)
            {
                closest = distance;
                hovered_axis = i;
            }

            Cone cone;
            cone.base_center = transform_point(model, shaft_axis);
            cone.axis = move_tool.scale * head_axis;
            cone.radius = move_tool.scale * move_tool.head_radius;

            intersected = intersect_ray_cone(ray, cone, &intersection);
            distance = squared_distance(ray.origin, intersection);
            if(intersected && distance < closest)
            {
                closest = distance;
                hovered_axis = i;
            }
        }
        move_tool.hovered_axis = hovered_axis;

        int hovered_plane = invalid_index;
        FOR_N(i, 3)
        {
            Vector3 center = set_all_vector3(move_tool.shaft_length);
            center[i] = 0.0f;

            Vector3 extents = set_all_vector3(move_tool.scale * move_tool.plane_extent);
            extents[i] = move_tool.scale * move_tool.plane_thickness;

            Box box;
            box.center = transform_point(model, center);
            box.extents = extents;
            box.orientation = move_tool.orientation;

            Vector3 intersection;
            bool intersected = intersect_ray_box(ray, box, &intersection);
            if(intersected)
            {
                float distance = squared_distance(ray.origin, intersection);
                if(distance < closest)
                {
                    closest = distance;
                    hovered_plane = i;
                }
            }
        }
        move_tool.hovered_plane = hovered_plane;
        if(hovered_plane != invalid_index || move_tool.selected_plane != invalid_index)
        {
            move_tool.hovered_axis = invalid_index;
        }

        if(input::get_mouse_clicked(input::MouseButton::Left))
        {
            if(hovered_axis != invalid_index)
            {
                move_tool.selected_axis = hovered_axis;
                begin_move(ray);
                action_perform(Action::Move);
            }
            else if(hovered_plane != invalid_index)
            {
                move_tool.selected_plane = hovered_plane;
                begin_move(ray);
                action_perform(Action::Move);
            }
        }
        if(mouse.drag && mouse.button == input::MouseButton::Left && (move_tool.selected_axis != invalid_index || move_tool.selected_plane != invalid_index))
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
        int prior_hovered_index = hovered_object_index;
        hovered_object_index = invalid_index;
        FOR_N(i, lady.objects_count)
        {
            Object* object = &lady.objects[i];
            jan::Mesh* mesh = &object->mesh;

            Matrix4 model = compose_transform(object->position, object->orientation, vector3_one);
            Matrix4 view = look_at_matrix(camera.position, camera.target, vector3_unit_z);
            Ray ray = ray_from_viewport_point(mouse.position, viewport, view, projection, false);
            ray = transform_ray(ray, inverse_transform(model));
            float distance;
            jan::Face* face = first_face_hit_by_ray(mesh, ray, &distance);
            if(face && distance < closest)
            {
                closest = distance;
                hovered_object_index = i;
            }
        }

        // Update the mouse cursor based on the hover status.
        if(prior_hovered_index == invalid_index && hovered_object_index != invalid_index)
        {
            change_cursor(platform, CursorType::Hand_Pointing);
        }
        if(prior_hovered_index != invalid_index && hovered_object_index == invalid_index)
        {
            change_cursor(platform, CursorType::Arrow);
        }

        // Detect selection.
        if(input::get_mouse_clicked(input::MouseButton::Left) && hovered_object_index != invalid_index)
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
    if(selected_object_index != invalid_index)
    {
        Object object = lady.objects[selected_object_index];
        Vector3 position = object.position;
        Vector3 normal = normalise(camera.target - camera.position);
        float scale = 0.05f * distance_point_plane(position, camera.position, normal);
        move_tool.position = object.position;
        move_tool.orientation = object.orientation;
        move_tool.scale = scale;
    }

    update_camera_controls();

    if(action_in_progress == Action::None)
    {
        if(input::get_hotkey_tapped(input::Function::Undo))
        {
            undo(&history, &lady, &heap, platform);
        }
        if(input::get_hotkey_tapped(input::Function::Redo))
        {
            redo(&history, &lady, &heap);
        }
        if(selected_object_index != invalid_index && input::get_hotkey_tapped(input::Function::Delete))
        {
            delete_object(platform);
        }
    }
}

static void update_face_mode()
{
    ASSERT(selected_object_index != invalid_index);
    ASSERT(selected_object_index >= 0 && selected_object_index < lady.objects_count);

    Object* object = &lady.objects[selected_object_index];
    jan::Mesh* mesh = &object->mesh;
    Matrix4 projection = perspective_projection_matrix(camera.field_of_view, viewport.width, viewport.height, camera.near_plane, camera.far_plane);

    if(input::get_key_tapped(input::Key::G))
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
        int velocity_x;
        int velocity_y;
        input::get_mouse_velocity(&velocity_x, &velocity_y);
        mouse.velocity.x = velocity_x;
        mouse.velocity.y = velocity_y;

        Vector3 forward = camera.position - camera.target;
        Vector3 right = normalise(cross(vector3_unit_z, forward));
        Vector3 up = normalise(cross(right, forward));

        const Vector2 move_speed = {0.007f, 0.007f};
        Vector2 move_velocity = pointwise_multiply(move_speed, mouse.velocity);
        Vector3 move = (move_velocity.x * right) + (move_velocity.y * up);
        move_faces(mesh, &selection, move);
    }

    // Cast a ray from the mouse to the test model.
    {
        jan::colour_all_faces(mesh, vector3_yellow);
        jan::colour_selection(mesh, &selection, vector3_cyan);

        Matrix4 model = compose_transform(object->position, object->orientation, vector3_one);
        Matrix4 view = look_at_matrix(camera.position, camera.target, vector3_unit_z);
        Ray ray = ray_from_viewport_point(mouse.position, viewport, view, projection, false);
        ray = transform_ray(ray, inverse_transform(model));
        jan::Face* face = first_face_hit_by_ray(mesh, ray, nullptr);
        if(face)
        {
            if(input::get_key_tapped(input::Key::F))
            {
                toggle_face_in_selection(&selection, face);
                jan::colour_just_the_one_face(face, vector3_cyan);
            }
            else
            {
                jan::colour_just_the_one_face(face, vector3_red);
            }
        }
        video::Object* video_object = video::get_object(object->video_object);
        video::object_update_mesh(video_object, mesh, &heap);
    }
}

static void exit_face_mode()
{
    Object* object = &lady.objects[selected_object_index];
    jan::Mesh* mesh = &object->mesh;

    jan::colour_all_faces(mesh, vector3_yellow);
    video::Object* video_object = video::get_object(object->video_object);
    video::object_update_mesh(video_object, mesh, &heap);
    destroy_selection(&selection);
}

static void request_mode_change(Mode requested_mode)
{
    switch(requested_mode)
    {
        case Mode::Object:
        {
            switch(mode)
            {
                case Mode::Face:
                {
                    exit_face_mode();
                    break;
                }
                case Mode::Object:
                case Mode::Edge:
                case Mode::Vertex:
                {
                    break;
                }
            }
            mode = requested_mode;
            break;
        }
        case Mode::Face:
        case Mode::Edge:
        case Mode::Vertex:
        {
            if(selected_object_index != invalid_index)
            {
                mode = requested_mode;
            }
            break;
        }
    }
}

void editor_update(Platform* platform)
{
    // Interpret user input.
    if(input::get_mouse_clicked(input::MouseButton::Left))
    {
        mouse.drag = true;
        mouse.button = input::MouseButton::Left;
    }
    if(input::get_mouse_clicked(input::MouseButton::Right))
    {
        mouse.drag = true;
        mouse.button = input::MouseButton::Right;
    }
    if(mouse.drag)
    {
        if(
            input::get_mouse_pressed(input::MouseButton::Left) ||
            input::get_mouse_pressed(input::MouseButton::Right))
        {
            int velocity_x;
            int velocity_y;
            input::get_mouse_velocity(&velocity_x, &velocity_y);
            mouse.velocity.x = velocity_x;
            mouse.velocity.y = velocity_y;
        }
        else
        {
            mouse.drag = false;
        }
    }
    {
        int position_x;
        int position_y;
        input::get_mouse_position(&position_x, &position_y);
        mouse.position.x = position_x;
        mouse.position.y = position_y;

        const float scroll_speed = 0.15f;
        int scroll_velocity_x;
        int scroll_velocity_y;
        input::get_mouse_scroll_velocity(&scroll_velocity_x, &scroll_velocity_y);
        mouse.scroll_velocity_y = scroll_speed * scroll_velocity_y;
    }

    // Update the UI system and respond to any events that occurred.
    {
        ui::update(&ui_context, platform);
        ui::Event event;
        while(ui::dequeue(&ui_context.queue, &event))
        {
            if(dialog.enabled)
            {
                handle_input(&dialog, event, &font, &lady, &history, &ui_context, &heap, &scratch);
            }
            else
            {
                switch(event.type)
                {
                    case ui::EventType::Button:
                    {
                        ui::Id id = event.button.id;
                        if(id == import_button_id)
                        {
                            open_dialog(&dialog, &font, &ui_context, &heap);
                            dialog.panel->unfocusable = false;
                        }
                        else if(id == export_button_id)
                        {

                        }
                        else if(id == object_mode_button_id)
                        {
                            request_mode_change(Mode::Object);
                        }
                        else if(id == face_mode_button_id)
                        {
                            request_mode_change(Mode::Face);
                        }
                        break;
                    }
                }
            }
        }
    }

    if(!ui_context.focused_container)
    {
        switch(mode)
        {
            case Mode::Object:
            {
                update_object_mode(platform);
                break;
            }
            case Mode::Face:
            {
                update_face_mode();
                break;
            }
            case Mode::Edge:
            {
                break;
            }
            case Mode::Vertex:
            {
                break;
            }
        }
    }

    // Clean up the history.
    {
        FOR_N(i, history.changes_to_clean_up_count)
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
    update.ui_context = &ui_context;
    update.main_menu = main_menu;
    update.dialog_panel = dialog.panel;
    update.dialog_enabled = dialog.enabled;
    update.test_anime = test_anime;
    update.lady = &lady;
    update.hovered_object_index = hovered_object_index;
    update.selected_object_index = selected_object_index;

    video::system_update(&update, platform);
}

void editor_destroy_clipboard_copy(char* clipboard)
{
    HEAP_DEALLOCATE(&heap, clipboard);
}

void editor_paste_from_clipboard(char* clipboard)
{
    ui::TextInput* text_input = &test_anime->container.items[0].text_input;
    ui::insert_text(text_input, clipboard, &heap);
}

void resize_viewport(int width, int height, double dots_per_millimeter)
{
    viewport.width = width;
    viewport.height = height;

    ui_context.viewport.x = width;
    ui_context.viewport.y = height;

    video::resize_viewport(width, height, dots_per_millimeter, camera.field_of_view);
}
