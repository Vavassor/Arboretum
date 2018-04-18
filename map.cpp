#include "map.h"

#include "assert.h"
#include "memory.h"

namespace
{
    // signifies an empty key slot
    const void* empty = 0;

    // signifies that the overflow slot is empty
    const void* overflow_empty = reinterpret_cast<void*>(1);

    // This value is used to indicate an invalid iterator, or one that's reached
    // the end of iteration.
    const int end_index = -1;
}

static bool is_power_of_two(unsigned int x)
{
    return (x != 0) && !(x & (x - 1));
}

static u32 next_power_of_two(u32 x)
{
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    return x + 1;
}

// In an expression x % n, if n is a power of two the expression can be
// simplified to x & (n - 1). So, this check is for making sure that
// reduction is legal for a given n.
static bool can_use_bitwise_and_to_cycle(int count)
{
    return is_power_of_two(count);
}

static u32 hash_key(u64 key)
{
    key = (~key) + (key << 18); // key = (key << 18) - key - 1;
    key = key ^ (key >> 31);
    key = key * 21; // key = (key + (key << 2)) + (key << 4);
    key = key ^ (key >> 11);
    key = key + (key << 6);
    key = key ^ (key >> 22);
    return key;
}

void map_create(Map* map, int cap, Heap* heap)
{
    if(cap <= 0)
    {
        cap = 16;
    }
    else
    {
        cap = next_power_of_two(cap);
    }

    map->cap = cap;
    map->count = 0;

    map->keys = HEAP_ALLOCATE(heap, void*, cap + 1);
    map->values = HEAP_ALLOCATE(heap, void*, cap + 1);
    map->hashes = HEAP_ALLOCATE(heap, u32, cap);

    int overflow_index = cap;
    map->keys[overflow_index] = const_cast<void*>(overflow_empty);
    map->values[overflow_index] = nullptr;
}

void map_destroy(Map* map, Heap* heap)
{
    if(map)
    {
        SAFE_HEAP_DEALLOCATE(heap, map->keys);
        SAFE_HEAP_DEALLOCATE(heap, map->values);
        SAFE_HEAP_DEALLOCATE(heap, map->hashes);

        map->cap = 0;
        map->count = 0;
    }
}

void map_clear(Map* map)
{
    int cap = map->cap;
    set_memory(map->keys, 0, sizeof(*map->keys) * cap);
    set_memory(map->values, 0, sizeof(*map->values) * cap);
    set_memory(map->hashes, 0, sizeof(*map->hashes) * cap);

    int overflow_index = cap;
    map->keys[overflow_index] = const_cast<void*>(overflow_empty);
    map->values[overflow_index] = nullptr;

    map->count = 0;
}

static int find_slot(void** keys, int cap, void* key, u32 hash)
{
    ASSERT(can_use_bitwise_and_to_cycle(cap));

    int probe = hash & (cap - 1);
    while(keys[probe] != key && keys[probe] != empty)
    {
        probe = (probe + 1) & (cap - 1);
    }

    return probe;
}

bool map_get(Map* map, void* key, void** value)
{
    if(key == empty)
    {
        int overflow_index = map->cap;
        if(map->keys[overflow_index] == overflow_empty)
        {
            return false;
        }
        else
        {
            *value = map->values[overflow_index];
            return true;
        }
    }

    u32 hash = hash_key(reinterpret_cast<u64>(key));
    int slot = find_slot(map->keys, map->cap, key, hash);

    bool got = map->keys[slot] == key;
    if(got)
    {
        *value = map->values[slot];
    }
    return got;
}

