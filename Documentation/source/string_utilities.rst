String Utilities
================

.. c:type:: ConvertedInt

    This is an integer converted from a string.

    .. c:member:: char* after

        a pointer to the character after the integer in the string

    .. c:member:: int value
        
        the integer itself

    .. c:member:: bool valid

        true if an integer was read successfully

.. c:function:: const char* bool_to_string(bool b)

    Produce a string representing a boolean value.

    :param b: the bool
    :return: ``"true"`` or ``"false"``

.. c:function:: int copy_string(char* RESTRICT to, int to_size, \
        const char* RESTRICT from)

    Copy a string to an array. This copies either the whole string or the first
    ``to_size - 1`` bytes and a null terminator.

    :param to: the array to store the given string
    :param to_size: the size of the array available
    :param from: the string to copy
    :return: the number of bytes copied

.. c:function:: int count_char_occurrences(const char* string, char c)

    Count how many times a byte occurs in a string.

    :param string: the string
    :param c: the byte
    :return: a count

.. c:function:: int count_substring_occurrences(const char* string, \
        const char* pattern)

    Count the number of times a substring appears in a string.

    A substring only counts if it's an exact, case-sensitive match.

    :param string: the string
    :param pattern: the substring
    :return: a count

.. c:function:: int find_char(const char* s, char c)

    Find the first occurence of a byte in a string.

    :param s: the string
    :param c: the byte
    :return: the index of the byte, or :c:data:`invalid_index` if it isn't found

.. c:function:: int find_last_char(const char* s, char c)

    Find the last occurence of a byte in a string.

    :param s: the string
    :param c: the byte
    :return: the index of the byte, or :c:data:`invalid_index` if it isn't found

.. c:function:: char* find_string(const char* RESTRICT a, \
        const char* RESTRICT b)

    Find a substring in a string. This looks for an exact, case-sensitive match.

    :param a: the string to search
    :param b: the substring to look for
    :return: a pointer to the first byte of the substring or ``NULL`` if it
            wasn't found

.. c:function:: void float_to_string(char* string, int size, float value, \
        unsigned int precision)

    Write a real number to a string in decimal.

    :param string: the array to write the string

            Write whichever is shorter of scientific notation form or decimal
            form. If there isn't enough room in the array, the first
            ``size - 1`` characters will be written.

            - infinity will produce ``"infinity"``
            - Not-a-Number will produce ``"NaN"``
    :param size: the size of the array
    :param value: the real number
    :param precision: the number of decimal places of precision

.. c:function:: void format_string(char* buffer, int buffer_size, \
        const char* format, ...)

    Format a string using a given format and arguments.

    :param buffer: the array to write to

            If the array isn't big enough, only the first ``buffer_size - 1``
            bytes are written, plus the null terminator.
    :param buffer_size: the array size
    :param format: the format string

            See :ref:`section-format-string` for an in-depth description of the
            format string language.
    :param ...: a variable number of arguments, depending on the sequences in
            the format string

.. c:function:: int int_to_string(char* string, int size, int value)

    Write an integer to a string in decimal.

    :param string: the array to write the string
    :param size: the size of the array
    :param value: the integer
    :return: how many bytes were written

.. c:function:: bool only_control_characters(const char* string)

    Check if a string contains only control characters.

    :param string: the string
    :return: true if there's only control characters in the string

.. c:function:: void replace_chars(char* s, char original, char replacement)

    Find each occurence of a byte in a string and replace it with another byte.

    :param s: the string
    :param original: the byte to search for
    :param replacement: the byte to replace it with

.. c:function:: bool string_ends_with(const char* RESTRICT a, \
        const char* RESTRICT b)

    Check if a string ends with a substring.

    :param a: the string
    :param b: the substring
    :return: true if the string ends with the substring

.. c:function:: int string_size(const char* string)

    Find the size of a string in bytes.

    If the string is encoded in ASCII, it will be the number of characters in
    the string, but if it's UTF-8 instead use :c:func:`utf8_codepoint_count`.

    :param string: the string
    :return: a size in bytes

.. c:function:: bool string_starts_with(const char* RESTRICT a, \
        const char* RESTRICT b)

    Check if a string starts with a substring.

    :param a: the string
    :param b: the substring
    :return: true if the string starts with the substring

.. c:function:: MaybeDouble string_to_double(const char* string)

    Read a number string and produce the real number it represents.

    :param string: the string
    :return: the real number

.. c:function:: MaybeFloat string_to_float(const char* string)

    Read a number string and produce the real number it represents.

    :param string: the string
    :return: the real number

.. c:function:: MaybeInt string_to_int(const char* string)

    Read a number string and produce the integer it represents.

    :param string: the string, with digits in base 10
    :return: the integer

.. c:function:: ConvertedInt string_to_int_extra(const char* string, int base)

    Read a number string and produce the integer it represents.

    :param string: the string
    :param base: the base of the integer to read

            If this is zero, look for an prefix to indicate the base.

            - ``0x`` for hexadecimal
            - ``0`` for octal
            - nothing for decimal
    :return: the integer

.. c:function:: bool strings_match(const char* RESTRICT a, \
        const char* RESTRICT b)

    Check if two strings match.

    This is an exact byte-for-byte comparison and is case sensitive.

    :param a: the first string
    :param b: the second string
    :return: true if they contain the same bytes

.. c:function:: void va_list_format_string(char* buffer, int buffer_size, \
        const char* format, va_list* arguments)

    Format a string using a given format and arguments.

    :param buffer: the array to write to

            If the array isn't big enough, only the first ``buffer_size - 1``
            bytes are written, plus the null terminator.
    :param buffer_size: the array size
    :param format: the format string

            See :ref:`section-format-string` for an in-depth description of the
            format string language.
    :param arguments: a variable-argument list of arguments corresponding to
            sequences in the format string


