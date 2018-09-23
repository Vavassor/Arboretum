#include "dense_map.h"

#include "array2.h"
#include "assert.h"
#include "video_object.h"

void dense_map_create(DenseMap* map, Heap* heap)
{
    const int cap = 1024;

    map->array = NULL;
    map_create(&map->id_map, cap, heap);
    map_create(&map->index_map, cap, heap);
    map->seed = 1;
}

void dense_map_destroy(DenseMap* map, Heap* heap)
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
    map_add_uint64_from_uint64(&map->id_map, index, id, heap);
    map_add_uint64_from_uint64(&map->index_map, id, index, heap);
}

DenseMapId dense_map_add(DenseMap* map, Heap* heap)
{
    int index = array_count(map->array);
    DenseMapId id = generate_id(map);

    VideoObject nobody = {0};
    ARRAY_ADD(map->array, nobody, heap);
    add_pair(map, id, index, heap);

    return id;
}

static int look_up_index(DenseMap* map, DenseMapId id)
{
    void* key = (void*) (uintptr_t) id;
    MapResult result = map_get(&map->index_map, key);
    ASSERT(result.found);
    return (int) (uintptr_t) result.value;
}

static DenseMapId look_up_id(DenseMap* map, int index)
{
    void* key = (void*) (uintptr_t) index;
    MapResult result = map_get(&map->id_map, key);
    ASSERT(result.found);
    return (DenseMapId) (uintptr_t) result.value;
}

VideoObject* dense_map_look_up(DenseMap* map, DenseMapId id)
{
    int index = look_up_index(map, id);
    return &map->array[index];
}

static void remove_pair(DenseMap* map, DenseMapId id, int index)
{
    map_remove_uint64(&map->id_map, index);
    map_remove_uint64(&map->index_map, id);
}

void dense_map_remove(DenseMap* map, DenseMapId id, Heap* heap)
{
    int index = look_up_index(map, id);
    VideoObject* object = &map->array[index];
    ARRAY_REMOVE(map->array, object);

    remove_pair(map, id, index);

    int moved_index = array_count(map->array);
    if(moved_index != index)
    {
        DenseMapId moved_id = look_up_id(map, moved_index);
        remove_pair(map, moved_id, moved_index);
        add_pair(map, moved_id, index, heap);
    }
}
