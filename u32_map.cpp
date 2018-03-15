#include "u32_map.h"

#include "assert.h"
#include "int_utilities.h"
#include "loop_macros.h"
#include "memory.h"

namespace
{
    const u32 invalid_key = 0xffffffff;
}

// a public domain 4-byte hash function by Bob Jenkins, adapted from a
// multiplicative method by Thomas Wang, to do it 6-shifts
// http://burtleburtle.net/bob/hash/integer.html
static u32 hash_bj6(u32 a)
{
    a = (a + 0x7ed55d16) + (a << 12);
    a = (a ^ 0xc761c23c) ^ (a >> 19);
    a = (a + 0x165667b1) + (a << 5);
    a = (a + 0xd3a2646c) ^ (a << 9);
    a = (a + 0xfd7046c5) + (a << 3);
    a = (a ^ 0xb55a4f09) ^ (a >> 16);
    return a;
}

static int hash_key(u32 key, int n)
{
    return hash_bj6(key) & (n - 1);
}

void create(U32Map* map, int cap, Heap* heap)
{
    cap = next_power_of_two(cap);

    map->keys = HEAP_ALLOCATE(heap, u32, cap);
    map->values = HEAP_ALLOCATE(heap, u32, cap);
    map->cap = cap;

    reset_all(map);
}

void destroy(U32Map* map, Heap* heap)
{
    if(map)
    {
        SAFE_HEAP_DEALLOCATE(heap, map->keys);
        SAFE_HEAP_DEALLOCATE(heap, map->values);
    }
}

void reset_all(U32Map* map)
{
    FOR_N(i, map->cap)
    {
        map->keys[i] = invalid_key;
    }
}

void insert(U32Map* map, u32 key, u32 value)
{
    ASSERT(key != invalid_key);
    ASSERT(can_use_bitwise_and_to_cycle(map->cap));

    int probe = hash_key(key, map->cap);
    while(map->keys[probe] != invalid_key)
    {
        probe = (probe + 1) & (map->cap - 1);
    }
    map->keys[probe] = key;
    map->values[probe] = value;
}

bool look_up(U32Map* map, u32 key, u32* value)
{
    ASSERT(can_use_bitwise_and_to_cycle(map->cap));

    int probe = hash_key(key, map->cap);
    while(map->keys[probe] != key)
    {
        if(map->keys[probe] == invalid_key)
        {
            return false;
        }
        probe = (probe + 1) & (map->cap - 1);
    }

    *value = map->values[probe];
    return true;
}
