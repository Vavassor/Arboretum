#include "array2.h"

#include "assert.h"
#include "int_utilities.h"

int array_count(void* array)
{
    return (array) ? array_header_(array)->count : 0;
}

int array_cap(void* array)
{
    return (array) ? array_header_(array)->cap : 0;
}

void* resize_array_heap(void* array, int count, int element_size, Heap* heap)
{
    int cap = imax(1 + 2 * array_cap(array), count);
    int bytes = offsetof(ArrayHeader, elements) + (element_size * cap);

    ArrayHeader* header;
    if(!array)
    {
        header = (ArrayHeader*) heap_allocate(heap, bytes);
        header->count = 0;
    }
    else
    {
        header = (ArrayHeader*) heap_reallocate(heap, array_header_(array), bytes);
    }
    header->cap = cap;
    ASSERT(count <= cap);

    return header->elements;
}

void* resize_array_stack(void* array, int count, int element_size, Stack* stack)
{
    int cap = imax(1 + 2 * array_cap(array), count);
    int bytes = offsetof(ArrayHeader, elements) + (element_size * cap);

    ArrayHeader* header;
    if(!array)
    {
        header = (ArrayHeader*) stack_allocate(stack, bytes);
        header->count = 0;
    }
    else
    {
        header = (ArrayHeader*) stack_reallocate(stack, array_header_(array), bytes);
    }
    header->cap = cap;
    ASSERT(count <= cap);

    return header->elements;
}

ArrayHeader* array_header_(void* array)
{
    return (ArrayHeader*) (((uint8_t*) array) - offsetof(ArrayHeader, elements));
}

bool array_fits_(void* array, int extra)
{
    return array_count(array) + (extra) <= array_cap(array);
}
