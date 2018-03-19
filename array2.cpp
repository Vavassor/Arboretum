#include "array2.h"

#include "assert.h"
#include "int_utilities.h"
#include "memory.h"

void* resize_array(void* array, int count, int element_size, Heap* heap)
{
    int cap = MAX(1 + 2 * ARRAY_CAP(array), count);
    int bytes = offsetof(ArrayHeader, elements) + (element_size * cap);

    ArrayHeader* header;
    if(!array)
    {
        header = static_cast<ArrayHeader*>(heap_allocate(heap, bytes));
        header->count = 0;
    }
    else
    {
        header = static_cast<ArrayHeader*>(heap_reallocate(heap, ARRAY_HEADER_(array), bytes));
    }
    header->cap = cap;
    ASSERT(count <= cap);

    return header->elements;
}

void* resize_array(void* array, int count, int element_size, Stack* stack)
{
    int cap = MAX(1 + 2 * ARRAY_CAP(array), count);
    int bytes = offsetof(ArrayHeader, elements) + (element_size * cap);

    ArrayHeader* header;
    if(!array)
    {
        header = static_cast<ArrayHeader*>(stack_allocate(stack, bytes));
        header->count = 0;
    }
    else
    {
        header = static_cast<ArrayHeader*>(stack_reallocate(stack, ARRAY_HEADER_(array), bytes));
    }
    header->cap = cap;
    ASSERT(count <= cap);

    return header->elements;
}
