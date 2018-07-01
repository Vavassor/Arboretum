#include "object.h"

#include "video_object.h"

void object_create(Object* object)
{
    jan_create_mesh(&object->mesh);

    object->position = float3_zero;
    object->orientation = quaternion_identity;

    object->video_object = video_add_object(VERTEX_LAYOUT_PNC);
}

void object_destroy(Object* object)
{
    jan_destroy_mesh(&object->mesh);

    video_remove_object(object->video_object);
}

void object_set_position(Object* object, Float3 position)
{
    object->position = position;

    Matrix4 model = matrix4_compose_transform(object->position, object->orientation, float3_one);
    VideoObject* video_object = video_get_object(object->video_object);
    video_object_set_model(video_object, model);
}
