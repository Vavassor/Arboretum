ATR
====

Version 0

ATR stands for an Arboretum Trie. It's a binary file format for storing Unicode
property tables.

Encoding
--------

All integer values are stored in little-endian byte order and all text is
`UTF-8 <https://en.wikipedia.org/wiki/UTF-8>`_. The exception is individual
codepoints, which are `UTF-32 <https://en.wikipedia.org/wiki/UTF-32>`_ without
a byte order mark, because they're implicitly little-endian.

Layout
------

The file starts with a fixed header followed by a variable number of chunks.
Each chunk has an 8-byte header with an ID and the size of the chunk.

Any chunks with unrecognized IDs should be checksummed and otherwise ignored.
This is to maintain forward compatibility if new chunk types are added that the
reader doesn't recognize.

Required Sections
^^^^^^^^^^^^^^^^^

#. :ref:`file-header`
#. :ref:`format-chunk`
#. :ref:`index-chunk`
#. :ref:`data-chunk`


.. _file-header:

.. table:: File Header

    =====  =========  ==========================================================
    Bytes  Name       Description
    =====  =========  ==========================================================
    8      Signature  the `ASCII <https://en.wikipedia.org/wiki/ASCII>`_
                      characters ``ARBOTRIE``
    -----  ---------  ----------------------------------------------------------
    4      Checksum   This is a
                      `CRC-32 <https://en.wikipedia.org/wiki/CRC-32>`_
                      checksum including every byte in the file after this.
                      It uses all-ones-bits or ffffffff₁₆ as the initial value
                      to compute the code.
    -----  ---------  ----------------------------------------------------------
    2      Version    the version number of the file format
    =====  =========  ==========================================================

The header must be the first thing in the file.

.. _format-chunk:

.. table:: Format Chunk

    =====  =============  ======================================================
    Bytes  Name           Description
    =====  =============  ======================================================
    4      ID             the `ASCII <https://en.wikipedia.org/wiki/ASCII>`_
                          characters ``FORM``
    -----  -------------  ------------------------------------------------------
    4      Chunk Size     the number of bytes after this in the chunk
    -----  -------------  ------------------------------------------------------
    4      Default Value  the property value to use for codepoints not in the
                          table
    -----  -------------  ------------------------------------------------------
    4      High End       the highest codepoint specified in the table, in
                          `UTF-32 <https://en.wikipedia.org/wiki/UTF-32>`_
    =====  =============  ======================================================

The format chunk must be before any index or data chunk. This is because it
describes the format of index and data tables and they couldn't be interpreted
sensibly without determining that information, first.

.. _index-chunk:

.. table:: Index Chunk

    ==========  ==========  ====================================================
    Bytes       Name        Description
    ==========  ==========  ====================================================
    4           ID          the `ASCII <https://en.wikipedia.org/wiki/ASCII>`_
                            characters ``INDX``
    ----------  ----------  ----------------------------------------------------
    4           Chunk Size  the number of bytes after this in the chunk
    ----------  ----------  ----------------------------------------------------
    Chunk Size  Indices     the index table, where each index is an unsigned
                            16-bit integer
    ==========  ==========  ====================================================

The index chunk contains the index table, which refers to locations in the data
table.

.. _data-chunk:

.. table:: Data Chunk

    ==========  ==========  ====================================================
    Bytes       Name        Description
    ==========  ==========  ====================================================
    4           ID          the `ASCII <https://en.wikipedia.org/wiki/ASCII>`_
                            characters ``DATA``
    ----------  ----------  ----------------------------------------------------
    4           Chunk Size  the number of bytes after this in the chunk
    ----------  ----------  ----------------------------------------------------
    Chunk Size  Data        the property value table, where each value is an
                            unsigned 32-bit integer
    ==========  ==========  ====================================================

The data chunk contains the data table, which is the actual property values.

