#include "memory.h"

#include "assert.h"

#if defined(OS_WINDOWS)
#define WINVER        0x0600
#define _WIN32_WINNT  WINVER
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#else
#include <sys/mman.h>
#endif

// General Memory...............................................................

#if defined(OS_WINDOWS)

void* virtual_allocate(uint64_t bytes)
{
    return VirtualAlloc(NULL, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
}

void virtual_deallocate(void* memory)
{
    VirtualFree(memory, 0, MEM_RELEASE);
}

#else

void* virtual_allocate(uint64_t bytes)
{
    void* m = mmap(NULL, bytes + sizeof(bytes), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    uint64_t* p = (uint64_t*) m;
    *p = bytes;
    return p + 1;
}

void virtual_deallocate(void* memory)
{
    uint64_t* p = (uint64_t*) memory;
    p -= 1;
    uint64_t bytes = *p;
    munmap(p, bytes);
}

#endif // defined(OS_WINDOWS)

void set_memory(void* memory, uint8_t value, uint64_t bytes)
{
    for(uint8_t* p = memory; bytes; bytes -= 1, p += 1)
    {
        *p = value;
    }
}

void copy_memory(void* RESTRICT to, const void* RESTRICT from, uint64_t bytes)
{
    const uint8_t* p0 = from;
    uint8_t* p1 = to;
    for(; bytes; bytes -= 1, p0 += 1, p1 += 1)
    {
        *p1 = *p0;
    }
}

void move_memory(void* to, const void* from, uint64_t bytes)
{
    const uint8_t* p0 = from;
    uint8_t* p1 = to;
    if(p0 < p1)
    {
        for(p0 += bytes, p1 += bytes; bytes; bytes -= 1)
        {
            p0 -= 1;
            p1 -= 1;
            *p1 = *p0;
        }
    }
    else
    {
        for(; bytes; bytes -= 1, p0 +=1, p1 += 1)
        {
            *p1 = *p0;
        }
    }
}

uint64_t kilobytes(uint64_t count)
{
    return 1000 * count;
}

uint64_t megabytes(uint64_t count)
{
    return 1000 * kilobytes(count);
}

uint64_t ezlabytes(uint64_t count)
{
    return 256 * count;
}

uint64_t capobytes(uint64_t count)
{
    return 256 * ezlabytes(count);
}

uint64_t uptibytes(uint64_t count)
{
    return 256 * capobytes(count);
}

static bool is_aligned(const void* memory, uint64_t alignment)
{
    uintptr_t address = (uintptr_t) memory;
    return ~(address & (alignment - 1));
}

// Stack........................................................................

void stack_create(Stack* stack, uint32_t bytes)
{
    stack->memory = (uint8_t*) virtual_allocate(bytes);
    stack->top = 0;
    stack->bytes = bytes;
}

void stack_destroy(Stack* stack)
{
    if(stack)
    {
        SAFE_VIRTUAL_DEALLOCATE(stack->memory);
        stack->top = 0;
        stack->bytes = 0;
    }
}

void* stack_allocate(Stack* stack, uint32_t bytes)
{
    uint32_t prior_top = stack->top;
    uint32_t header_size = sizeof(prior_top);
    uint8_t* top = stack->memory + stack->top;

    // ARM Neon's 4 lane 32-bit vector types and Intel AVX-256 packed types ask
    // for 32 byte aligned addresses (This isn't a hard requirement under Neon,
    // but it is faster). It doesn't hurt to just align everything to support
    // these easily.
    //
    // If Intel AVX-512 support is added, this should be bumped up to 64 bytes
    // to align accesses for its 512-bit registers.
    const uint32_t alignment = 32;
    uintptr_t address = (uintptr_t) (top + header_size);
    uint32_t adjustment = alignment - (address & (alignment - 1));
    if(adjustment == alignment)
    {
        adjustment = 0;
    }

    uint32_t total_bytes = adjustment + header_size + bytes;
    if(stack->top + total_bytes > stack->bytes)
    {
        return NULL;
    }
    stack->top += total_bytes;

    top += adjustment;
    *((uint32_t*) top) = prior_top;

    void* result = top + header_size;
    set_memory(result, 0, bytes);
    ASSERT(is_aligned(result, alignment));
    return result;
}

void* stack_reallocate(Stack* stack, void* memory, uint32_t bytes)
{
    if(!memory)
    {
        return stack_allocate(stack, bytes);
    }

    uint8_t* place = (uint8_t*) memory;
    uint32_t present_bytes = (uint32_t) (stack->top - (place - stack->memory));
    uint32_t more_bytes = bytes - present_bytes;
    if(stack->top + more_bytes > stack->bytes)
    {
        return NULL;
    }

    set_memory(&stack->memory[stack->top], 0, more_bytes);
    stack->top += more_bytes;

    return memory;
}

void stack_deallocate(Stack* stack, void* memory)
{
    uint32_t* header = ((uint32_t*) memory) - 1;
    uint32_t prior_top = *header;
    stack->top = prior_top;
}

// Pool.........................................................................

void* pool_iterator_next(PoolIterator* it)
{
    int object_count = it->pool->object_count;
    do
    {
        it->index += 1;
        if(it->index >= object_count)
        {
            return NULL;
        }
    } while(it->pool->statuses[it->index] == POOL_BLOCK_STATUS_FREE);
    return it->pool->memory + (it->pool->object_size * it->index);
}

void* pool_iterator_create(PoolIterator* it, Pool* pool)
{
    it->pool = pool;
    it->index = -1;
    return pool_iterator_next(it);
}

bool pool_create(Pool* pool, uint32_t object_size, uint32_t object_count)
{
    // The free list can't fit in empty slots unless objects are at least as
    // large as a pointer.
    ASSERT(object_size >= sizeof(void*));

    void* memory = virtual_allocate(object_size * object_count);
    PoolBlockStatus* statuses = (PoolBlockStatus*) virtual_allocate(sizeof(PoolBlockStatus) * object_count);
    if(!memory || !statuses)
    {
        return false;
    }

    pool->memory = (uint8_t*) memory;
    pool->object_size = object_size;
    pool->object_count = object_count;
    pool->free_list = (void**) pool->memory;
    pool->statuses = statuses;

    void** p = pool->free_list;
    for(uint32_t i = 0; i < object_count - 1; i += 1)
    {
        *p = ((uint8_t*) p) + object_size;
        p = ((void**) *p);
    }
    *p = NULL;

    for(uint32_t i = 0; i < object_count; i += 1)
    {
        statuses[i] = POOL_BLOCK_STATUS_FREE;
    }

    return true;
}

void pool_destroy(Pool* pool)
{
    if(pool)
    {
        SAFE_VIRTUAL_DEALLOCATE(pool->memory);
        SAFE_VIRTUAL_DEALLOCATE(pool->statuses);
        pool->free_list = NULL;
    }
}

static void mark_block_status(Pool* pool, void* object, PoolBlockStatus status)
{
    uint8_t* offset = ((uint8_t*) object);
    int index = (int) ((offset - pool->memory) / pool->object_size);
    pool->statuses[index] = status;
}

void* pool_allocate(Pool* pool)
{
    if(!pool->free_list)
    {
        ASSERT(false);
        return NULL;
    }
    void* next_free = pool->free_list;
    pool->free_list = ((void**) *pool->free_list);
    mark_block_status(pool, next_free, POOL_BLOCK_STATUS_USED);
    *((void**) next_free) = NULL;
    return next_free;
}

void pool_deallocate(Pool* pool, void* memory)
{
    ASSERT(memory);
    set_memory(memory, 0, pool->object_size);
    *((void**) memory) = pool->free_list;
    pool->free_list = ((void**) memory);
    mark_block_status(pool, memory, POOL_BLOCK_STATUS_FREE);
}

// Heap.........................................................................

#define DO_BEST_FIT

#define NEXT_FREE(index)   heap->blocks[index].body.free.next
#define PREV_FREE(index)   heap->blocks[index].body.free.prior
#define BLOCK_DATA(index)  heap->blocks[index].body.data
#define NEXT_BLOCK(index)  heap->blocks[index].header.used.next
#define PREV_BLOCK(index)  heap->blocks[index].header.used.prior

static const uint32_t freelist_mask = 0x80000000;
static const uint32_t blockno_mask = 0x7fffffff;

void heap_make_in_place(Heap* heap, void* place, uint32_t bytes)
{
    ASSERT(heap);
    ASSERT(place);
    ASSERT(bytes != 0);
    ASSERT(!heap->blocks); // trying to create an already existent heap
    heap->blocks = (HeapBlock*) place;
    heap->total_blocks = bytes / sizeof(HeapBlock);
    NEXT_BLOCK(0) = 1;
    NEXT_FREE(0) = 1;
}

bool heap_create(Heap* heap, uint32_t bytes)
{
    void* memory = virtual_allocate(bytes);
    if(!memory)
    {
        return false;
    }
    heap_make_in_place(heap, memory, bytes);
    return true;
}

void heap_destroy(Heap* heap)
{
    SAFE_VIRTUAL_DEALLOCATE(heap->blocks);
    heap->total_blocks = 0;
}

static uint32_t determine_blocks_needed(uint32_t size)
{
    // When a block removed from the free list, the space used by the free
    // pointers is available for data.
    if(size <= sizeof(HeapBlockBody))
    {
        return 1;
    }
    // If it's for more than that, then we need to figure out the number of
    // additional whole blocks the size of an Heap::Block are required.
    size -= 1 + sizeof(HeapBlockBody);
    return 2 + size / sizeof(HeapBlock);
}

static void disconnect_from_free_list(Heap* heap, uint32_t c)
{
    NEXT_FREE(PREV_FREE(c)) = NEXT_FREE(c);
    PREV_FREE(NEXT_FREE(c)) = PREV_FREE(c);
    NEXT_BLOCK(c) &= ~freelist_mask;
}

static void make_new_block(Heap* heap, uint32_t c, uint32_t blocks, uint32_t freemask)
{
    NEXT_BLOCK(c + blocks) = NEXT_BLOCK(c) & blockno_mask;
    PREV_BLOCK(c + blocks) = c;
    PREV_BLOCK(NEXT_BLOCK(c) & blockno_mask) = c + blocks;
    NEXT_BLOCK(c) = (c + blocks) | freemask;
}

void* heap_allocate(Heap* heap, uint32_t bytes)
{
    ASSERT(heap);
    ASSERT(bytes != 0);

    uint32_t blocks = determine_blocks_needed(bytes);
    uint32_t cf;
    uint32_t block_size = 0;
    {
        uint32_t best_size = 0x7fffffff;
        uint32_t best_block = NEXT_FREE(0);

        for(cf = NEXT_FREE(0); NEXT_FREE(cf); cf = NEXT_FREE(cf))
        {
            block_size = (NEXT_BLOCK(cf) & blockno_mask) - cf;
            if(block_size >= blocks && block_size < best_size)
            {
                best_block = cf;
                best_size = block_size;
            }
        }

        if(best_size != 0x7fffffff)
        {
            cf = best_block;
            block_size = best_size;
        }
    }

    if(NEXT_BLOCK(cf) & blockno_mask)
    {
        // This is an existing block in the memory heap, we just need to split
        // off what we need, unlink it from the free list and mark it as in
        // use, and link the rest of the block back into the freelist as if it
        // was a new block on the free list...
        if(block_size == blocks)
        {
            // It's an exact fit and we don't need to split off a block.
            disconnect_from_free_list(heap, cf);
        }
        else
        {
            // It's not an exact fit and we need to split off a block.
            make_new_block(heap, cf, block_size - blocks, freelist_mask);
            cf += block_size - blocks;
        }
    }
    else
    {
        // We're at the end of the heap - allocate a new block, but check to
        // see if there's enough memory left for the requested block!
        if(heap->total_blocks <= (uint64_t) (cf + blocks + 1))
        {
            return NULL;
        }
        NEXT_FREE(PREV_FREE(cf)) = cf + blocks;
        copy_memory(&heap->blocks[cf + blocks], &heap->blocks[cf], sizeof(HeapBlock));
        NEXT_BLOCK(cf) = cf + blocks;
        PREV_BLOCK(cf + blocks) = cf;
    }

    set_memory(&BLOCK_DATA(cf), 0, bytes);
    return &BLOCK_DATA(cf);
}

static void try_to_assimilate_up(Heap* heap, uint32_t c)
{
    if(NEXT_BLOCK(NEXT_BLOCK(c)) & freelist_mask)
    {
        // The next block is a free block, so assimilate up and remove it from
        // the free list.
        disconnect_from_free_list(heap, NEXT_BLOCK(c));
        // Assimilate the next block with this one
        PREV_BLOCK(NEXT_BLOCK(NEXT_BLOCK(c)) & blockno_mask) = c;
        NEXT_BLOCK(c) = NEXT_BLOCK(NEXT_BLOCK(c)) & blockno_mask;
    }
}

static uint32_t assimilate_down(Heap* heap, uint32_t c, uint32_t freemask)
{
    NEXT_BLOCK(PREV_BLOCK(c)) = NEXT_BLOCK(c) | freemask;
    PREV_BLOCK(NEXT_BLOCK(c)) = PREV_BLOCK(c);
    return PREV_BLOCK(c);
}

static uint32_t index_from_pointer(void* base, void* p, uint32_t size)
{
    return (uint32_t) ((((uintptr_t) p) - ((uintptr_t) base)) / size);
}

void* heap_reallocate(Heap* heap, void* memory, uint32_t bytes)
{
    ASSERT(heap);

    if(!memory)
    {
        return heap_allocate(heap, bytes);
    }
    if(bytes == 0)
    {
        heap_deallocate(heap, memory);
        return NULL;
    }

    // which block we're in
    uint32_t c = index_from_pointer(heap->blocks, memory, sizeof(HeapBlock));

    uint32_t blocks = determine_blocks_needed(bytes);
    uint32_t block_room = NEXT_BLOCK(c) - c;
    uint32_t current_size = sizeof(HeapBlock) * block_room - sizeof(HeapBlockHeader);

    if(block_room == blocks)
    {
        // They had the space needed all along. ;o)
        return memory;
    }

    try_to_assimilate_up(heap, c);

    if((NEXT_BLOCK(PREV_BLOCK(c)) & freelist_mask) && (blocks <= NEXT_BLOCK(c) - PREV_BLOCK(c)))
    {
        disconnect_from_free_list(heap, PREV_BLOCK(c));
        // Connect the previous block to the next block ... and then realign
        // the current block pointer
        c = assimilate_down(heap, c, 0);
        // Move the bytes down to the new block we just created, but be sure to
        // move only the original bytes.
        void* to = &BLOCK_DATA(c);
        copy_memory(to, memory, current_size);
        memory = to;
    }

    block_room = NEXT_BLOCK(c) - c;

    if(block_room == blocks)
    {
        // return the original pointer
        return memory;
    }
    else if(blocks < block_room)
    {
        // New block is smaller than the old block, so just make a new block
        // at the end of this one and put it up on the free list.
        make_new_block(heap, c, blocks, 0);
        heap_deallocate(heap, &BLOCK_DATA(c + blocks));
    }
    else
    {
        // New block is bigger than the old block.
        void* old = memory;
        memory = heap_allocate(heap, bytes);
        if(memory)
        {
            copy_memory(memory, old, current_size);
        }
        heap_deallocate(heap, old);
    }

    return memory;
}

void heap_deallocate(Heap* heap, void* memory)
{
    ASSERT(heap);
    if(!memory)
    {
        return;
    }
    // which block the memory is in
    uint32_t c = index_from_pointer(heap->blocks, memory, sizeof(HeapBlock));

    try_to_assimilate_up(heap, c);

    if (NEXT_BLOCK(PREV_BLOCK(c)) & freelist_mask)
    {
        // assimilate with the previous block if possible
        c = assimilate_down(heap, c, freelist_mask);
    }
    else
    {
        // The previous block is not a free block, so add this one to the head
        // of the free list
        PREV_FREE(NEXT_FREE(0)) = c;
        NEXT_FREE(c) = NEXT_FREE(0);
        PREV_FREE(c) = 0;
        NEXT_FREE(0) = c;
        NEXT_BLOCK(c) |= freelist_mask;
    }
}

HeapInfo heap_get_info(Heap* heap)
{
    HeapInfo info = {0};
    uint32_t blockno = 0;
    for(
        blockno = NEXT_BLOCK(blockno) & blockno_mask;
        NEXT_BLOCK(blockno) & blockno_mask;
        blockno = NEXT_BLOCK(blockno) & blockno_mask)
    {
        info.total_entries += 1;
        info.total_blocks += (NEXT_BLOCK(blockno) & blockno_mask) - blockno;
        if(NEXT_BLOCK(blockno) & freelist_mask)
        {
            info.free_entries += 1;
            info.free_blocks += (NEXT_BLOCK(blockno) & blockno_mask) - blockno;
        }
        else
        {
            info.used_entries += 1;
            info.used_blocks += (NEXT_BLOCK(blockno) & blockno_mask) - blockno;
        }
    }
    info.free_blocks  += heap->total_blocks - blockno;
    info.total_blocks += heap->total_blocks - blockno;
    return info;
}

