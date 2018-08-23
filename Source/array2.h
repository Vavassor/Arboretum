// This type of array is based on stretchy_buffer.h by Sean Barrett:
// https://github.com/nothings/stb/blob/master/stretchy_buffer.h
//
// It's an automatically resizing array that can be used generically for any
// type and also allows indexing in the normal way like `array[index]`.
// Instead of having an associated structure, use a pointer to the desired type
// as follows:
//
//    Type* array = NULL;
//    while(has_more_items())
//    {
//        Type item = get_next_item();
//        ARRAY_ADD(array, item, heap);
//    }
//    use_the_array_for_something(array);
//    ARRAY_DESTROY(array, heap);
//
// It works by allocating a bit of extra memory to keep the element count and
// capacity. This *header* is stored first, followed by all of the elements.
//
//     [header][element 0][element 1][element 2][...]
//
// The `array` pointer, though, actually points to `element 0`. So, the header
// is effectively stored before the `array` pointer. Because it's actually
// pointing to an address part of the way into an allocation, it **should not**
// be deallocated. Instead use ARRAY_DESTROY.

#ifndef ARRAY2_H_
#define ARRAY2_H_

#include "memory.h"

#include <stddef.h>

typedef struct ArrayHeader
{
    int count;
    int cap;
    uint8_t elements[0];
} ArrayHeader;

#define ARRAY_ADD(array, element, heap) \
    (ARRAY_FIT_(array, 1, heap), (array)[array_header_(array)->count++] = (element))

#define ARRAY_ADD_STACK(array, element, stack) \
    (ARRAY_FIT_STACK_(array, 1, stack), (array)[array_header_(array)->count++] = (element))

#define ARRAY_REMOVE(array, element) \
    (*element = (array)[--array_header_(array)->count])

#define ARRAY_RESERVE(array, extra, heap) \
    ARRAY_FIT_(array, extra, heap)

#define ARRAY_DESTROY(array, heap) \
    ((array) ? (HEAP_DEALLOCATE(heap, array_header_(array)), (array) = NULL) : 0)

#define ARRAY_DESTROY_STACK(array, stack) \
    ((array) ? (STACK_DEALLOCATE(stack, array_header_(array)), (array) = NULL) : 0)

#define ARRAY_LAST(array) \
    ((array)[array_header_(array)->count - 1])

#define ARRAY_RESIZE_(array, extra, heap) \
    ((array) = resize_array_heap(array, array_count(array) + (extra), sizeof(*(array)), heap))

#define ARRAY_RESIZE_STACK_(array, extra, stack) \
    ((array) = resize_array_stack(array, array_count(array) + (extra), sizeof(*(array)), stack))

#define ARRAY_FIT_(array, extra, heap) \
    (array_fits_(array, extra) ? 0 : ARRAY_RESIZE_(array, extra, heap))

#define ARRAY_FIT_STACK_(array, extra, stack) \
    (array_fits_(array, extra) ? 0 : ARRAY_RESIZE_STACK_(array, extra, stack))

#define FOR_EACH(type, it, array) \
    for(type* it = array; it != (array + array_count(array)); it += 1)

#define FOR_ALL(type, array) \
    FOR_EACH(type, it, array)

int array_count(void* array);
int array_cap(void* array);
void* resize_array_heap(void* array, int count, int element_size, Heap* heap);
void* resize_array_stack(void* array, int count, int element_size, Stack* stack);

ArrayHeader* array_header_(void* array);
bool array_fits_(void* array, int extra);

#endif // ARRAY2_H_
