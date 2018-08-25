Memory
============


General Memory
--------------

.. c:function:: uint64_t capobytes(uint64_t count)

    A capobyte is 1,00,00₁₆ or 16⁴ bytes.

    :return: the number of bytes in ``count`` capobytes

.. c:function:: void copy_memory(void* RESTRICT to, const void* RESTRICT from, \
        uint64_t bytes)

    Copy a block of memory from one place to another. The original and new
    locations for the memory must not overlap one another.

    :param to: the destination
    :param from: the memory to copy
    :param bytes: the number of bytes to copy

.. c:function:: uint64_t ezlabytes(uint64_t count)

    An ezlabyte is 1,00₁₆ or 16² bytes.

    :return: the number of bytes in ``count`` ezlabytes

.. c:function:: uint64_t kilobytes(uint64_t count)

    A kilobyte is 1,000 or 10³ bytes.

    :return: the number of bytes in ``count`` kilobytes

.. c:function:: uint64_t megabytes(uint64_t count)

    A megabyte is 1,000,000 or 10⁶ bytes.

    :return: the number of bytes in ``count`` megabytes

.. c:function:: void move_memory(void* to, const void* from, uint64_t bytes)

    Copy a block memory from one place to another. Unlike :c:func:`copy_memory`,
    the original and new locations for the memory are allowed to overlap one
    another.

    :param to: the destination
    :param from: the memory to copy
    :param bytes: the number of bytes to copy

.. c:function:: MOVE_ARRAY(to, from, count)

    This is a convenience macro to use :c:func:`move_memory` on arrays, which
    allows counting in terms of number of elements rather than bytes.

    :param to: a pointer to elements in an array of the same type as ``from``
    :param from: a pointer to elements in an array of any type
    :param count: the number of elements to move

.. c:function:: SAFE_VIRTUAL_DEALLOCATE(memory)

    A wrapper around :c:func:`virtual_deallocate` that prevents double-free
    errors and accidental passing of ``NULL``.

    :param memory: a pointer previously returned from :c:func:`virtual_allocate`
            or ``NULL``

.. c:function:: void set_memory(void* memory, uint8_t value, uint64_t bytes)

    Set each byte of a block of memory to a value.

    :param memory: the memory to set
    :param value: the value to set each byte to
    :param bytes: the number of bytes to set

.. c:function:: uint64_t uptibytes(uint64_t count)

    An uptibyte is 1,00,00,00₁₆ or 16⁶ bytes.

    :return: the number of bytes in ``count`` uptibytes

.. c:function:: void* virtual_allocate(uint64_t bytes)

    Allocate virtual memory pages from the operating system.

    Since memory pages tend to be at least 4 KiB in size, any returned memory
    will effectively be padded to the next largest multiple of the page size.

    :param bytes: the number of bytes desired
    :return: memory at least as large as the amount requested

.. c:function:: void virtual_deallocate(void* memory)

    Deallocate virtual memory pages from the operating system.
    
    :param memory: a pointer previously returned from :c:func:`virtual_allocate`

        This should not be ``NULL`` or another pointer under any circumstance.


Heap
----

.. c:type:: Heap

    A heap allocator. It is not thread-safe. It doesn't automatically resize
    on demand and *can* run out of memory.

.. c:type:: HeapInfo

    Statistics about a heap.

.. c:function:: void* heap_allocate(Heap* heap, uint32_t bytes)

    Allocate memory from within the heap.

    :param heap: the heap
    :param bytes: how many bytes of space to allocate

            This cannot be 0.
    :return: the memory, with all bytes set to 0

.. c:function:: HEAP_ALLOCATE(heap, type, count)

    Allocate an array from within the heap.

    :param Heap* heap: the heap
    :param type: the type of array
    :param int count: number of elements to allocate

            This cannot be 0.
    :return: the array, with all elements cleared to 0

.. c:function:: bool heap_create(Heap* heap, uint32_t bytes)

    Create a heap. This uses :c:func:`virtual_allocate` to get its own memory.

    :param heap: the heap to create
    :param bytes: how many bytes large the heap should be

            This cannot be 0.
    :return: true if the heap was created, false if :c:func:`virtual_allocate`
            failed

.. c:function:: HEAP_DEALLOCATE(heap, array)

    Deallocate an array from within the heap.

    :param Heap* heap: the heap
    :param array: an array previously allocated from the heap, or ``NULL``

.. c:function:: void heap_deallocate(Heap* heap, void* memory)

    Deallocate memory from within the heap.

    :param heap: the heap
    :param memory: memory previously allocated from the heap, or ``NULL``

.. c:function:: void heap_destroy(Heap* heap)

    Destroy a heap.

    :param heap: the heap to destroy

.. c:function:: HeapInfo heap_get_info(Heap* heap)

    Gather information about the current status of the heap.

    :param heap: the heap
    :return: the information

.. c:function:: void heap_make_in_place(Heap* heap, void* place, uint32_t bytes)

    Create a heap in a given area of memory rather than let it allocate its own
    memory to use.
    
    :param heap: the heap to create
    :param place: the area in memory to use
    :param bytes: how many bytes are available at ``place``

.. c:function:: void* heap_reallocate(Heap* heap, void* memory, uint32_t bytes)

    Resize memory to a larger size than initially allocated.

    :param heap: the heap
    :param memory: memory previously allocated from the heap, or ``NULL``
    :param bytes: the new size in bytes to return

            - If this is 0, the memory is deallocated.
            - If it's less than the original size, the original memory is
              returned unchanged.
    :return: memory with the same contents, plus any additional bytes of space
            set to 0

            - If ``memory`` is ``NULL``, return a new allocation.
            - If ``bytes`` is 0, return ``NULL``.

