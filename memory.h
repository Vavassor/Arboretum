#ifndef MEMORY_H_
#define MEMORY_H_

#include "sized_types.h"
#include "restrict.h"

// General Memory...............................................................

void* virtual_allocate(u64 bytes);
void virtual_deallocate(void* memory);
void set_memory(void* memory, u8 value, u64 bytes);
void copy_memory(void* RESTRICT to, const void* RESTRICT from, u64 bytes);
void move_memory(void* RESTRICT to, const void* RESTRICT from, u64 bytes);

#define SAFE_VIRTUAL_DEALLOCATE(memory)\
    if(memory) {virtual_deallocate(memory); (memory) = nullptr;}

#define KIBIBYTES(x) (1024 * (x))
#define MEBIBYTES(x) (1024 * KIBIBYTES(x))

// Stack........................................................................

struct Stack
{
    u8* memory;
    u32 top;
    u32 bytes;
};

void stack_create(Stack* stack, u32 bytes);
void stack_destroy(Stack* stack);
void* stack_allocate(Stack* stack, u32 bytes);
void* stack_reallocate(Stack* stack, void* memory, u32 bytes);
void stack_deallocate(Stack* stack, void* memory);

#define STACK_ALLOCATE(stack, type, count)\
    static_cast<type*>(stack_allocate(stack, sizeof(type) * (count)))

#define STACK_REALLOCATE(stack, type, memory, count)\
    static_cast<type*>(stack_reallocate(stack, memory, sizeof(type) * (count)))

#define STACK_DEALLOCATE(stack, memory)\
    stack_deallocate(stack, memory)

// Pool.........................................................................

enum class PoolBlockStatus
{
    Free,
    Used,
};

struct Pool
{
    u8* memory;
    u32 object_size;
    u32 object_count;
    void** free_list;
    PoolBlockStatus* statuses;
};

struct PoolIterator
{
    Pool* pool;
    int index;
};

void* pool_iterator_next(PoolIterator* it);
void* pool_iterator_create(PoolIterator* it, Pool* pool);

#define PASTE2(x, y) x##y
#define PASTE(x, y) PASTE2(x, y)

#define FOR_EACH_IN_POOL(type, object, pool)\
    PoolIterator PASTE(it_, __LINE__);\
    for(type* object = static_cast<type*>(pool_iterator_create(&PASTE(it_, __LINE__), &pool)); object; object = static_cast<type*>(pool_iterator_next(&PASTE(it_, __LINE__))))

bool pool_create(Pool* pool, u32 object_size, u32 object_count);
void pool_destroy(Pool* pool);
void* pool_allocate(Pool* pool);
void pool_deallocate(Pool* pool, void* memory);

#define POOL_ALLOCATE(pool, type)\
    static_cast<type*>(pool_allocate(pool))

// Heap.........................................................................

struct Heap
{
    struct Node
    {
        u32 next;
        u32 prior;
    };

    struct Block
    {
        union Header
        {
            Node used;
        } header;

        union Body
        {
            Node free;
            u8 data[sizeof(Node)];
        } body;
    };

    Block* blocks;
    u64 total_blocks;
};

struct HeapInfo
{
    u64 total_entries;
    u64 total_blocks;
    u64 free_entries;
    u64 free_blocks;
    u64 used_entries;
    u64 used_blocks;
};

void heap_make_in_place(Heap* heap, void* place, u32 bytes);
bool heap_create(Heap* heap, u32 bytes);
void heap_destroy(Heap* heap);
void* heap_allocate(Heap* heap, u32 bytes);
void* heap_reallocate(Heap* heap, void* memory, u32 bytes);
void heap_deallocate(Heap* heap, void* memory);
HeapInfo heap_get_info(Heap* heap);

#define HEAP_ALLOCATE(heap, type, count)\
    static_cast<type*>(heap_allocate(heap, sizeof(type) * (count)))

#define HEAP_REALLOCATE(heap, array, count)\
    static_cast<decltype(array)>(heap_reallocate(heap, array, sizeof(*(array)) * (count)))

#define HEAP_DEALLOCATE(heap, array)\
    heap_deallocate(heap, array)

#define SAFE_HEAP_DEALLOCATE(heap, array)\
    {heap_deallocate(heap, array); (array) = nullptr;}

#endif // MEMORY_H_
