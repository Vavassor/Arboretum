#include "object_lady.h"

#include "memory.h"
#include "loop_macros.h"

void create_object_lady(ObjectLady* lady, Heap* heap)
{
    const int cap = 10;

    lady->objects = HEAP_ALLOCATE(heap, Object, cap);
    lady->storage = HEAP_ALLOCATE(heap, Object, cap);
    lady->objects_cap = cap;
    lady->objects_count = 0;
    lady->storage_cap = cap;
    lady->storage_count = 0;
    lady->seed = 1;
}

void destroy_object_lady(ObjectLady* lady, Heap* heap)
{
    if(lady)
    {
        FOR_N(i, lady->objects_count)
        {
            object_destroy(&lady->objects[i]);
        }
        FOR_N(i, lady->storage_count)
        {
            object_destroy(&lady->storage[i]);
        }
        SAFE_HEAP_DEALLOCATE(heap, lady->objects);
        SAFE_HEAP_DEALLOCATE(heap, lady->storage);
    }
}

static bool reserve(ObjectLady* lady, int space, Heap* heap)
{
    while(space >= lady->objects_cap)
    {
        lady->objects_cap *= 2;
        Object* objects = HEAP_REALLOCATE(heap, Object, lady->objects, lady->objects_cap);
        if(!objects)
        {
            return false;
        }
        lady->objects = objects;
    }
    return true;
}

static bool reserve_storage_space(ObjectLady* lady, int space, Heap* heap)
{
    while(space >= lady->storage_cap)
    {
        lady->storage_cap *= 2;
        Object* storage = HEAP_REALLOCATE(heap, Object, lady->storage, lady->storage_cap);
        if(!storage)
        {
            return false;
        }
        lady->storage = storage;
    }
    return true;
}

static ObjectId generate_object_id(ObjectLady* lady)
{
    ObjectId id = lady->seed;
    lady->seed += 1;
    return id;
}

Object* add_object(ObjectLady* lady, Heap* heap)
{
    Object* object = &lady->objects[lady->objects_count];
    object->id = generate_object_id(lady);
    object_create(object);

    reserve(lady, lady->objects_count + 1, heap);
    lady->objects_count += 1;

    return object;
}

static void add_to_storage(ObjectLady* lady, Object* object, Heap* heap)
{
    Object* slot = &lady->storage[lady->storage_count];
    *slot = *object;

    reserve_storage_space(lady, lady->storage_count + 1, heap);
    lady->storage_count += 1;
}

Object* get_object_by_id(ObjectLady* lady, ObjectId id)
{
    FOR_N(i, lady->objects_count)
    {
        Object* object = &lady->objects[i];
        if(object->id == id)
        {
            return object;
        }
    }
    return nullptr;
}

void store_object(ObjectLady* lady, ObjectId id, Heap* heap)
{
    Object* object = get_object_by_id(lady, id);
    if(object)
    {
        add_to_storage(lady, object, heap);
        if(lady->objects_count > 1)
        {
            *object = lady->objects[lady->objects_count - 1];
        }
        lady->objects_count -= 1;
    }
}

static Object* get_object_in_storage_by_id(ObjectLady* lady, ObjectId id)
{
    FOR_N(i, lady->storage_count)
    {
        Object* object = &lady->storage[i];
        if(object->id == id)
        {
            return object;
        }
    }
    return nullptr;
}

void take_out_of_storage(ObjectLady* lady, ObjectId id, Heap* heap)
{
    Object* object = get_object_in_storage_by_id(lady, id);
    if(object)
    {
        // Copy from the storage slot back into the objects array.
        lady->objects[lady->objects_count] = *object;
        reserve(lady, lady->objects_count + 1, heap);
        lady->objects_count += 1;

        // Overwrite the slot in storage.
        if(lady->storage_count > 1)
        {
            *object = lady->storage[lady->storage_count - 1];
        }
        lady->storage_count -= 1;
    }
}

void remove_from_storage(ObjectLady* lady, ObjectId id)
{
    Object* object = get_object_in_storage_by_id(lady, id);
    if(object)
    {
        object_destroy(object);
        if(lady->storage_count > 1)
        {
            *object = lady->storage[lady->storage_count - 1];
        }
        lady->storage_count -= 1;
    }
}
