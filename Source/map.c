#include "map.h"

#include "assert.h"
#include "memory.h"

// signifies an empty key slot
static const void* empty = 0;

// signifies that the overflow slot is empty
static const void* overflow_empty = ((void*) 1);

// This value is used to indicate an invalid iterator, or one that's reached the
// end of iteration.
static const int end_index = -1;

static bool is_power_of_two(unsigned int x)
{
    return (x != 0) && !(x & (x - 1));
}

static uint32_t next_power_of_two(uint32_t x)
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

static uint32_t hash_key(uint64_t key)
{
    key = (~key) + (key << 18); // key = (key << 18) - key - 1;
    key = key ^ (key >> 31);
    key = key * 21; // key = (key + (key << 2)) + (key << 4);
    key = key ^ (key >> 11);
    key = key + (key << 6);
    key = key ^ (key >> 22);
    return (uint32_t) key;
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
    map->hashes = HEAP_ALLOCATE(heap, uint32_t, cap);

    int overflow_index = cap;
    map->keys[overflow_index] = (void*) overflow_empty;
    map->values[overflow_index] = NULL;
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
    zero_memory(map->keys, sizeof(*map->keys) * cap);
    zero_memory(map->values, sizeof(*map->values) * cap);
    zero_memory(map->hashes, sizeof(*map->hashes) * cap);

    int overflow_index = cap;
    map->keys[overflow_index] = (void*) overflow_empty;
    map->values[overflow_index] = NULL;

    map->count = 0;
}

static int find_slot(void** keys, int cap, void* key, uint32_t hash)
{
    ASSERT(can_use_bitwise_and_to_cycle(cap));

    int probe = hash & (cap - 1);
    while(keys[probe] != key && keys[probe] != empty)
    {
        probe = (probe + 1) & (cap - 1);
    }

    return probe;
}

MaybePointer map_get(Map* map, void* key)
{
    MaybePointer result = {0};

    if(key == empty)
    {
        int overflow_index = map->cap;
        if(map->keys[overflow_index] == overflow_empty)
        {
            result.valid = false;
        }
        else
        {
            result.value = map->values[overflow_index];
            result.valid = true;
        }
        return result;
    }

    uint32_t hash = hash_key((uint64_t) key);
    int slot = find_slot(map->keys, map->cap, key, hash);

    bool got = map->keys[slot] == key;
    if(got)
    {
        result.value = map->values[slot];
        result.valid = true;
    }
    return result;
}

MaybeUint64 map_get_uint64(Map* map, void* key)
{
    MaybePointer result = map_get(map, key);
    MaybeUint64 result_uint64 =
    {
        .valid = result.valid,
        .value = (uint64_t) result.value,
    };
    return result_uint64;
}

MaybePointer map_get_from_uint64(Map* map, uint64_t key)
{
    return map_get(map, (void*) (uintptr_t) key);
}

MaybeUint64 map_get_uint64_from_uint64(Map* map, uint64_t key)
{
    MaybePointer result = map_get(map, (void*) (uintptr_t) key);
    MaybeUint64 result_uint64 =
    {
        .valid = result.valid,
        .value = (uint64_t) result.value,
    };
    return result_uint64;
}

static void map_grow(Map* map, int cap, Heap* heap)
{
    int prior_cap = map->cap;

    void** keys = HEAP_ALLOCATE(heap, void*, cap + 1);
    void** values = HEAP_ALLOCATE(heap, void*, cap + 1);
    uint32_t* hashes = HEAP_ALLOCATE(heap, uint32_t, cap);

    for(int i = 0; i < prior_cap; i += 1)
    {
        void* key = map->keys[i];
        if(key == empty)
        {
            continue;
        }
        uint32_t hash = map->hashes[i];
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
        if(map->keys[overflow_index] != key)
        {
            map->count += 1;
        }
        return;
    }

    int load_limit = (3 * map->cap) / 4;
    if(map->count >= load_limit)
    {
        map_grow(map, 2 * map->cap, heap);
    }

    uint32_t hash = hash_key((uint64_t) key);
    int slot = find_slot(map->keys, map->cap, key, hash);
    if(map->keys[slot] != key)
    {
        map->count += 1;
    }
    map->keys[slot] = key;
    map->values[slot] = value;
    map->hashes[slot] = hash;
}

void map_add_uint64(Map* map, void* key, uint64_t value, Heap* heap)
{
    map_add(map, key, (void*) (uintptr_t) value, heap);
}

void map_add_from_uint64(Map* map, uint64_t key, void* value, Heap* heap)
{
    map_add(map, (void*) (uintptr_t) key, value, heap);
}

void map_add_uint64_from_uint64(Map* map, uint64_t key, uint64_t value, Heap* heap)
{
    map_add(map, (void*) (uintptr_t) key, (void*) (uintptr_t) value, heap);
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
            map->keys[overflow_index] = (void*) overflow_empty;
            map->values[overflow_index] = NULL;
            map->count -= 1;
        }
        return;
    }

    uint32_t hash = hash_key((uint64_t) key);
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
        map->keys[i] = (void*) empty;
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

void map_remove_uint64(Map* map, uint64_t key)
{
    map_remove(map, (void*) (uintptr_t) key);
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
            if(index == it.map->cap
                    && it.map->keys[it.map->cap] != overflow_empty)
            {
                return (MapIterator){it.map, it.map->cap};
            }
            return (MapIterator){it.map, end_index};
        }
    } while(it.map->keys[index] == empty);

    return (MapIterator){it.map, index};
}

MapIterator map_iterator_start(Map* map)
{
    if(map->count > 0)
    {
        return (MapIterator){map, 0};
    }
    else
    {
        return (MapIterator){map, end_index};
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
