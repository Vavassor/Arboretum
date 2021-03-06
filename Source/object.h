#ifndef OBJECT_H_
#define OBJECT_H_

#include "jan.h"
#include "video.h"

typedef unsigned int ObjectId;
extern const int object_id_none;

typedef struct Object
{
    JanMesh mesh;

    Float3 position;
    Quaternion orientation;

    DenseMapId video_object;

    ObjectId id;
} Object;

void object_create(Object* object, VideoContext* context);
void object_destroy(Object* object, VideoContext* context);
void object_set_position(Object* object, Float3 position, VideoContext* context);
Matrix4 object_get_model(Object* object);

#endif // OBJECT_H_