.. _section-format-string:

Format String
-------------

The format string is a template string that may contain subsequences called
*specifiers*, which mark where arguments are placed. A specifier also may
contain formatting information for the argument.

A specifier has the form::

    %[flags][width][.precision][length]specifier

The initial percent sign and the single-character specifier type are the only
mandatory components. The others, if present, must appear in the order above.

The specifier type indicates the base type or the use of one argument.

.. table:: Specifier Table

    =========  =================================================================
    Specifier  Result
    =========  =================================================================
    %          Output an actual percent sign %.
    ---------  -----------------------------------------------------------------
    a          a lowercase hexadecimal floating point number
    ---------  -----------------------------------------------------------------
    A          an uppercase hexadecimal floating point number
    ---------  -----------------------------------------------------------------
    c          a character
    ---------  -----------------------------------------------------------------
    d          a signed decimal integer
    ---------  -----------------------------------------------------------------
    e          a floating point number in scientific notation form, in lowercase
    ---------  -----------------------------------------------------------------
    E          a floating point number in scientific notation form, in uppercase
    ---------  -----------------------------------------------------------------
    f          a floating point number in decimal form, in lowercase
    ---------  -----------------------------------------------------------------
    F          a floating point number in decimal form, in uppercase
    ---------  -----------------------------------------------------------------
    g          a floating point number in the shorter of scientific notation or
               decimal forms, in lowercase
    ---------  -----------------------------------------------------------------
    G          a floating point number in the shorter of scientific notation or
               decimal forms, in uppercase
    ---------  -----------------------------------------------------------------
    i          a signed decimal integer
    ---------  -----------------------------------------------------------------
    n          Do not output the argument. Instead, take the argument as an int
               pointer and store in it the number of characters formatted so
               far.
    ---------  -----------------------------------------------------------------
    o          an unsigned octal integer
    ---------  -----------------------------------------------------------------
    p          a pointer address
    ---------  -----------------------------------------------------------------
    s          a string
    ---------  -----------------------------------------------------------------
    x          an unsigned hexadecimal integer, in lowercase
    ---------  -----------------------------------------------------------------
    X          an unsigned hexadecimal integer, in uppercase
    =========  =================================================================

.. table:: Flags Table

    =======  ===================================================================
    Flags    Description
    =======  ===================================================================
    (space)  Insert a blank space in front of positive numbers.
    -------  -------------------------------------------------------------------
    #        For specifiers o, x, or X, force nonzero values to have a 0, 0x, or
             0X prefix, respectively. For specifiers a, A, e, E, f, F, g, or G,
             force a decimal separator to be output.
    -------  -------------------------------------------------------------------
    \+       Force positive numbers to display their sign.
    -------  -------------------------------------------------------------------
    \-       Left-justify within the field width. Ignore when no width is given.
    -------  -------------------------------------------------------------------
    0        Left-pad with zeroes instead of spaces when the width is specified.
    =======  ===================================================================

.. table:: Width Table

    ========  ==================================================================
    Width     Description
    ========  ==================================================================
    \*        Take the argument before the one to be formatted and use it as
              the width, instead of specifying it directly in the string.
    --------  ------------------------------------------------------------------
    (number)  Format at least this many characters. If the value is shorter, add
              padding spaces to right-align it, by default. The 0 and - flags
              modify this behaviour.
    ========  ==================================================================

.. table:: Precision Table

    =========  =================================================================
    Precision  Description
    =========  =================================================================
    .*         Take the argument before the one to be formatted and use it as
               the precision, instead of specifying it directly in the string.
    ---------  -----------------------------------------------------------------
    .(number)  For specifiers a, A, e, E, f and F, format this many digits after
               the decimal point.

               For specifiers d, i, o, u, x, and X, format at least this many
               digits. If the value is shorter, add leading zeroes until it's
               this many digits.

               For specifiers g and G, format this many significant figures.

               For specifier s, format at most this many characters.

               Given no number, assume a 0.
    =========  =================================================================

.. table:: Length Table

    ======  ======  ======  =========  =========  ==========  =====  ========
    Length  a, A,   c       d, i       o, u, x,   n           p      s
            e, E,                      X
            f, F,
            g, G
    ======  ======  ======  =========  =========  ==========  =====  ========
    (none)  double  int     int        unsigned   int*        void*  char*
                                       int
    ------  ------  ------  ---------  ---------  ----------  -----  --------
    hh                      char       unsigned   char*
                                       char
    ------  ------  ------  ---------  ---------  ----------  -----  --------
    h                       short      unsigned   short*
                                       short
    ------  ------  ------  ---------  ---------  ----------  -----  --------
    j                       intmax_t   uintmax_t  intmax_t*
    ------  ------  ------  ---------  ---------  ----------  -----  --------
    l               wint_t  long       unsigned   long*              wchar_t*
                                       long
    ------  ------  ------  ---------  ---------  ----------  -----  --------
    ll                      long long  unsigned   long long
                            int        long long  int*
                                       int
    ------  ------  ------  ---------  ---------  ----------  -----  --------
    L       long
            double
    ------  ------  ------  ---------  ---------  ----------  -----  --------
    t                       ptrdiff_t  ptrdiff_t  ptrdiff_t*
    ------  ------  ------  ---------  ---------  ----------  -----  --------
    z                       size_t     size_t     size_t*
    ======  ======  ======  =========  =========  ==========  =====  ========

An empty cell indicates that the default, the type in the top row, will be used.

