#include "camera.h"

Ray ray_from_viewport_point(Vector2 point, Int2 viewport, Matrix4 view, Matrix4 projection, bool orthographic)
{
    float extent_x = viewport.x / 2.0f;
    float extent_y = viewport.y / 2.0f;
    point.x = (point.x - extent_x) / extent_x;
    point.y = -(point.y - extent_y) / extent_y;

    Matrix4 inverse;
    if(orthographic)
    {
        inverse = inverse_view_matrix(view) * inverse_orthographic_matrix(projection);
    }
    else
    {
        inverse = inverse_view_matrix(view) * inverse_perspective_matrix(projection);
    }

    Vector3 near = {point.x, point.y, 0.0f};
    Vector3 far = {point.x, point.y, 1.0f};

    Vector3 start = transform_point(inverse, near);
    Vector3 end = transform_point(inverse, far);

    Ray result;
    result.origin = start;
    result.direction = normalise(end - start);
    return result;
}
