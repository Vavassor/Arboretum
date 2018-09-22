#ifndef MAP_H_
#define MAP_H_

#include "memory.h"

#include <stdint.h>

// This is a hash table that uses pointer-sized values for its key and value
// pairs. It uses open addressing and linear probing for its collision
// resolution.
typedef struct Map
{
    void** keys;
    void** values;
    uint32_t* hashes;
    int cap;
    int count;
} Map;

typedef struct MapResult
{
    union
    {
        void* value_void;
        uint64_t value_uint64;
    };
    bool found;
} MapResult;

void map_create(Map* map, int cap, Heap* heap);
void map_destroy(Map* map, Heap* heap);
void map_clear(Map* map);
MapResult map_get(Map* map, void* key);
MapResult map_get_from_uint64(Map* map, uint64_t key);
void map_add(Map* map, void* key, void* value, Heap* heap);
void map_add_uint64(Map* map, void* key, uint64_t value, Heap* heap);
void map_add_from_uint64(Map* map, uint64_t key, void* value, Heap* heap);
void map_add_uint64_from_uint64(Map* map, uint64_t key, uint64_t value, Heap* heap);
void map_remove(Map* map, void* key);
void map_remove_uint64(Map* map, uint64_t key);
void map_reserve(Map* map, int cap, Heap* heap);

typedef struct MapIterator
{
    Map* map;
    int index;
} MapIterator;

MapIterator map_iterator_next(MapIterator it);
MapIterator map_iterator_start(Map* map);
bool map_iterator_is_not_end(MapIterator it);
void* map_iterator_get_key(MapIterator it);
void* map_iterator_get_value(MapIterator it);

#define ITERATE_MAP(it, map) \
    for(MapIterator it = map_iterator_start(map); map_iterator_is_not_end(it); it = map_iterator_next(it))

#endif // MAP_H_
