Array2
============

This type of array is based on `stretchy_buffer.h
<https://github.com/nothings/stb/blob/master/stretchy_buffer.h>`_
by Sean Barrett.

It's an automatically resizing array that can be used generically for any type
and also allows indexing in the normal way like ``array[index]``. Instead of
having an associated structure, use a pointer to the desired type.
::

    Type* array = NULL;
    while(has_more_items())
    {
        Type item = get_next_item();
        ARRAY_ADD(array, item, heap);
    }
    use_the_array_for_something(array);
    ARRAY_DESTROY(array, heap);

It works by allocating a bit of extra memory to keep the element count and
capacity. This *header* is stored first, followed by all of the elements.
::

    [header][element 0][element 1][element 2][...]

The ``array`` pointer, though, actually points to ``element 0``. So, the header
is effectively stored before the ``array`` pointer. Because it's actually
pointing to an address part of the way into an allocation, it **should not**
be deallocated. Instead use :c:func:`ARRAY_DESTROY` or
:c:func:`ARRAY_DESTROY_STACK`.

.. c:function:: ARRAY_ADD(array, element, heap)
    
    Add an element to the end of an existing array, or create an array to add
    it to.

    :param array: an array of any type, or ``NULL``
    :param element: an item to add at the end of the array
    :param Heap* heap: to allocate memory for the array when it's first created
             and when automatically resizing it

.. c:function:: ARRAY_ADD_STACK(array, element, stack)

    Add an element to the end of an existing array, or create an array to add
    it to.

    :param array: an array of any type, or ``NULL``
    :param element: an item to add at the end of the array
    :param Stack* stack: to allocate memory for the array when it's first
             created and when automatically resizing it

.. c:function:: int array_cap(void* array)

    :param array: an array or ``NULL``
    :return: the amount of space available in number of elements, or 0 if the
             array is ``NULL``

.. c:function:: int array_count(void* array)

    :param array: an array or ``NULL``
    :return: the current number of elements, or 0 if the array is ``NULL``

.. c:function:: ARRAY_DESTROY(array, heap)

    Destroy the array.

    :param array: an array of any type, or ``NULL``
    :param Heap* heap: the same heap used when adding or reserving

.. c:function:: ARRAY_DESTROY_STACK(array, stack)

    Destroy the array.

    :param array: an array of any type, or ``NULL``
    :param Stack* stack: the same stack used when adding or reserving

.. c:function:: ARRAY_LAST(array)

    Get the final element in the array.

    :param array: an array of any type
    :return: the last element

.. c:function:: ARRAY_REMOVE(array, element)

    Remove an element from the array and swap the end element into the vacated
    slot, if there is another element left.

    This does not shrink the array's cap.

    :param array: an array of any type
    :param element: a pointer to an element in the array

.. c:function:: ARRAY_RESERVE(array, extra, heap)

    Manually increase the cap for the array by a fixed amount, rather than
    let allocation occur automatically as elements are added. If the array
    is ``NULL``, create an array with ``extra`` elements.

    :param array: an array of any type, or ``NULL``
    :param int extra: the amount to increase the cap by
    :param Heap* heap: to allocate the extra reserved space

.. c:function:: FOR_EACH(type, it, array)
    
    Loop over the elements of an array.

    It can be used as follows.
    ::

        FOR_EACH(int, value, numbers)
        {
            calculate_something(*value);
        }

    :param type: the type of array
    :param it: a name for the current element each iteration of the loop
    :param array: the array to iterate

.. c:function:: FOR_ALL(type, array)

    Loop over the elements of an array.

    It can be used as follows.
    ::

        FOR_ALL(int, numbers)
        {
            calculate(*it);
        }

    Unlike :c:func:`FOR_EACH`, this can't be nested because the current element
    in both loops would be called ``it``.

    :param type: the type of array
    :param array: the array to iterate

