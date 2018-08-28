ASCII
=====

ASCII is an encoding for text. It stands for the American Standard Code for
Information Interchange.

Generally, text is encoded in UTF-8, which ASCII is a subset of. In situations
where only the ASCII characters are relevant, it's useful to have procedures
for them.

.. c:function:: int ascii_compare_alphabetic(const char* RESTRICT a, \
        const char* RESTRICT b)

    Compare strings alphabetically.

    :param a: the first string
    :param b: the second string
    :return:
       
        - negative if ``a`` comes first
        - positive if the ``b`` comes first
        - zero if the strings are the same

.. c:function:: int ascii_digit_to_int(char c)

    Read a single digit.

    :param c: the digit
    :return: a number corresponding to the digit, or zero if it's not a digit

.. c:function:: bool ascii_is_alphabetic(char c)

    Check if a character is a letter of the alphabet, either upper or lowercase.

    :param c: the character
    :return: true if the character is alphabetic

.. c:function:: bool ascii_is_alphanumeric(char c)

    Check if a character is a number or a letter, either upper or lowercase.

    :param c: the character
    :return: true if the character is a number or letter

.. c:function:: bool ascii_is_lowercase(char c)

    Check if a character is a lowercase letter.

    :param c: the character
    :return:

        - true if the character is a lowercase letter
        - false if it's uppercase, or not a letter

.. c:function:: bool ascii_is_newline(char c)

    Check if a character is a newline.

    A newline is considered a line-feed ``'\n'`` or carriage return ``'\r'``
    only.

    :param c: the character
    :return: true if it's a newline

.. c:function:: bool ascii_is_numeric(char c)

    Check if a character is a number.

    :param c: the character
    :return: true if it's a number

.. c:function:: bool ascii_is_space_or_tab(char c)

    Check if a character is a space ``' '`` or tab ``'\t'``.

    :param c: the character
    :return: true if it's a space or tab

.. c:function:: bool ascii_is_uppercase(char c)

    Check if a character is an uppercase letter.

    :param c: the character
    :return:

        - true if the character is an uppercase letter
        - false if it's lowercase, or not a letter

.. c:function:: bool ascii_is_whitespace(char c)

    Check if a character is whitespace.

    The characters in this table are all those that count as whitespace.

    ===============  =========  ===========
    Name             C Literal  Hexadecimal
    ===============  =========  ===========
    Carriage Return  ``'\r'``   0d
    Form Feed        ``'\f'``   0c
    Line Feed        ``'\n'``   0a
    Space            ``' '``    20
    Horizontal Tab   ``'\t'``   09
    Vertical Tab     ``'\v'``   0b
    ===============  =========  ===========

    :param c: the character
    :return: true if the character is whitespace

.. c:function:: void ascii_to_lowercase(char* s)

    Convert a string to all lowercase.

    :param s: the string

.. c:function:: char ascii_to_lowercase_char(char c)

    Convert a character to lowercase.

    :param c: the character
    :return: a lowercase character, or if not a letter, the character as-is

.. c:function:: void ascii_to_uppercase(char* s)

    Convert a string to all uppercase.

    :param s: the string

.. c:function:: char ascii_to_uppercase_char(char c)

    Convert a character to uppercase.

    :param c: the character
    :return: an uppercase character, or if not a letter, the character as-is

