#include "dense_map.h"

#include "array2.h"
#include "assert.h"
#include "memory.h"
#include "video_object.h"

namespace video {

void create(DenseMap* map, Heap* heap)
{
    const int cap = 1024;

    map->array = nullptr;
    map_create(&map->id_map, cap, heap);
    map_create(&map->index_map, cap, heap);
    map->seed = 1;
}

void destroy(DenseMap* map, Heap* heap)
{
    if(map)
    {
        ARRAY_DESTROY(map->array, heap);
        map_destroy(&map->id_map, heap);
        map_destroy(&map->index_map, heap);
    }
}

static DenseMapId generate_id(DenseMap* map)
{
    DenseMapId id = map->seed;
    map->seed += 1;
    return id;
}

static void add_pair(DenseMap* map, DenseMapId id, int index, Heap* heap)
{
    MAP_ADD(&map->id_map, index, id, heap);
    MAP_ADD(&map->index_map, id, index, heap);
}

DenseMapId add(DenseMap* map, Heap* heap)
{
    int index = ARRAY_COUNT(map->array);
    DenseMapId id = generate_id(map);

    Object nobody = {};
    ARRAY_ADD(map->array, nobody, heap);
    add_pair(map, id, index, heap);

    return id;
}

static int look_up_index(DenseMap* map, DenseMapId id)
{
    void* key = reinterpret_cast<void*>(id);
    void* value;
    bool got = map_get(&map->index_map, key, &value);
    ASSERT(got);
    return reinterpret_cast<uintptr_t>(value);
}

static DenseMapId look_up_id(DenseMap* map, int index)
{
    void* key = reinterpret_cast<void*>(index);
    void* value;
    bool got = map_get(&map->id_map, key, &value);
    ASSERT(got);
    return reinterpret_cast<uintptr_t>(value);
}

Object* look_up(DenseMap* map, DenseMapId id)
{
    int index = look_up_index(map, id);
    return &map->array[index];
}

static void remove_pair(DenseMap* map, DenseMapId id, int index)
{
    MAP_REMOVE(&map->id_map, index);
    MAP_REMOVE(&map->index_map, id);
}

void remove(DenseMap* map, DenseMapId id, Heap* heap)
{
    int index = look_up_index(map, id);
    Object* object = &map->array[index];
    ARRAY_REMOVE(map->array, object);

    remove_pair(map, id, index);

    int moved_index = ARRAY_COUNT(map->array);
    if(moved_index != index)
    {
        DenseMapId moved_id = look_up_id(map, moved_index);
        remove_pair(map, moved_id, moved_index);
        add_pair(map, moved_id, index, heap);
    }
}

} // namespace video
