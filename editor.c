#include "editor.h"

#include "array2.h"
#include "assert.h"
#include "asset_paths.h"
#include "camera.h"
#include "closest_point_of_approach.h"
#include "colours.h"
#include "debug_draw.h"
#include "dense_map.h"
#include "file_pick_dialog.h"
#include "float_utilities.h"
#include "history.h"
#include "input.h"
#include "int_utilities.h"
#include "jan_validate.h"
#include "log.h"
#include "math_basics.h"
#include "tools.h"
#include "obj.h"
#include "object_lady.h"
#include "platform.h"
#include "string_build.h"
#include "string_utilities.h"
#include "ui.h"
#include "unicode_load_tables.h"
#include "vector_extras.h"
#include "vector_math.h"

typedef enum Mode
{
    MODE_OBJECT,
    MODE_FACE,
    MODE_EDGE,
    MODE_VERTEX,
} Mode;

typedef enum Action
{
    ACTION_NONE,
    ACTION_ZOOM_CAMERA,
    ACTION_ORBIT_CAMERA,
    ACTION_PAN_CAMERA,
    ACTION_SELECT,
    ACTION_MOVE,
    ACTION_ROTATE,
    ACTION_SCALE,
} Action;

typedef struct MouseState
{
    Float2 position;
    Float2 velocity;
    float scroll_velocity_y;
    MouseButton button;
    bool drag;
} MouseState;

struct Editor
{
    Heap heap;
    Stack scratch;

    ObjectLady lady;
    JanSelection selection;
    int hovered_object_index;
    int selected_object_index;

    Mode mode;
    Action action_in_progress;
    bool translating;
    MoveTool move_tool;
    RotateTool rotate_tool;
    History history;

    VideoContext* video_context;
    DenseMapId selection_id;
    DenseMapId selection_pointcloud_id;
    DenseMapId selection_wireframe_id;
    DenseMapId hover_halo;
    DenseMapId selection_halo;

    Camera camera;
    Int2 viewport;
    FilePickDialog dialog;

    BmfFont font;

    UiContext ui_context;
    UiItem* main_menu;
    UiId import_button_id;
    UiId export_button_id;
    UiId object_mode_button_id;
    UiId vertex_mode_button_id;
    UiId edge_mode_button_id;
    UiId face_mode_button_id;

    MouseState mouse;
};

// Action.......................................................................

static bool action_allowed(Editor* editor, Action action)
{
    return editor->action_in_progress == ACTION_NONE || editor->action_in_progress == action;
}

static void action_perform(Editor* editor, Action action)
{
    editor->action_in_progress = action;
}

static void action_stop(Editor* editor, Action action)
{
    if(editor->action_in_progress == action)
    {
        editor->action_in_progress = ACTION_NONE;
    }
}

// Selection State..............................................................

static void select_mesh(Editor* editor, int index)
{
    jan_destroy_selection(&editor->selection);

    if(is_valid_index(index))
    {
        jan_create_selection(&editor->selection, &editor->heap);
        editor->selection.type = JAN_SELECTION_TYPE_FACE;
    }

    editor->selected_object_index = index;
}

void clear_object_from_hover_and_selection(Editor* editor, ObjectId id, Platform* platform)
{
    Object* object = &editor->lady.objects[editor->selected_object_index];
    if(object->id == id)
    {
        select_mesh(editor, invalid_index);
    }

    object = &editor->lady.objects[editor->hovered_object_index];
    if(object->id == id)
    {
        change_cursor(platform, CURSOR_TYPE_ARROW);
        editor->hovered_object_index = invalid_index;
    }
}

// Update.......................................................................

static void zoom_camera(Editor* editor, Camera* camera)
{
    MouseState* mouse = &editor->mouse;

    Float3 forward = float3_subtract(camera->position, camera->target);
    Float3 moved = float3_add(float3_multiply(-mouse->scroll_velocity_y, forward), camera->position);
    Float3 forward_moved = float3_subtract(moved, camera->target);
    const float too_close = 0.1f;
    const float too_far = 40.0f;
    if(float3_dot(forward_moved, forward) < 0.0f)
    {
        // If this would move the camera past the target, instead place
        // it at a close threshold.
        camera->position = float3_add(float3_multiply(too_close, float3_normalise(forward)), camera->target);
    }
    else if(float3_length(forward_moved) > too_far)
    {
        camera->position = float3_add(float3_multiply(too_far, float3_normalise(forward)), camera->target);
    }
    else
    {
        camera->position = moved;
    }
    action_perform(editor, ACTION_ZOOM_CAMERA);
}

static void orbit_camera(Editor* editor, Camera* camera)
{
    MouseState* mouse = &editor->mouse;

    Float3 forward = float3_subtract(camera->position, camera->target);
    Float3 right = float3_normalise(float3_cross(float3_unit_z, forward));

    const Float2 orbit_speed = (Float2){{0.01f, 0.01f}};
    Float2 angular_velocity = float2_pointwise_multiply(orbit_speed, float2_negate(mouse->velocity));
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
    camera->position = float3_add(quaternion_rotate(orbit, forward), camera->target);

    action_perform(editor, ACTION_ORBIT_CAMERA);
}

static void pan_camera(Editor* editor, Camera* camera)
{
    MouseState* mouse = &editor->mouse;
    Int2 viewport = editor->viewport;

    // Cast rays into world space corresponding to the mouse
    // position for this frame and the frame prior.
    Float2 prior_position = float2_add(mouse->position, mouse->velocity);
    Matrix4 view = camera_get_view(camera);
    Matrix4 projection = camera_get_projection(camera, viewport);
    Ray prior = ray_from_viewport_point(prior_position, viewport, view, projection, false);
    Ray current = ray_from_viewport_point(mouse->position, viewport, view, projection, false);

    // Project the rays onto the plane containing the camera
    // target and facing the camera.
    Float3 n = float3_normalise(float3_subtract(camera->position, camera->target));
    float d0 = float3_dot(float3_subtract(camera->target, current.origin), n) / float3_dot(n, current.direction);
    float d1 = float3_dot(float3_subtract(camera->target, prior.origin), n) / float3_dot(n, prior.direction);
    Float3 p0 = float3_add(float3_multiply(d0, current.direction), current.origin);
    Float3 p1 = float3_add(float3_multiply(d1, prior.direction), prior.origin);

    // Pan by the amount moved across that plane.
    Float3 pan = float3_subtract(p0, p1);
    camera->position = float3_add(camera->position, pan);
    camera->target = float3_add(camera->target, pan);

    action_perform(editor, ACTION_PAN_CAMERA);
}

