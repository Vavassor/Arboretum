#include "object.h"

void object_create(Object* object, VideoContext* context)
{
    jan_create_mesh(&object->mesh);

    object->position = float3_zero;
    object->orientation = quaternion_identity;

    object->video_object = video_add_object(context, VERTEX_LAYOUT_PNC);
}

void object_destroy(Object* object, VideoContext* context)
{
    jan_destroy_mesh(&object->mesh);

    video_remove_object(context, object->video_object);
}

void object_set_position(Object* object, Float3 position, VideoContext* context)
{
    object->position = position;

    Matrix4 model = object_get_model(object);
    video_set_model(context, object->video_object, model);
}

Matrix4 object_get_model(Object* object)
{
    return matrix4_compose_transform(object->position, object->orientation, float3_one);
}
