#ifndef OBJECT_H_
#define OBJECT_H_

#include "video.h"
#include "jan.h"

typedef unsigned int ObjectId;
extern const int object_id_none;

struct Object
{
    jan::Mesh mesh;

    Vector3 position;
    Quaternion orientation;

    video::Object* video_object;

    ObjectId id;
};

void object_create(Object* object);
void object_destroy(Object* object);
void object_set_position(Object* object, Vector3 position);

#endif // OBJECT_H_
