// The ID Pool is a data structure meant to be used together with an array of
// the same size. The ID pool manages which indices of the array are used. Users
// are expected to use IDs returned by the pool to refer to the array's elements
// instead of pointers. This allows the array to be resized or reallocated
// without invalidating their references to the elements.

#ifndef ID_POOL_H_
#define ID_POOL_H_

#include "memory.h"

#include <stdint.h>

typedef struct IdPool
{
    int* free_slots;
    int cap;
    int top;
    uint32_t counter;
} IdPool;

extern const uint32_t invalid_id;

uint32_t make_id(uint32_t number, uint32_t slot);
uint32_t get_id_slot(uint32_t id);

void create_id_pool(IdPool* pool, Heap* heap, int cap);
void destroy_id_pool(IdPool* pool, Heap* heap);
uint32_t allocate_id(IdPool* pool);
void deallocate_id(IdPool* pool, uint32_t id);

#endif // ID_POOL_H_
