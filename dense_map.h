#ifndef DENSE_MAP_H_
#define DENSE_MAP_H_

#include "map.h"

#include <stdint.h>

struct Heap;

typedef uint32_t DenseMapId;

namespace video {

struct Object;

struct DenseMap
{
    Map id_map;
    Map index_map;
    video::Object* array;
    DenseMapId seed;
};

void create(DenseMap* map, Heap* heap);
void destroy(DenseMap* map, Heap* heap);
Object* look_up(DenseMap* map, DenseMapId id);
DenseMapId add(DenseMap* map, Heap* heap);
void remove(DenseMap* map, DenseMapId id, Heap* heap);

} // namespace video

#endif // DENSE_MAP_H_
