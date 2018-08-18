#include "id_pool.h"

const uint32_t invalid_id = 0;

uint32_t make_id(uint32_t number, uint32_t slot)
{
    return (number << 16) | slot;
}

uint32_t get_id_slot(uint32_t id)
{
    return id & 0xffff;
}

void create_id_pool(IdPool* pool, Heap* heap, int cap)
{
    pool->cap = cap + 1;
    pool->top = 0;
    pool->counter = 0;
    pool->free_slots = HEAP_ALLOCATE(heap, int, cap);

    for(int i = pool->cap - 1; i >= 1; i -= 1)
    {
        pool->free_slots[pool->top] = i;
        pool->top += 1;
    }
}

void destroy_id_pool(IdPool* pool, Heap* heap)
{
    SAFE_HEAP_DEALLOCATE(heap, pool->free_slots);
    pool->cap = 0;
    pool->top = 0;
    pool->counter = 0;
}

uint32_t allocate_id(IdPool* pool)
{
    if(pool->top > 0)
    {
        pool->top -= 1;
        int slot_index = pool->free_slots[pool->top];
        uint32_t id = make_id(pool->counter, slot_index);
        pool->counter += 1;
        return id;
    }
    else
    {
        return invalid_id;
    }
}

void deallocate_id(IdPool* pool, uint32_t id)
{
    pool->free_slots[pool->top] = get_id_slot(id);
    pool->top += 1;
}
