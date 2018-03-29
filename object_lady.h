#ifndef OBJECT_LADY_H_
#define OBJECT_LADY_H_

#include "object.h"

struct ObjectLady
{
    Object* objects;
    Object* storage;
    ObjectId seed;
};

struct Heap;

void create_object_lady(ObjectLady* lady, Heap* heap);
void destroy_object_lady(ObjectLady* lady, Heap* heap);
Object* add_object(ObjectLady* lady, Heap* heap);
Object* get_object_by_id(ObjectLady* lady, ObjectId id);
void store_object(ObjectLady* lady, ObjectId id, Heap* heap);
void take_out_of_storage(ObjectLady* lady, ObjectId id, Heap* heap);
void remove_from_storage(ObjectLady* lady, ObjectId id);

#endif // OBJECT_LADY_H_
