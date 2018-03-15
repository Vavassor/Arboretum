#ifndef U32_MAP_H_
#define U32_MAP_H_

#include "sized_types.h"

struct Heap;

struct U32Map
{
    u32* keys;
    u32* values;
    int cap;
};

void create(U32Map* map, int cap, Heap* heap);
void destroy(U32Map* map, Heap* heap);
void reset_all(U32Map* map);
void insert(U32Map* map, u32 key, u32 value);
bool look_up(U32Map* map, u32 key, u32* value);

#endif // U32_MAP_H_
