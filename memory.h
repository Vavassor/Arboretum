#ifndef MEMORY_H_
#define MEMORY_H_

#if defined(__cplusplus)
#include "restrict.h"
#define restrict RESTRICT
#endif

#if defined(__cplusplus)
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// General Memory...............................................................

void* virtual_allocate(uint64_t bytes);
void virtual_deallocate(void* memory);
void set_memory(void* memory, uint8_t value, uint64_t bytes);
void copy_memory(void* restrict to, const void* restrict from, uint64_t bytes);
void move_memory(void* to, const void* from, uint64_t bytes);
uint64_t kilobytes(uint64_t count);
uint64_t megabytes(uint64_t count);
uint64_t ezlabytes(uint64_t count);
uint64_t capobytes(uint64_t count);
uint64_t uptibytes(uint64_t count);

#define SAFE_VIRTUAL_DEALLOCATE(memory) \
    if(memory) {virtual_deallocate(memory); (memory) = NULL;}

#define MOVE_ARRAY(to, from, count) \
    move_memory(to, from, (count) * sizeof(*from))

// Stack........................................................................

typedef struct Stack
{
    uint8_t* memory;
    uint32_t top;
    uint32_t bytes;
} Stack;

void stack_create(Stack* stack, uint32_t bytes);
void stack_destroy(Stack* stack);
void* stack_allocate(Stack* stack, uint32_t bytes);
void* stack_reallocate(Stack* stack, void* memory, uint32_t bytes);
void stack_deallocate(Stack* stack, void* memory);

#define STACK_ALLOCATE(stack, type, count) \
    ((type*) stack_allocate(stack, sizeof(type) * (count)))

#define STACK_REALLOCATE(stack, memory, count) \
    ((decltype(memory)) stack_reallocate(stack, memory, sizeof(*(memory)) * (count)))

#define STACK_DEALLOCATE(stack, memory) \
    stack_deallocate(stack, memory)

// Pool.........................................................................

typedef enum PoolBlockStatus
{
    POOL_BLOCK_STATUS_FREE,
    POOL_BLOCK_STATUS_USED,
} PoolBlockStatus;

typedef struct Pool
{
    uint8_t* memory;
    uint32_t object_size;
    uint32_t object_count;
    void** free_list;
    PoolBlockStatus* statuses;
} Pool;

typedef struct PoolIterator
{
    Pool* pool;
    int index;
} PoolIterator;

void* pool_iterator_next(PoolIterator* it);
void* pool_iterator_create(PoolIterator* it, Pool* pool);

#define PASTE2(x, y) x##y
#define PASTE(x, y) PASTE2(x, y)

#define FOR_EACH_IN_POOL(type, object, pool) \
    PoolIterator PASTE(it_, __LINE__); \
    for(type* object = ((type*) pool_iterator_create(&PASTE(it_, __LINE__), &pool)); object; object = ((type*) pool_iterator_next(&PASTE(it_, __LINE__))))

bool pool_create(Pool* pool, uint32_t object_size, uint32_t object_count);
void pool_destroy(Pool* pool);
void* pool_allocate(Pool* pool);
void pool_deallocate(Pool* pool, void* memory);

#define POOL_ALLOCATE(pool, type) \
    ((type*) pool_allocate(pool))

// Heap.........................................................................

typedef struct HeapNode
{
    uint32_t next;
    uint32_t prior;
} HeapNode;

typedef union HeapBlockHeader
{
    HeapNode used;
} HeapBlockHeader;

typedef union HeapBlockBody
{
    HeapNode free;
    uint8_t data[sizeof(HeapNode)];
} HeapBlockBody;

typedef struct HeapBlock
{
    HeapBlockHeader header;
    HeapBlockBody body;
} HeapBlock;

typedef struct Heap
{
    HeapBlock* blocks;
    uint64_t total_blocks;
} Heap;

typedef struct HeapInfo
{
    uint64_t total_entries;
    uint64_t total_blocks;
    uint64_t free_entries;
    uint64_t free_blocks;
    uint64_t used_entries;
    uint64_t used_blocks;
} HeapInfo;

void heap_make_in_place(Heap* heap, void* place, uint32_t bytes);
bool heap_create(Heap* heap, uint32_t bytes);
void heap_destroy(Heap* heap);
void* heap_allocate(Heap* heap, uint32_t bytes);
void* heap_reallocate(Heap* heap, void* memory, uint32_t bytes);
void heap_deallocate(Heap* heap, void* memory);
HeapInfo heap_get_info(Heap* heap);

#define HEAP_ALLOCATE(heap, type, count) \
    ((type*) heap_allocate(heap, sizeof(type) * (count)))

#define HEAP_REALLOCATE(heap, array, count) \
    ((decltype(array)) heap_reallocate(heap, array, sizeof(*(array)) * (count)))

#define HEAP_DEALLOCATE(heap, array) \
    heap_deallocate(heap, array)

#define SAFE_HEAP_DEALLOCATE(heap, array) \
    {heap_deallocate(heap, array); (array) = NULL;}

#if defined(__cplusplus)
} // extern "C"
#endif

#endif // MEMORY_H_
