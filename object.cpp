#include "object.h"

extern const int object_id_none = 0;

void object_create(Object* object)
{
    jan::create_mesh(&object->mesh);

    object->position = vector3_zero;
    object->orientation = quaternion_identity;

    object->video_object = video::add_object();

}

void object_destroy(Object* object)
{
    jan::destroy_mesh(&object->mesh);

    video::remove_object(object->video_object);
}

void object_set_position(Object* object, Vector3 position)
{
    object->position = position;

    Matrix4 model = compose_transform(object->position, object->orientation, vector3_one);
    video::Object* video_object = video::get_object(object->video_object);
    video::object_set_model(video_object, model);
}
