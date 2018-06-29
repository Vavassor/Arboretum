#include "object_lady.h"

#include "array2.h"
#include "memory.h"

void create_object_lady(ObjectLady* lady, Heap* heap)
{
    lady->objects = nullptr;
    lady->storage = nullptr;
    lady->seed = 1;
}

void destroy_object_lady(ObjectLady* lady, Heap* heap)
{
    if(lady)
    {
        FOR_ALL(Object, lady->objects)
        {
            object_destroy(it);
        }
        FOR_ALL(Object, lady->storage)
        {
            object_destroy(it);
        }
        ARRAY_DESTROY(lady->objects, heap);
        ARRAY_DESTROY(lady->storage, heap);
    }
}

static ObjectId generate_object_id(ObjectLady* lady)
{
    ObjectId id = lady->seed;
    lady->seed += 1;
    return id;
}

Object* add_object(ObjectLady* lady, Heap* heap)
{
    Object object = {};
    object.id = generate_object_id(lady);
    object_create(&object);

    ARRAY_ADD(lady->objects, object, heap);

    return &ARRAY_LAST(lady->objects);
}

Object* get_object_by_id(ObjectLady* lady, ObjectId id)
{
    FOR_ALL(Object, lady->objects)
    {
        if(it->id == id)
        {
            return it;
        }
    }
    return nullptr;
}

void store_object(ObjectLady* lady, ObjectId id, Heap* heap)
{
    Object* object = get_object_by_id(lady, id);
    if(object)
    {
        ARRAY_ADD(lady->storage, *object, heap);
        ARRAY_REMOVE(lady->objects, object);
    }
}

static Object* get_object_in_storage_by_id(ObjectLady* lady, ObjectId id)
{
    FOR_ALL(Object, lady->storage)
    {
        if(it->id == id)
        {
            return it;
        }
    }
    return nullptr;
}

void take_out_of_storage(ObjectLady* lady, ObjectId id, Heap* heap)
{
    Object* object = get_object_in_storage_by_id(lady, id);
    if(object)
    {
        ARRAY_ADD(lady->objects, *object, heap);
        ARRAY_REMOVE(lady->storage, object);
    }
}

void remove_from_storage(ObjectLady* lady, ObjectId id)
{
    Object* object = get_object_in_storage_by_id(lady, id);
    if(object)
    {
        object_destroy(object);
        ARRAY_REMOVE(lady->storage, object);
    }
}