static void map_grow(Map* map, int cap, Heap* heap)
{
    int prior_cap = map->cap;

    void** keys = HEAP_ALLOCATE(heap, void*, cap + 1);
    void** values = HEAP_ALLOCATE(heap, void*, cap + 1);
    u32* hashes = HEAP_ALLOCATE(heap, u32, cap);

    for(int i = 0; i < prior_cap; i += 1)
    {
        void* key = map->keys[i];
        if(key == empty)
        {
            continue;
        }
        u32 hash = map->hashes[i];
        int slot = find_slot(keys, cap, key, hash);
        keys[slot] = key;
        hashes[slot] = hash;
        values[slot] = map->values[i];
    }
    // Copy over the overflow pair.
    keys[cap] = map->keys[prior_cap];
    values[cap] = map->values[prior_cap];

    HEAP_DEALLOCATE(heap, map->keys);
    HEAP_DEALLOCATE(heap, map->values);
    HEAP_DEALLOCATE(heap, map->hashes);
    map->keys = keys;
    map->values = values;
    map->hashes = hashes;
    map->cap = cap;
}

void map_add(Map* map, void* key, void* value, Heap* heap)
{
    if(key == empty)
    {
        int overflow_index = map->cap;
        map->keys[overflow_index] = key;
        map->values[overflow_index] = value;
        map->count += 1;
        return;
    }

    int load_limit = (3 * map->cap) / 4;
    if(map->count >= load_limit)
    {
        map_grow(map, 2 * map->cap, heap);
    }

    u32 hash = hash_key(reinterpret_cast<u64>(key));
    int slot = find_slot(map->keys, map->cap, key, hash);
    map->keys[slot] = key;
    map->values[slot] = value;
    map->hashes[slot] = hash;
    map->count += 1;
}

static bool in_cyclic_interval(int x, int first, int second)
{
    if(second > first)
    {
        return x > first && x <= second;
    }
    else
    {
        return x > first || x <= second;
    }
}

void map_remove(Map* map, void* key)
{
    ASSERT(can_use_bitwise_and_to_cycle(map->cap));

    if(key == empty)
    {
        int overflow_index = map->cap;
        if(map->keys[overflow_index] == key)
        {
            map->keys[overflow_index] = const_cast<void*>(overflow_empty);
            map->values[overflow_index] = nullptr;
            map->count -= 1;
        }
        return;
    }

    u32 hash = hash_key(reinterpret_cast<u64>(key));
    int slot = find_slot(map->keys, map->cap, key, hash);
    if(map->keys[slot] == empty)
    {
        return;
    }

    // Empty the slot, but also shuffle down any stranded pairs. There may
    // have been pairs that slid past their natural hash position and over this
    // slot. And any lookup for that key would hit this now-empty slot and fail
    // to find it. So, look for any such keys and shuffle those pairs down.
    for(int i = slot, j = slot;; i = j)
    {
        map->keys[i] = const_cast<void*>(empty);
        int k;
        do
        {
            j = (j + 1) & (map->cap - 1);
            if(map->keys[j] == empty)
            {
                return;
            }
            k = map->hashes[j];
        } while(in_cyclic_interval(k, i, j));

        map->keys[i] = map->keys[j];
        map->values[i] = map->values[j];
        map->hashes[i] = map->hashes[j];
    }
	map->count -= 1;
}

void map_reserve(Map* map, int cap, Heap* heap)
{
    cap = next_power_of_two(cap);
    if(cap > map->cap)
    {
        map_grow(map, cap, heap);
    }
}

MapIterator map_iterator_next(MapIterator it)
{
    int index = it.index;
    do
    {
        index += 1;
        if(index >= it.map->cap)
        {
            if(index == it.map->cap &&
                it.map->keys[it.map->cap] != overflow_empty)
            {
                return {it.map, it.map->cap};
            }
            return {it.map, end_index};
        }
    } while(it.map->keys[index] == empty);

    return {it.map, index};
}

MapIterator map_iterator_start(Map* map)
{
    if(map->count > 0)
    {
        return {map, 0};
    }
    else
    {
        return {map, end_index};
    }
}

bool map_iterator_is_not_end(MapIterator it)
{
    return it.index != end_index;
}

void* map_iterator_get_key(MapIterator it)
{
    ASSERT(it.index >= 0 && it.index <= it.map->cap);
    return it.map->keys[it.index];
}

void* map_iterator_get_value(MapIterator it)
{
    ASSERT(it.index >= 0 && it.index <= it.map->cap);
    return it.map->values[it.index];
}
