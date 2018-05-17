#ifndef ARRAY2_H_
#define ARRAY2_H_

#include "memory.h"
#include "sized_types.h"

#include <cstddef>

struct ArrayHeader
{
    int count;
    int cap;
    u8 elements[0];
};

#define ARRAY_ADD(array, element, heap) \
    (ARRAY_FIT_(array, 1, heap), (array)[array_header_(array)->count++] = (element))

#define ARRAY_ADD_STACK(array, element, stack) \
    (ARRAY_FIT_STACK_(array, 1, stack), (array)[array_header_(array)->count++] = (element))

#define ARRAY_REMOVE(array, element) \
    (*element = (array)[--array_header_(array)->count])

#define ARRAY_RESERVE(array, extra, heap) \
    ARRAY_FIT_(array, extra, heap)

#define ARRAY_DESTROY(array, heap) \
    ((array) ? (HEAP_DEALLOCATE(heap, array_header_(array)), (array) = nullptr) : 0)

#define ARRAY_DESTROY_STACK(array, stack) \
    ((array) ? (STACK_DEALLOCATE(stack, array_header_(array)), (array) = nullptr) : 0)

#define ARRAY_LAST(array) \
    ((array)[array_header_(array)->count - 1])


#define ARRAY_RESIZE_(array, extra, heap) \
    ((array) = static_cast<decltype(array)>(resize_array(array, array_count(array) + (extra), sizeof(*(array)), heap)))

#define ARRAY_RESIZE_STACK_(array, extra, stack) \
    ((array) = static_cast<decltype(array)>(resize_array(array, array_count(array) + (extra), sizeof(*(array)), stack)))

#define ARRAY_FIT_(array, extra, heap) \
    (array_fits_(array, extra) ? 0 : ARRAY_RESIZE_(array, extra, heap))

#define ARRAY_FIT_STACK_(array, extra, stack) \
    (array_fits_(array, extra) ? 0 : ARRAY_RESIZE_STACK_(array, extra, stack))

#define FOR_EACH(it, array) \
    for(auto* it = array; it != (array + array_count(array)); it += 1)

#define FOR_ALL(array) \
    FOR_EACH(it, array)

int array_count(void* array);
int array_cap(void* array);
void* resize_array(void* array, int count, int element_size, Heap* heap);
void* resize_array(void* array, int count, int element_size, Stack* stack);

ArrayHeader* array_header_(void* array);
bool array_fits_(void* array, int extra);

#endif // ARRAY2_H_
