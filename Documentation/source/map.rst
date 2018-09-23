Map
===

.. c:type:: Map

    This is a hash table that uses pointer-sized values for its key and value
    pairs. It uses open addressing and linear probing for its collision
    resolution.

.. c:function:: void map_add(Map* map, void* key, void* value, Heap* heap)
        void map_add_uint64(Map* map, void* key, uint64_t value, Heap* heap)
        void map_add_from_uint64(Map* map, uint64_t key, void* value, \
                Heap* heap)
        void map_add_uint64_from_uint64(Map* map, uint64_t key, \
                uint64_t value, Heap* heap)

    Map the value to the key.

    :param map: the map
    :param key: the key
    :param value: the value
    :param heap: the heap to use to expand the map if it doesn't have enough
            space to store the pair

.. c:function:: void map_clear(Map* map)

    Remove all pairs from the map.

    :param map: the map

.. c:function:: void map_create(Map* map, int cap, Heap* heap)

    Create a map.

    :param map: the map
    :param cap: the initial cap on the number of elements

            If it's smaller than 16, it will default to 16 instead.
    :param heap: a heap to create the map from

.. c:function:: void map_destroy(Map* map, Heap* heap)

    Destroy a map.

    :param map: the map, or ``NULL``
    :param heap: the heap that the map was created using

.. c:function:: MaybePointer map_get(Map* map, void* key)
        MaybeUint64 map_get_uint64(Map* map, void* key)
        MaybePointer map_get_from_uint64(Map* map, uint64_t key)
        MaybeUint64 map_get_uint64_from_uint64(Map* map, uint64_t key)

    Get the value mapped to the given key.

    :param map: the map
    :param key: the key
    :return: a value

.. c:function:: void map_remove(Map* map, void* key)
        void map_remove_uint64(Map* map, uint64_t key)

    Remove a pair corresponding to the given key. If no pair is found, do
    nothing.

    :param map: the map
    :param key: the key to remove

.. c:function:: void map_reserve(Map* map, int cap, Heap* heap)

    Resize the array to at least as large as the given cap.

    :param map: the map
    :param cap: the desired cap
    :param heap: the heap to use to expand the map


MapIterator
-----------

.. c:type:: MapIterator

    This is an iterator used to walk a :c:type:`Map`.

.. c:function:: ITERATE_MAP(it, map)

    Iterate over each element in the map.
    ::

        ITERATE_MAP(it, edge_map)
        {
            Edge* edge = (Edge*) map_iterator_get_value(it);
            attach_edge(edge);
        }

    :param it: a name to give the :c:type:`MapIterator`
    :param Map* map: the map

.. c:function:: void* map_iterator_get_key(MapIterator it)

    :param it: the iterator
    :return: the key in the map where the iterator is positioned

.. c:function:: void* map_iterator_get_value(MapIterator it)

    :param it: the iterator
    :return: the value in the map where the iterator is positioned

.. c:function:: bool map_iterator_is_not_end(MapIterator it)

    :param it: the iterator
    :return: true if the iterator is not at the end

.. c:function:: MapIterator map_iterator_next(MapIterator it)

    Get an iterator at the next element in the map.

    :param it: the current iterator
    :return: an iterator at the next element in the map

.. c:function:: MapIterator map_iterator_start(Map* map)

    Get an iterator at the start of the map.

    :param map: the map to iterate
    :return: an iterator at the start of the map, or at the end if the map is
            empty

