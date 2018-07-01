#ifndef OBJECT_LADY_H_
#define OBJECT_LADY_H_

#if defined(__cplusplus)
extern "C" {
#endif

#include "memory.h"
#include "object.h"

typedef struct ObjectLady
{
    Object* objects;
    Object* storage;
    ObjectId seed;
} ObjectLady;

void object_lady_create(ObjectLady* lady, Heap* heap);
void object_lady_destroy(ObjectLady* lady, Heap* heap);
Object* object_lady_add_object(ObjectLady* lady, Heap* heap);
Object* object_lady_get_object_by_id(ObjectLady* lady, ObjectId id);
void object_lady_store_object(ObjectLady* lady, ObjectId id, Heap* heap);
void object_lady_take_out_of_storage(ObjectLady* lady, ObjectId id, Heap* heap);
void object_lady_remove_from_storage(ObjectLady* lady, ObjectId id);

#if defined(__cplusplus)
} // extern "C"
#endif

#endif // OBJECT_LADY_H_
