#include "object_lady.h"

#include "array2.h"
#include "memory.h"

void object_lady_create(ObjectLady* lady, Heap* heap)
{
    lady->objects = NULL;
    lady->storage = NULL;
    lady->seed = 1;
}

void object_lady_destroy(ObjectLady* lady, Heap* heap, VideoContext* context)
{
    if(lady)
    {
        FOR_ALL(Object, lady->objects)
        {
            object_destroy(it, context);
        }
        FOR_ALL(Object, lady->storage)
        {
            object_destroy(it, context);
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

Object* object_lady_add_object(ObjectLady* lady, Heap* heap, VideoContext* context)
{
    Object object = {0};
    object.id = generate_object_id(lady);
    object_create(&object, context);

    ARRAY_ADD(lady->objects, object, heap);

    return &ARRAY_LAST(lady->objects);
}

Object* object_lady_get_object_by_id(ObjectLady* lady, ObjectId id)
{
    FOR_ALL(Object, lady->objects)
    {
        if(it->id == id)
        {
            return it;
        }
    }
    return NULL;
}

void object_lady_store_object(ObjectLady* lady, ObjectId id, Heap* heap)
{
    Object* object = object_lady_get_object_by_id(lady, id);
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
    return NULL;
}

void object_lady_take_out_of_storage(ObjectLady* lady, ObjectId id, Heap* heap)
{
    Object* object = get_object_in_storage_by_id(lady, id);
    if(object)
    {
        ARRAY_ADD(lady->objects, *object, heap);
        ARRAY_REMOVE(lady->storage, object);
    }
}

void object_lady_remove_from_storage(ObjectLady* lady, ObjectId id, VideoContext* context)
{
    Object* object = get_object_in_storage_by_id(lady, id);
    if(object)
    {
        object_destroy(object, context);
        ARRAY_REMOVE(lady->storage, object);
    }
}
