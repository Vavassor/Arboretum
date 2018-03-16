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

#define ARRAY_COUNT(array) \
    ((array) ? ARRAY_HEADER_(array)->count : 0)

#define ARRAY_CAP(array) \
    ((array) ? ARRAY_HEADER_(array)->cap : 0)

#define ARRAY_ADD(array, element, heap) \
    (ARRAY_FIT_(array, 1, heap), (array)[ARRAY_HEADER_(array)->count++] = (element))

#define ARRAY_REMOVE(array, element) \
    (*element = (array)[--ARRAY_HEADER_(array)->count])

#define ARRAY_RESERVE(array, extra, heap) // Nothing, for now

#define ARRAY_DESTROY(array, heap) \
    ((array) ? (HEAP_DEALLOCATE(heap, ARRAY_HEADER_(array)), (array) = nullptr) : 0)


#define ARRAY_HEADER_(array) \
    reinterpret_cast<ArrayHeader*>(reinterpret_cast<u8*>(array) - offsetof(ArrayHeader, elements))

#define ARRAY_FITS_(array, extra) \
    (ARRAY_COUNT(array) + (extra) <= ARRAY_CAP(array))

#define ARRAY_RESIZE_(array, extra, heap) \
    ((array) = static_cast<decltype(array)>(resize_array(array, ARRAY_COUNT(array) + (extra), sizeof(*(array)), heap)))

#define ARRAY_FIT_(array, extra, heap) \
    (ARRAY_FITS_(array, extra) ? 0 : ARRAY_RESIZE_(array, extra, heap))


void* resize_array(void* array, int count, int element_size, Heap* heap);

#endif // ARRAY2_H_