.. c:function:: HEAP_REALLOCATE(heap, array, type, count)

    Resize an array to a larger size than initially allocated.

    :param Heap* heap: the heap
    :param array: an array previously allocated from the heap, or ``NULL``
    :param type: the type of array
    :param int count: the new number of elements for the array

            - If this is 0, the array is deallocated.
            - If this is less than the original amount of elements, the array
              is returned unchanged.
    :return: an array with the same elements, plus any additional elements
            cleared to 0

            - If ``array`` is ``NULL``, return a new array.
            - If ``bytes`` is 0, return ``NULL``.

.. c:function:: SAFE_HEAP_DEALLOCATE(heap, array)

    A wrapper around :c:func:`heap_deallocate` that prevents double-free errors.

    :param Heap* heap: the heap
    :param array: an array previously allocated from the heap, or ``NULL``


Pool
----

.. c:type:: Pool

    A pool is a memory allocator which holds many objects of the same size.
    They can freely be allocated and deallocated in any order.

.. c:function:: FOR_EACH_IN_POOL(type, object, pool)

    Iterate through each object in the pool.

    Use it as follows.
    ::

        FOR_EACH_IN_POOL(JanFace, face, faces)
        {
            move_face(face);
        }

    :param type: the type of object in the pool
    :param object: a name to give the current element each iteration of the loop
    :param pool: the pool

.. c:function:: void* pool_allocate(Pool* pool)

    Allocate one object from the pool.

    :param pool: the pool
    :return: the object, or ``NULL`` if the pool is out of memory

.. c:function:: POOL_ALLOCATE(pool, type)

    Allocate one object from the pool.

    :param Pool* pool: the pool
    :param type: the type of object
    :return: the object, or ``NULL`` if the pool is out of memory

.. c:function:: bool pool_create(Pool* pool, uint32_t object_size, \
        uint32_t object_count)

    Create a pool.

    :param pool: the pool
    :param object_size: the size of each object in bytes

            This must be larger than or equal to ``sizeof(void*)``.
    :param object_count: how many objects to create total

            This cannot be 0.
    :return: true if the pool is created, false otherwise

.. c:function:: void pool_deallocate(Pool* pool, void* memory)

    Deallocate one object from the pool.

    :param pool: the pool
    :param memory: the object to deallocate

        This cannot be ``NULL``.

.. c:function:: void pool_destroy(Pool* pool)

    Destroy a pool.

    :param pool: the pool to destroy, or ``NULL``


Stack
-----

.. c:type:: Stack

    A Stack is a memory allocator where any amount of memory can be taken off
    the top, but must be returned in the reverse order it was taken.

    It's intended for fast temporary variable-sized allocations, or fixed-size
    ones that would be too large for putting on the program stack.

    It also has the limitation that only the top allocation can be resized. So,
    procedures that require multiple resizing structures should use
    :c:type:`Heap`.

.. c:function:: void* stack_allocate(Stack* stack, uint32_t bytes)

    Allocate memory from the top of the stack.

    :param stack: the stack
    :param bytes: how many bytes of space to allocate

            This cannot be 0.
    :return: the memory, with all bytes set to 0

            - If the stack is out of space, return ``NULL``.

.. c:function:: STACK_ALLOCATE(stack, type, count)

    Allocate an array from the top of the stack.

    :param Stack* stack: the stack
    :param type: the type of array
    :param count: how many elements to allocate

            This cannot be 0.
    :return: the array, with all elements cleared to 0

            - If the stack is out of space, return ``NULL``.

.. c:function:: void stack_create(Stack* stack, uint32_t bytes)

    Create a new stack.

    :param stack: the stack to create
    :param bytes: the size of the stack in bytes

.. c:function:: void stack_destroy(Stack* stack)

    Destroy a stack.

    :param stack: the stack to destroy, or ``NULL``

.. c:function:: void stack_deallocate(Stack* stack, void* memory)

    Deallocate memory from the top of a stack.

    :param stack: the stack
    :param memory: the last thing allocated from the stack

.. c:function:: STACK_DEALLOCATE(stack, array)

    :param Stack* stack: the stack
    :param array: the last array allocated from the stack

.. c:function:: stack_reallocate(Stack* stack, void* memory, uint32_t bytes)

    :param stack: the stack
    :param memory: the last thing allocated from the stack, or ``NULL``
    :param bytes: the new size in bytes to return

            - If this is 0, the memory is deallocated.
            - If it's less than the original size, the original memory is
              returned unchanged.
    :return: memory with the same contents, plus any additional bytes of space
            set to 0

            - If ``memory`` is ``NULL``, return a new allocation.
            - If ``bytes`` is 0, or if the stack is out of space, return ``NULL``.
    
.. c:function:: STACK_REALLOCATE(stack, memory, type, count)

    :param Stack* stack: the stack
    :param memory: the last array allocated from the stack, or ``NULL``
    :param type: the type of array
    :param count: the new number of elements for the array

            - If this is 0, the array is deallocated.
            - If this is less than the original amount of elements, the array
              is returned unchanged.
    :return: an array with the same elements, plus any additional elements
            cleared to 0

            - If ``array`` is ``NULL``, return a new array.
            - If ``bytes`` is 0, return ``NULL``.