static void update_camera_controls(Editor* editor)
{
    Camera* camera = &editor->camera;
    MouseState* mouse = &editor->mouse;

    if(mouse->scroll_velocity_y != 0.0f && action_allowed(editor, ACTION_ZOOM_CAMERA))
    {
        zoom_camera(editor, camera);
    }
    else
    {
        action_stop(editor, ACTION_ZOOM_CAMERA);
    }

    if(mouse->drag && (action_allowed(editor, ACTION_ORBIT_CAMERA) || action_allowed(editor, ACTION_PAN_CAMERA)))
    {
        switch(mouse->button)
        {
            case MOUSE_BUTTON_LEFT:
            {
                orbit_camera(editor, camera);
                break;
            }
            case MOUSE_BUTTON_RIGHT:
            {
                pan_camera(editor, camera);
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
        action_stop(editor, ACTION_ORBIT_CAMERA);
        action_stop(editor, ACTION_PAN_CAMERA);
    }
}

static void begin_move(Editor* editor, Ray mouse_ray)
{
    ASSERT(is_valid_index(editor->selected_object_index));
    int index = editor->selected_object_index;
    Object object = editor->lady.objects[index];
    editor->move_tool.reference_position = object.position;

    Float3 point = closest_ray_point(mouse_ray, object.position);
    editor->move_tool.reference_offset = float3_subtract(object.position, point);
}

static void end_move(Editor* editor)
{
    ASSERT(is_valid_index(editor->selected_object_index));
    int index = editor->selected_object_index;
    Object object = editor->lady.objects[index];

    Change change;
    change.type = CHANGE_TYPE_MOVE;
    change.move.object_id = object.id;
    change.move.position = object.position;
    history_add(&editor->history, change);
}

static void move(Editor* editor, Ray mouse_ray)
{
    MoveTool* move_tool = &editor->move_tool;

    ASSERT(is_valid_index(editor->selected_object_index));
    ASSERT(is_valid_index(move_tool->selected_axis) || is_valid_index(move_tool->selected_plane));

    Object* object = &editor->lady.objects[editor->selected_object_index];

    Float3 point;
    if(is_valid_index(move_tool->selected_axis))
    {
        Float3 axis = float3_zero;
        axis.e[move_tool->selected_axis] = 1.0f;
        axis = quaternion_rotate(quaternion_conjugate(object->orientation), axis);

        Float3 line_point = float3_subtract(object->position, move_tool->reference_offset);
        point = closest_point_on_line(mouse_ray, line_point, float3_add(line_point, axis));
    }
    else if(is_valid_index(move_tool->selected_plane))
    {
        Float3 normal = float3_zero;
        normal.e[move_tool->selected_plane] = 1.0f;
        normal = float3_normalise(quaternion_rotate(quaternion_conjugate(object->orientation), normal));

        Float3 origin = float3_subtract(object->position, move_tool->reference_offset);
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

    object_set_position(object, float3_add(point, move_tool->reference_offset), editor->video_context);
}

static void delete_object(Editor* editor, Platform* platform)
{
    Object* object = &editor->lady.objects[editor->selected_object_index];

    Change change;
    change.type = CHANGE_TYPE_DELETE_OBJECT;
    change.delete_object.object_id = object->id;
    history_add(&editor->history, change);

    clear_object_from_hover_and_selection(editor, object->id, platform);

    object_lady_store_object(&editor->lady, object->id, &editor->heap);
}

static void update_object_hover_and_selection(Editor* editor, Platform* platform)
{
    Camera* camera = &editor->camera;
    MouseState* mouse = &editor->mouse;
    Int2 viewport = editor->viewport;

    int prior_hovered = editor->hovered_object_index;
    int prior_selected = editor->selected_object_index;

    // Update pointer hover detection.
    // Casually raycast against every triangle in the scene.
    float closest = infinity;
    editor->hovered_object_index = invalid_index;
    for(int i = 0; i < array_count(editor->lady.objects); i += 1)
    {
        Object* object = &editor->lady.objects[i];
        JanMesh* mesh = &object->mesh;

        Matrix4 model = matrix4_compose_transform(object->position, object->orientation, float3_one);
        Ray ray = camera_get_ray(camera, mouse->position, viewport);
        ray = transform_ray(ray, matrix4_inverse_transform(model));
        float distance;
        JanFace* face = jan_first_face_hit_by_ray(mesh, ray, &distance, &editor->scratch);
        if(face && distance < closest)
        {
            closest = distance;
            editor->hovered_object_index = i;
        }
    }

    // Update the mouse cursor based on the hover status.
    if(is_valid_index(editor->hovered_object_index))
    {
        change_cursor(platform, CURSOR_TYPE_HAND_POINTING);
    }

    // Detect selection.
    if(input_get_mouse_clicked(platform->input_context, MOUSE_BUTTON_LEFT) && is_valid_index(editor->hovered_object_index))
    {
        if(editor->selected_object_index == editor->hovered_object_index)
        {
            select_mesh(editor, invalid_index);
        }
        else
        {
            select_mesh(editor, editor->hovered_object_index);
        }
        action_perform(editor, ACTION_SELECT);
    }
    else
    {
        action_stop(editor, ACTION_SELECT);
    }

    if(prior_hovered != editor->hovered_object_index && is_valid_index(editor->hovered_object_index))
    {
        Object* object = &editor->lady.objects[editor->hovered_object_index];
        video_update_wireframe(editor->video_context, editor->hover_halo, &object->mesh, &editor->heap);
    }

    if(prior_selected != editor->selected_object_index && is_valid_index(editor->selected_object_index))
    {
        Object* object = &editor->lady.objects[editor->selected_object_index];
        video_update_wireframe(editor->video_context, editor->selection_halo, &object->mesh, &editor->heap);
    }
}

static void detect_move_tool_arrow_hover(MoveTool* move_tool, float* closest, Ray ray, Matrix4 model)
{
    int hovered_axis = invalid_index;
    for(int i = 0; i < 3; i += 1)
    {
        Float3 shaft_axis = float3_zero;
        shaft_axis.e[i] = move_tool->shaft_length;
        Float3 head_axis = float3_zero;
        head_axis.e[i] = move_tool->head_height;

        Cylinder cylinder =
        {
            .start = matrix4_transform_point(model, float3_zero),
            .end = matrix4_transform_point(model, shaft_axis),
            .radius = move_tool->scale * move_tool->shaft_radius,
        };

        Float3 intersection;
        bool intersected = intersect_ray_cylinder(ray, cylinder, &intersection);
        float distance = float3_squared_distance(ray.origin, intersection);
        if(intersected && distance < *closest)
        {
            *closest = distance;
            hovered_axis = i;
        }

        Cone cone =
        {
            .base_center = matrix4_transform_point(model, shaft_axis),
            .axis = float3_multiply(move_tool->scale, head_axis),
            .radius = move_tool->scale * move_tool->head_radius,
        };

        intersected = intersect_ray_cone(ray, cone, &intersection);
        distance = float3_squared_distance(ray.origin, intersection);
        if(intersected && distance < *closest)
        {
            *closest = distance;
            hovered_axis = i;
        }
    }
    move_tool->hovered_axis = hovered_axis;
}

static void detect_move_tool_plane_hover(MoveTool* move_tool, float* closest, Ray ray, Matrix4 model)
{
    int hovered_plane = invalid_index;
    for(int i = 0; i < 3; i += 1)
    {
        Float3 center = float3_set_all(move_tool->shaft_length);
        center.e[i] = 0.0f;

        Float3 extents = float3_set_all(move_tool->scale * move_tool->plane_extent);
        extents.e[i] = move_tool->scale * move_tool->plane_thickness;

        Box box =
        {
            .center = matrix4_transform_point(model, center),
            .extents = extents,
            .orientation = move_tool->orientation,
        };

        Float3 intersection;
        bool intersected = intersect_ray_box(ray, box, &intersection);
        if(intersected)
        {
            float distance = float3_squared_distance(ray.origin, intersection);
            if(distance < *closest)
            {
                *closest = distance;
                hovered_plane = i;
            }
        }
    }
    move_tool->hovered_plane = hovered_plane;
    if(is_valid_index(hovered_plane) || is_valid_index(move_tool->selected_plane))
    {
        move_tool->hovered_axis = invalid_index;
    }
}

static void update_move_tool_selection_and_move(Editor* editor, InputContext* input_context, MoveTool* move_tool, Ray ray)
{
    MouseState* mouse = &editor->mouse;

    if(input_get_mouse_clicked(input_context, MOUSE_BUTTON_LEFT))
    {
        if(is_valid_index(move_tool->hovered_axis))
        {
            move_tool->selected_axis = move_tool->hovered_axis;
            begin_move(editor, ray);
            action_perform(editor, ACTION_MOVE);
        }
        else if(is_valid_index(move_tool->hovered_plane))
        {
            move_tool->selected_plane = move_tool->hovered_plane;
            begin_move(editor, ray);
            action_perform(editor, ACTION_MOVE);
        }
    }
    if(mouse->drag && mouse->button == MOUSE_BUTTON_LEFT && (is_valid_index(move_tool->selected_axis) || is_valid_index(move_tool->selected_plane)))
    {
        move(editor, ray);
    }
    else
    {
        if(editor->action_in_progress == ACTION_MOVE)
        {
            end_move(editor);
        }

        move_tool->selected_axis = invalid_index;
        move_tool->selected_plane = invalid_index;
        action_stop(editor, ACTION_MOVE);
    }
}

static void center_move_tool_on_object(MoveTool* move_tool, Object* object)
{
    move_tool->position = object->position;
    move_tool->orientation = object->orientation;
}

static void size_move_tool_in_camera(MoveTool* move_tool, Camera* camera)
{
    Float3 normal = float3_normalise(float3_subtract(camera->target, camera->position));
    float scale = 0.05f * distance_point_plane(move_tool->position, camera->position, normal);
    move_tool->scale = scale;
}

static void update_rotate_tool(RotateTool* rotate_tool, Camera* camera)
{
    Float3 center = (Float3){{0.0f, -5.0f, 0.0f}};
    Float3 normal = float3_normalise(float3_subtract(camera->target, camera->position));
    float radius = 0.2f * distance_point_plane(center, camera->position, normal);

    Disk disks[3] =
    {
        {center, float3_unit_x, radius},
        {center, float3_unit_y, radius},
        {center, float3_unit_z, radius},
    };

    Float3 camera_forward = float3_subtract(camera->target, camera->position);
    Float3 axes[3] =
    {
        float3_subtract(closest_disk_plane(disks[0], camera->position, camera_forward), disks[0].center),
        float3_subtract(closest_disk_plane(disks[1], camera->position, camera_forward), disks[1].center),
        float3_subtract(closest_disk_plane(disks[2], camera->position, camera_forward), disks[2].center),
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

    rotate_tool->position = center;
    rotate_tool->radius = radius;
    rotate_tool->angles[0] = angles[0];
    rotate_tool->angles[1] = angles[1];
    rotate_tool->angles[2] = angles[2];
}

static void add_halo(Editor* editor)
{
    editor->selection_halo = video_add_object(editor->video_context, VERTEX_LAYOUT_LINE);
}

static void remove_halo(Editor* editor)
{
    video_remove_object(editor->video_context, editor->selection_halo);
    editor->selection_halo = 0;
}

static void enter_object_mode(Editor* editor)
{
    add_halo(editor);
    if(is_valid_index(editor->selected_object_index))
    {
        Object* object = &editor->lady.objects[editor->selected_object_index];
        video_update_wireframe(editor->video_context, editor->selection_halo, &object->mesh, &editor->heap);
    }

    editor->hover_halo = video_add_object(editor->video_context, VERTEX_LAYOUT_LINE);
}

static void exit_object_mode(Editor* editor)
{
    remove_halo(editor);

    video_remove_object(editor->video_context, editor->hover_halo);
    editor->hover_halo = 0;
}

static void update_object_mode(Editor* editor, Platform* platform)
{
    Camera* camera = &editor->camera;
    MouseState* mouse = &editor->mouse;
    MoveTool* move_tool = &editor->move_tool;
    RotateTool* rotate_tool = &editor->rotate_tool;
    Int2 viewport = editor->viewport;
    InputContext* input_context = platform->input_context;

    if(is_valid_index(editor->selected_object_index) && action_allowed(editor, ACTION_MOVE))
    {
        Float3 scale = float3_set_all(move_tool->scale);
        Matrix4 model = matrix4_compose_transform(move_tool->position, move_tool->orientation, scale);
        Ray ray = camera_get_ray(camera, mouse->position, viewport);

        float closest = infinity;
        detect_move_tool_arrow_hover(move_tool, &closest, ray, model);
        detect_move_tool_plane_hover(move_tool, &closest, ray, model);

        update_move_tool_selection_and_move(editor, input_context, move_tool, ray);
    }

    if(action_allowed(editor, ACTION_SELECT))
    {
        update_object_hover_and_selection(editor, platform);
    }

    if(is_valid_index(editor->selected_object_index))
    {
        Object* object = &editor->lady.objects[editor->selected_object_index];
        center_move_tool_on_object(move_tool, object);
        size_move_tool_in_camera(move_tool, camera);
    }

    update_rotate_tool(rotate_tool, camera);
    update_camera_controls(editor);

    if(editor->action_in_progress == ACTION_NONE)
    {
        if(input_get_hotkey_tapped(input_context, INPUT_FUNCTION_UNDO))
        {
            undo(&editor->history, &editor->lady, editor->video_context, &editor->heap, editor, platform);
        }
        if(input_get_hotkey_tapped(input_context, INPUT_FUNCTION_REDO))
        {
            redo(&editor->history, &editor->lady, editor->video_context, &editor->heap);
        }
        if(is_valid_index(editor->selected_object_index) && input_get_hotkey_tapped(input_context, INPUT_FUNCTION_DELETE))
        {
            delete_object(editor, platform);
        }
    }
}

static void enter_edge_mode(Editor* editor)
{
    editor->selection_wireframe_id = video_add_object(editor->video_context, VERTEX_LAYOUT_LINE);

    Object* object = &editor->lady.objects[editor->selected_object_index];
    Matrix4 model = object_get_model(object);

    video_set_model(editor->video_context, editor->selection_wireframe_id, model);

    add_halo(editor);
    video_update_wireframe(editor->video_context, editor->selection_halo, &object->mesh, &editor->heap);
}

static void exit_edge_mode(Editor* editor)
{
    jan_destroy_selection(&editor->selection);

    video_remove_object(editor->video_context, editor->selection_wireframe_id);
    editor->selection_wireframe_id = 0;

    remove_halo(editor);
}

static void update_edge_mode(Editor* editor, Platform* platform)
{
    ASSERT(is_valid_index(editor->selected_object_index));
    ASSERT(editor->selected_object_index >= 0 && editor->selected_object_index < array_count(editor->lady.objects));

    const float touch_radius = 30.0f;

    Camera* camera = &editor->camera;
    MouseState* mouse = &editor->mouse;
    Int2 viewport = editor->viewport;

    Object* object = &editor->lady.objects[editor->selected_object_index];
    JanMesh* mesh = &object->mesh;

    update_camera_controls(editor);

    Matrix4 model = matrix4_compose_transform(object->position, object->orientation, float3_one);
    Matrix4 view = matrix4_look_at(camera->position, camera->target, float3_unit_z);
    Matrix4 projection = matrix4_perspective_projection(camera->field_of_view, (float) viewport.x, (float) viewport.y, camera->near_plane, camera->far_plane);
    Matrix4 model_view_projection = matrix4_multiply(projection, matrix4_multiply(view, model));
    Matrix4 inverse_model = matrix4_inverse_transform(model);
    Matrix4 inverse = matrix4_multiply(matrix4_inverse_view(view), matrix4_inverse_perspective(projection));

    Ray ray = ray_from_viewport_point(mouse->position, viewport, view, projection, false);
    ray = transform_ray(ray, inverse_model);

    float distance_to_edge;
    JanEdge* edge = jan_first_edge_under_point(mesh, mouse->position, touch_radius, model_view_projection, inverse, viewport, ray.origin, ray.direction, &distance_to_edge);
    if(edge)
    {
        float distance_to_face = infinity;
        jan_first_face_hit_by_ray(mesh, ray, &distance_to_face, &editor->scratch);

        if(distance_to_edge < distance_to_face && input_get_mouse_clicked(platform->input_context, MOUSE_BUTTON_LEFT))
        {
            jan_toggle_edge_in_selection(&editor->selection, edge);
        }
    }

    video_update_wireframe_selection(editor->video_context, editor->selection_wireframe_id, mesh, &editor->selection, edge, &editor->heap);
}

static void enter_face_mode(Editor* editor)
{
    editor->selection_id = video_add_object(editor->video_context, VERTEX_LAYOUT_PNC);
    editor->selection_wireframe_id = video_add_object(editor->video_context, VERTEX_LAYOUT_LINE);

    // Set the selection's transform to match the selected mesh.
    Object* object = &editor->lady.objects[editor->selected_object_index];
    Matrix4 model = matrix4_compose_transform(object->position, object->orientation, float3_one);

    video_set_model(editor->video_context, editor->selection_id, model);
    video_set_model(editor->video_context, editor->selection_wireframe_id, model);

    add_halo(editor);
    video_update_wireframe(editor->video_context, editor->selection_halo, &object->mesh, &editor->heap);
}

static void exit_face_mode(Editor* editor)
{
    jan_destroy_selection(&editor->selection);

    video_remove_object(editor->video_context, editor->selection_id);
    video_remove_object(editor->video_context, editor->selection_wireframe_id);
    editor->selection_id = 0;
    editor->selection_wireframe_id = 0;

    remove_halo(editor);
}

static void translate_faces(Editor* editor, Object* object, Platform* platform)
{
    Camera* camera = &editor->camera;
    MouseState* mouse = &editor->mouse;
    VideoContext* video_context = editor->video_context;
    JanMesh* mesh = &object->mesh;
    Log* logger = &platform->logger;

    Int2 velocity = input_get_mouse_velocity(platform->input_context);
    mouse->velocity = int2_to_float2(velocity);

    Float3 forward = float3_subtract(camera->position, camera->target);
    Float3 right = float3_normalise(float3_cross(float3_unit_z, forward));
    Float3 up = float3_normalise(float3_cross(right, forward));

    const Float2 move_speed = (Float2){{0.007f, 0.007f}};
    Float2 move_velocity = float2_pointwise_multiply(move_speed, mouse->velocity);
    Float3 move = float3_add(float3_multiply(move_velocity.x, right), float3_multiply(move_velocity.y, up));
    jan_move_faces(mesh, &editor->selection, move);

    ASSERT(jan_validate_mesh(mesh, logger));

    video_update_mesh(video_context, object->video_object, mesh, &editor->heap);
}

static void select_face(Editor* editor, Platform* platform, Object* object)
{
    Camera* camera = &editor->camera;
    MouseState* mouse = &editor->mouse;
    VideoContext* video_context = editor->video_context;
    InputContext* input_context = platform->input_context;
    Int2 viewport = editor->viewport;
    JanMesh* mesh = &object->mesh;

    Matrix4 model = matrix4_compose_transform(object->position, object->orientation, float3_one);
    Ray ray = camera_get_ray(camera, mouse->position, viewport);
    ray = transform_ray(ray, matrix4_inverse_transform(model));

    JanFace* face = jan_first_face_hit_by_ray(mesh, ray, NULL, &editor->scratch);
    if(face && input_get_mouse_clicked(input_context, MOUSE_BUTTON_LEFT))
    {
        jan_toggle_face_in_selection(&editor->selection, face);
    }

    video_update_selection(video_context, editor->selection_id, mesh, &editor->selection, &editor->heap);
    video_update_wireframe(video_context, editor->selection_wireframe_id, mesh, &editor->heap);
}

static void update_face_mode(Editor* editor, Platform* platform)
{
    ASSERT(is_valid_index(editor->selected_object_index));
    ASSERT(editor->selected_object_index >= 0 && editor->selected_object_index < array_count(editor->lady.objects));

    Object* object = &editor->lady.objects[editor->selected_object_index];

    if(input_get_key_tapped(platform->input_context, INPUT_KEY_G))
    {
        editor->translating = !editor->translating;
    }

    if(!editor->translating)
    {
        update_camera_controls(editor);
    }

    if(editor->translating)
    {
        translate_faces(editor, object, platform);
    }

    select_face(editor, platform, object);
}

static void enter_vertex_mode(Editor* editor)
{
    editor->selection_pointcloud_id = video_add_object(editor->video_context, VERTEX_LAYOUT_POINT);

    Object* object = &editor->lady.objects[editor->selected_object_index];
    Matrix4 model = matrix4_compose_transform(object->position, object->orientation, float3_one);

    video_set_model(editor->video_context, editor->selection_pointcloud_id, model);

    add_halo(editor);
    video_update_wireframe(editor->video_context, editor->selection_halo, &object->mesh, &editor->heap);
}

static void exit_vertex_mode(Editor* editor)
{
    jan_destroy_selection(&editor->selection);

    video_remove_object(editor->video_context, editor->selection_pointcloud_id);
    editor->selection_pointcloud_id = 0;

    remove_halo(editor);
}

static void update_vertex_mode(Editor* editor, Platform* platform)
{
    ASSERT(is_valid_index(editor->selected_object_index));
    ASSERT(editor->selected_object_index >= 0 && editor->selected_object_index < array_count(editor->lady.objects));

    const float touch_radius = 30.0f;

    Camera* camera = &editor->camera;
    MouseState* mouse = &editor->mouse;
    Int2 viewport = editor->viewport;

    Object* object = &editor->lady.objects[editor->selected_object_index];
    JanMesh* mesh = &object->mesh;

    update_camera_controls(editor);

    Matrix4 model = matrix4_compose_transform(object->position, object->orientation, float3_one);
    Matrix4 view = camera_get_view(camera);
    Matrix4 projection = camera_get_projection(camera, viewport);

    Ray ray = ray_from_viewport_point(mouse->position, viewport, view, projection, false);
    ray = transform_ray(ray, matrix4_inverse_transform(model));

    float distance_to_vertex;
    JanVertex* vertex = jan_first_vertex_hit_by_ray(mesh, ray, touch_radius, (float) viewport.x, &distance_to_vertex);
    if(vertex)
    {
        float distance_to_face;
        jan_first_face_hit_by_ray(mesh, ray, &distance_to_face, &editor->scratch);

        if(distance_to_vertex <= distance_to_face && input_get_mouse_clicked(platform->input_context, MOUSE_BUTTON_LEFT))
        {
            jan_toggle_vertex_in_selection(&editor->selection, vertex);
        }
    }

    video_update_pointcloud_selection(editor->video_context, editor->selection_pointcloud_id, mesh, &editor->selection, vertex, &editor->heap);
}

static void request_mode_change(Editor* editor, Mode requested_mode)
{
    switch(requested_mode)
    {
        case MODE_EDGE:
        {
            if(is_valid_index(editor->selected_object_index))
            {
                switch(editor->mode)
                {
                    case MODE_EDGE:
                    {
                        break;
                    }
                    case MODE_FACE:
                    {
                        exit_face_mode(editor);
                        break;
                    }
                    case MODE_VERTEX:
                    {
                        exit_vertex_mode(editor);
                        break;
                    }
                    case MODE_OBJECT:
                    {
                        exit_object_mode(editor);
                        break;
                    }
                }
                enter_edge_mode(editor);
                editor->mode = requested_mode;
            }
            break;
        }
        case MODE_FACE:
        {
            if(is_valid_index(editor->selected_object_index))
            {
                switch(editor->mode)
                {
                    case MODE_EDGE:
                    {
                        exit_edge_mode(editor);
                        break;
                    }
                    case MODE_FACE:
                    {
                        break;
                    }
                    case MODE_VERTEX:
                    {
                        exit_vertex_mode(editor);
                        break;
                    }
                    case MODE_OBJECT:
                    {
                        exit_object_mode(editor);
                        break;
                    }
                }
                enter_face_mode(editor);
                editor->mode = requested_mode;
            }
            break;
        }
        case MODE_OBJECT:
        {
            switch(editor->mode)
            {
                case MODE_EDGE:
                {
                    exit_edge_mode(editor);
                    break;
                }
                case MODE_FACE:
                {
                    exit_face_mode(editor);
                    break;
                }
                case MODE_VERTEX:
                {
                    exit_vertex_mode(editor);
                    break;
                }
                case MODE_OBJECT:
                {
                    break;
                }
            }
            enter_object_mode(editor);
            editor->mode = requested_mode;
            break;
        }
        case MODE_VERTEX:
        {
            if(is_valid_index(editor->selected_object_index))
            {
                switch(editor->mode)
                {
                    case MODE_EDGE:
                    {
                        exit_edge_mode(editor);
                        break;
                    }
                    case MODE_FACE:
                    {
                        exit_face_mode(editor);
                        break;
                    }
                    case MODE_VERTEX:
                    {
                        break;
                    }
                    case MODE_OBJECT:
                    {
                        exit_object_mode(editor);
                        break;
                    }
                }
                enter_vertex_mode(editor);
                editor->mode = requested_mode;
            }
            break;
        }
    }
}

// Whole System.................................................................

static void set_up_ui_theme(UiTheme* theme)
{
    theme->colours.button_cap_disabled = (Float4){{0.318f, 0.318f, 0.318f, 1.0f}};
    theme->colours.button_cap_enabled = (Float4){{0.145f, 0.145f, 0.145f, 1.0f}};
    theme->colours.button_cap_hovered_disabled = (Float4){{0.382f, 0.386f, 0.418f, 1.0f}};
    theme->colours.button_cap_hovered_enabled = (Float4){{0.247f, 0.251f, 0.271f, 1.0f}};
    theme->colours.button_label_disabled = (Float3){{0.782f, 0.786f, 0.818f}};
    theme->colours.button_label_enabled = float3_white;
    theme->colours.focus_indicator = (Float4){{1.0f, 1.0f, 0.0f, 0.6f}};
    theme->colours.list_item_background_hovered = (Float4){{1.0f, 1.0f, 1.0f, 0.3f}};
    theme->colours.list_item_background_selected = (Float4){{1.0f, 1.0f, 1.0f, 0.5f}};
    theme->colours.text_input_cursor = float4_white;
    theme->colours.text_input_selection = (Float4){{1.0f, 1.0f, 1.0f, 0.4f}};

    theme->styles[0].background = float4_black;
    theme->styles[1].background = float4_blue;
    theme->styles[2].background = (Float4){{0.145f, 0.145f, 0.145f, 1.0f}};
    theme->styles[3].background = float4_yellow;
}

static void set_up_main_menu(Editor* editor, Platform* platform)
{
    Heap* heap = &editor->heap;
    UiContext* ui_context = &editor->ui_context;

    UiItem* main_menu = ui_create_toplevel_container(ui_context, heap);
    main_menu->type = UI_ITEM_TYPE_CONTAINER;
    main_menu->growable = true;
    main_menu->container.style_type = UI_STYLE_TYPE_MENU_BAR;
    main_menu->container.padding = (UiPadding){{1.0f, 1.0f, 1.0f, 1.0f}};
    main_menu->container.direction = UI_DIRECTION_LEFT_TO_RIGHT;
    main_menu->container.alignment = UI_ALIGNMENT_START;
    main_menu->container.justification = UI_JUSTIFICATION_START;
    editor->main_menu = main_menu;

    const int items_in_row = 6;
    ui_add_row(&main_menu->container, items_in_row, ui_context, heap);

    main_menu->container.items[0].type = UI_ITEM_TYPE_BUTTON;
    main_menu->container.items[0].button.enabled = true;
    main_menu->container.items[0].button.text_block.padding = (UiPadding){{4.0f, 4.0f, 4.0f, 4.0f}};
    ui_set_text(&main_menu->container.items[0].button.text_block, platform->localized_text.main_menu_import_file, heap);
    editor->import_button_id = main_menu->container.items[0].id;

    main_menu->container.items[1].type = UI_ITEM_TYPE_BUTTON;
    main_menu->container.items[1].button.enabled = true;
    main_menu->container.items[1].button.text_block.padding = (UiPadding){{4.0f, 4.0f, 4.0f, 4.0f}};
    ui_set_text(&main_menu->container.items[1].button.text_block, platform->localized_text.main_menu_export_file, heap);
    editor->export_button_id = main_menu->container.items[1].id;

    main_menu->container.items[2].type = UI_ITEM_TYPE_BUTTON;
    main_menu->container.items[2].button.enabled = true;
    main_menu->container.items[2].button.text_block.padding = (UiPadding){{4.0f, 4.0f, 4.0f, 4.0f}};
    ui_set_text(&main_menu->container.items[2].button.text_block, platform->localized_text.main_menu_enter_object_mode, heap);
    editor->object_mode_button_id = main_menu->container.items[2].id;

    main_menu->container.items[3].type = UI_ITEM_TYPE_BUTTON;
    main_menu->container.items[3].button.enabled = true;
    main_menu->container.items[3].button.text_block.padding = (UiPadding){{4.0f, 4.0f, 4.0f, 4.0f}};
    ui_set_text(&main_menu->container.items[3].button.text_block, platform->localized_text.main_menu_enter_face_mode, heap);
    editor->face_mode_button_id = main_menu->container.items[3].id;

    main_menu->container.items[4].type = UI_ITEM_TYPE_BUTTON;
    main_menu->container.items[4].button.enabled = true;
    main_menu->container.items[4].button.text_block.padding = (UiPadding){{4.0f, 4.0f, 4.0f, 4.0f}};
    ui_set_text(&main_menu->container.items[4].button.text_block, platform->localized_text.main_menu_enter_edge_mode, heap);
    editor->edge_mode_button_id = main_menu->container.items[4].id;

    main_menu->container.items[5].type = UI_ITEM_TYPE_BUTTON;
    main_menu->container.items[5].button.enabled = true;
    main_menu->container.items[5].button.text_block.padding = (UiPadding){{4.0f, 4.0f, 4.0f, 4.0f}};
    ui_set_text(&main_menu->container.items[5].button.text_block, platform->localized_text.main_menu_enter_vertex_mode, heap);
    editor->vertex_mode_button_id = main_menu->container.items[5].id;
}

static void update_mouse_state(MouseState* mouse, InputContext* input_context)
{
    if(input_get_mouse_clicked(input_context, MOUSE_BUTTON_LEFT))
    {
        mouse->drag = true;
        mouse->button = MOUSE_BUTTON_LEFT;
    }

    if(input_get_mouse_clicked(input_context, MOUSE_BUTTON_RIGHT))
    {
        mouse->drag = true;
        mouse->button = MOUSE_BUTTON_RIGHT;
    }

    if(mouse->drag)
    {
        if(input_get_mouse_pressed(input_context, MOUSE_BUTTON_LEFT)
                || input_get_mouse_pressed(input_context, MOUSE_BUTTON_RIGHT))
        {
            Int2 velocity = input_get_mouse_velocity(input_context);
            mouse->velocity = int2_to_float2(velocity);
        }
        else
        {
            mouse->drag = false;
        }
    }

    Int2 position = input_get_mouse_position(input_context);
    mouse->position = int2_to_float2(position);

    const float scroll_speed = 0.15f;
    Int2 scroll_velocity = input_get_mouse_scroll_velocity(input_context);
    mouse->scroll_velocity_y = scroll_speed * scroll_velocity.y;
}

static void handle_ui_events(Editor* editor, Platform* platform)
{
    FilePickDialog* dialog = &editor->dialog;
    UiContext* ui_context = &editor->ui_context;

    ui_update(ui_context, platform);
    UiEvent event;
    while(ui_dequeue(&ui_context->queue, &event))
    {
        if(dialog->enabled)
        {
            handle_input(dialog, event, &editor->lady, editor->selected_object_index, &editor->history, editor->video_context, ui_context, platform, &editor->heap, &editor->scratch);
        }

        switch(event.type)
        {
            case UI_EVENT_TYPE_BUTTON:
            {
                UiId id = event.button.id;
                if(id == editor->import_button_id)
                {
                    dialog->type = DIALOG_TYPE_IMPORT;
                    open_dialog(dialog, ui_context, platform, &editor->heap);
                }
                else if(id == editor->export_button_id)
                {
                    dialog->type = DIALOG_TYPE_EXPORT;
                    open_dialog(dialog, ui_context, platform, &editor->heap);
                }
                else if(id == editor->edge_mode_button_id)
                {
                    request_mode_change(editor, MODE_EDGE);
                }
                else if(id == editor->face_mode_button_id)
                {
                    request_mode_change(editor, MODE_FACE);
                }
                else if(id == editor->object_mode_button_id)
                {
                    request_mode_change(editor, MODE_OBJECT);
                }
                else if(id == editor->vertex_mode_button_id)
                {
                    request_mode_change(editor, MODE_VERTEX);
                }
                break;
            }
            case UI_EVENT_TYPE_FOCUS_CHANGE:
            case UI_EVENT_TYPE_LIST_SELECTION:
            case UI_EVENT_TYPE_TEXT_CHANGE:
            {
                break;
            }
        }
    }

    // The export button is only enabled if an object is selected.
    if(!dialog->enabled)
    {
        editor->main_menu->container.items[1].button.enabled = is_valid_index(editor->selected_object_index);
    }
}

static void clean_up_history(Editor* editor)
{
    for(int i = 0; i < editor->history.changes_to_clean_up_count; i += 1)
    {
        Change change = editor->history.changes_to_clean_up[i];
        switch(change.type)
        {
            case CHANGE_TYPE_INVALID:
            {
                ASSERT(false);
                break;
            }
            case CHANGE_TYPE_CREATE_OBJECT:
            case CHANGE_TYPE_MOVE:
            {
                break;
            }
            case CHANGE_TYPE_DELETE_OBJECT:
            {
                ObjectId id = change.delete_object.object_id;
                object_lady_remove_from_storage(&editor->lady, id, editor->video_context);
                history_remove_base_state(&editor->history, id);
                break;
            }
        }
    }
}

static Editor static_editor;

bool editor_start_up(Platform* platform)
{
    Editor* editor = &static_editor;

    Heap* heap = &editor->heap;
    Stack* stack = &editor->scratch;

    stack_create(stack, (uint32_t) uptibytes(1));
    heap_create(heap, (uint32_t) uptibytes(1));

    unicode_load_tables(heap, stack);

    editor->video_context = video_create_context(heap, &platform->logger);

    editor->hovered_object_index = invalid_index;
    editor->selected_object_index = invalid_index;

    History* history = &editor->history;
    ObjectLady* lady = &editor->lady;
    UiContext* ui_context = &editor->ui_context;
    VideoContext* video_context = editor->video_context;

    history_create(history, heap);
    object_lady_create(lady, heap);

    Camera camera =
    {
        .position = (Float3){{-4.0f, -4.0f, 2.0f}},
        .target = float3_zero,
        .field_of_view =  pi_over_2 * (2.0f / 3.0f),
        .near_plane = 0.001f,
        .far_plane = 100.0f,
    };
    editor->camera = camera;

    // Dodecahedron
    {
        Object* dodecahedron = object_lady_add_object(lady, heap, video_context);
        JanMesh* mesh = &dodecahedron->mesh;

        jan_make_a_weird_face(mesh, stack);

        JanSelection selection = jan_select_all(mesh, heap);
        jan_extrude(mesh, &selection, 0.6f, heap, stack);
        jan_destroy_selection(&selection);

        jan_colour_all_faces(mesh, float3_cyan);

        video_update_mesh(video_context, dodecahedron->video_object, mesh, heap);

        Float3 position = (Float3){{2.0f, 0.0f, 0.0f}};
        Quaternion orientation = quaternion_axis_angle(float3_unit_x, pi / 4.0f);
        dodecahedron->orientation = orientation;
        object_set_position(dodecahedron, position, video_context);

        add_object_to_history(history, dodecahedron, heap);
    }

    // Cheese
    {
        Object* cheese = object_lady_add_object(lady, heap, video_context);
        JanMesh* mesh = &cheese->mesh;

        jan_make_a_face_with_holes(mesh, stack);

        video_update_mesh(video_context, cheese->video_object, mesh, heap);

        Float3 position = (Float3){{0.0f, -2.0f, 0.0f}};
        object_set_position(cheese, position, video_context);

        add_object_to_history(history, cheese, heap);
    }

    // Test Model
    {
        Object* test_model = object_lady_add_object(lady, heap, video_context);
        JanMesh* mesh = &test_model->mesh;

        char* path = get_model_path_by_name("test.obj", stack);
        obj_load_file(path, mesh, heap, stack);
        STACK_DEALLOCATE(stack, path);
        jan_colour_all_faces(mesh, float3_yellow);

        Float3 position = (Float3){{-2.0f, 0.0f, 0.0f}};
        object_set_position(test_model, position, video_context);

        video_update_mesh(video_context, test_model->video_object, mesh, heap);

        add_object_to_history(history, test_model, heap);
    }

    ui_create_context(ui_context, heap, stack);

    // Fonts
    {
        char* path = get_font_path_by_name("droid_12.fnt", stack);
        bmf_load_font(&editor->font, path, heap, stack);
        STACK_DEALLOCATE(stack, path);
        video_set_up_font(video_context, &editor->font);
        ui_context->theme.font = &editor->font;
    }

    set_up_ui_theme(&ui_context->theme);
    set_up_main_menu(editor, platform);

    MoveTool move_tool =
    {
        .orientation = quaternion_identity,
        .position = float3_zero,
        .reference_position = float3_zero,
        .scale = 1.0f,
        .shaft_length = 2.0f * sqrtf(3.0f),
        .shaft_radius = 0.125f,
        .head_height = sqrtf(3.0f) / 2.0f,
        .head_radius = 0.5f,
        .plane_extent = 0.6f,
        .plane_thickness = 0.05f,
        .hovered_axis = invalid_index,
        .hovered_plane = invalid_index,
        .selected_axis = invalid_index,
        .selected_plane = invalid_index,
    };
    editor->move_tool = move_tool;

    RotateTool rotate_tool =
    {
        .position = float3_zero,
        .radius = 1.0f,
    };
    editor->rotate_tool = rotate_tool;

    enter_object_mode(editor);

    return true;
}

void editor_shut_down(bool functions_loaded)
{
    Editor* editor = &static_editor;
    Heap* heap = &editor->heap;

    object_lady_destroy(&editor->lady, heap, editor->video_context);

    close_dialog(&editor->dialog, &editor->ui_context, heap);

    jan_destroy_selection(&editor->selection);

    bmf_destroy_font(&editor->font, heap);
    ui_destroy_context(&editor->ui_context, heap);

    history_destroy(&editor->history, heap);

    video_destroy_context(editor->video_context, heap, functions_loaded);

    unicode_unload_tables(heap);

    stack_destroy(&editor->scratch);
    heap_destroy(heap);
}

void editor_update(Platform* platform)
{
    Editor* editor = &static_editor;

    FilePickDialog* dialog = &editor->dialog;
    UiContext* ui_context = &editor->ui_context;
    InputContext* input_context = platform->input_context;

    debug_draw_reset();

    update_mouse_state(&editor->mouse, input_context);
    handle_ui_events(editor, platform);

    if(!ui_context->focused_item)
    {
        switch(editor->mode)
        {
            case MODE_EDGE:
            {
                update_edge_mode(editor, platform);
                break;
            }
            case MODE_FACE:
            {
                update_face_mode(editor, platform);
                break;
            }
            case MODE_OBJECT:
            {
                update_object_mode(editor, platform);
                break;
            }
            case MODE_VERTEX:
            {
                update_vertex_mode(editor, platform);
                break;
            }
        }
    }

    // Reset the mouse cursor if nothing is hovered.
    if(!ui_context->anything_hovered && editor->hovered_object_index == invalid_index)
    {
        change_cursor(platform, CURSOR_TYPE_ARROW);
    }

    clean_up_history(editor);

    VideoUpdate update =
    {
        .viewport = editor->viewport,
        .camera = &editor->camera,
        .move_tool = &editor->move_tool,
        .rotate_tool = &editor->rotate_tool,
        .ui_context = &editor->ui_context,
        .main_menu = editor->main_menu,
        .dialog_panel = dialog->panel,
        .dialog_enabled = dialog->enabled,
        .lady = &editor->lady,
        .hovered_object_index = editor->hovered_object_index,
        .selected_object_index = editor->selected_object_index,
        .selection_id = editor->selection_id,
        .selection_pointcloud_id = editor->selection_pointcloud_id,
        .selection_wireframe_id = editor->selection_wireframe_id,
        .hover_halo = editor->hover_halo,
        .selection_halo = editor->selection_halo,
    };
    video_update_context(editor->video_context, &update, platform);
}

void editor_destroy_clipboard_copy(char* clipboard)
{
    Editor* editor = &static_editor;
    HEAP_DEALLOCATE(&editor->heap, clipboard);
}

void editor_paste_from_clipboard(Platform* platform, char* clipboard)
{
    Editor* editor = &static_editor;

    ui_accept_paste_from_clipboard(&editor->ui_context, clipboard, platform);
}

void resize_viewport(Int2 dimensions, double dots_per_millimeter)
{
    Editor* editor = &static_editor;

    editor->viewport = dimensions;

    editor->ui_context.viewport.x = (float) dimensions.x;
    editor->ui_context.viewport.y = (float) dimensions.y;

    video_resize_viewport(dimensions, dots_per_millimeter, editor->camera.field_of_view);
}
