#ifndef MAP_H_
#define MAP_H_

#include "sized_types.h"

struct Heap;

// This is a hash table that uses pointer-sized values for its key and value
// pairs. It uses open addressing and linear probing for its collision
// resolution.
struct Map
{
    void** keys;
    void** values;
    u32* hashes;
    int cap;
    int count;
};

void map_create(Map* map, int cap, Heap* heap);
void map_destroy(Map* map, Heap* heap);
void map_clear(Map* map);
bool map_get(Map* map, void* key, void** value);
void map_add(Map* map, void* key, void* value, Heap* heap);
void map_remove(Map* map, void* key);
void map_reserve(Map* map, int cap, Heap* heap);

struct MapIterator
{
    Map* map;
    int index;
};

MapIterator map_iterator_next(MapIterator it);
MapIterator map_iterator_start(Map* map);
bool map_iterator_is_not_end(MapIterator it);
void* map_iterator_get_key(MapIterator it);
void* map_iterator_get_value(MapIterator it);

#define ITERATE_MAP(it, map) \
    for(MapIterator it = map_iterator_start(map); map_iterator_is_not_end(it); it = map_iterator_next(it))

#endif // MAP_H_
