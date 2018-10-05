Maybe Types
===========

Maybe types, also known as option types, are values that can be either present
or not. These are useful for representing a value that might not exist separate
from designating a special value like -1 or 0 to mean it's invalid or null.

.. c:type:: MaybeDouble

    An optional type for a double precision floating-point number.

    .. c:member:: double value

        the number

    .. c:member:: bool valid

        true if the number is valid

.. c:type:: MaybeFloat

    An optional type for a single precision floating-point number.

    .. c:member:: float value

        the number

    .. c:member:: bool valid

        true if the number is valid

.. c:type:: MaybeInt

    An optional type for an integer.

    .. c:member:: int value

        the integer

    .. c:member:: bool valid

        true if the integer is valid

.. c:type:: MaybePointer

    An optional type for a pointer. It's allowed to be null and still exist.

    .. c:member:: void* value

        the pointer

    .. c:member:: bool valid

        true if the pointer is valid

.. c:type:: MaybeUint64

    An optional type for a 64-bit unsigned integer.

    .. c:member:: uint64_t value

        the integer

    .. c:member:: bool valid
    
        true if the integer is valid

