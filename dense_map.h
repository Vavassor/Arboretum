#ifndef DENSE_MAP_H_
#define DENSE_MAP_H_

#include "map.h"
#include "memory.h"

#include <stdint.h>

typedef uint32_t DenseMapId;

typedef struct DenseMap
{
    Map id_map;
    Map index_map;
    struct VideoObject* array;
    DenseMapId seed;
} DenseMap;

void dense_map_create(DenseMap* map, Heap* heap);
void dense_map_destroy(DenseMap* map, Heap* heap);
struct VideoObject* dense_map_look_up(DenseMap* map, DenseMapId id);
DenseMapId dense_map_add(DenseMap* map, Heap* heap);
void dense_map_remove(DenseMap* map, DenseMapId id, Heap* heap);

#endif // DENSE_MAP_H_
