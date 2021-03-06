#include "camera.h"

Matrix4 camera_get_view(Camera* camera)
{
    return matrix4_look_at(camera->position, camera->target, float3_unit_z);
}

Matrix4 camera_get_projection(Camera* camera, Int2 viewport)
{
    float fov = camera->field_of_view;
    float width = (float) viewport.x;
    float height = (float) viewport.y;
    float near = camera->near_plane;
    float far = camera->far_plane;
    return matrix4_perspective_projection(fov, width, height, near, far);
}

Ray camera_get_ray(Camera* camera, Float2 point, Int2 viewport)
{
    Matrix4 view = camera_get_view(camera);
    Matrix4 projection = camera_get_projection(camera, viewport);
    Ray ray = ray_from_viewport_point(point, viewport, view, projection, false);
    return ray;
}

Float2 viewport_point_to_ndc(Float2 point, Int2 viewport)
{
    float extent_x = viewport.x / 2.0f;
    float extent_y = viewport.y / 2.0f;

    Float2 result;
    result.x = (point.x - extent_x) / extent_x;
    result.y = -(point.y - extent_y) / extent_y;
    return result;
}

Ray ray_from_viewport_point(Float2 point, Int2 viewport, Matrix4 view, Matrix4 projection, bool orthographic)
{
    float extent_x = viewport.x / 2.0f;
    float extent_y = viewport.y / 2.0f;
    point.x = (point.x - extent_x) / extent_x;
    point.y = -(point.y - extent_y) / extent_y;

    Matrix4 inverse_projection;
    if(orthographic)
    {
        inverse_projection = matrix4_inverse_orthographic(projection);
    }
    else
    {
        inverse_projection = matrix4_inverse_perspective(projection);
    }

    Matrix4 inverse_view = matrix4_inverse_view(view);
    Matrix4 inverse = matrix4_multiply(inverse_view, inverse_projection);

    Float3 near = {{point.x, point.y, 0.0f}};
    Float3 far = {{point.x, point.y, 1.0f}};

    Float3 start = matrix4_transform_point(inverse, near);
    Float3 end = matrix4_transform_point(inverse, far);

    Ray result;
    result.origin = start;
    result.direction = float3_normalise(float3_subtract(end, start));
    return result;
}
