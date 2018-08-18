#ifndef OBJECT_LADY_H_
#define OBJECT_LADY_H_

#include "memory.h"
#include "object.h"

typedef struct ObjectLady
{
    Object* objects;
    Object* storage;
    ObjectId seed;
} ObjectLady;

void object_lady_create(ObjectLady* lady, Heap* heap);
void object_lady_destroy(ObjectLady* lady, Heap* heap, VideoContext* context);
Object* object_lady_add_object(ObjectLady* lady, Heap* heap, VideoContext* context);
Object* object_lady_get_object_by_id(ObjectLady* lady, ObjectId id);
void object_lady_store_object(ObjectLady* lady, ObjectId id, Heap* heap);
void object_lady_take_out_of_storage(ObjectLady* lady, ObjectId id, Heap* heap);
void object_lady_remove_from_storage(ObjectLady* lady, ObjectId id, VideoContext* context);

#endif // OBJECT_LADY_H_
