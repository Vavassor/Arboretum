#include "atr.h"

#include "filesystem.h"


// ASCII-encoded text "DATA", in little-endian order
#define CHUNK_ID_DATA 0x41544144

// ASCII-encoded text "INDX", in little-endian order
#define CHUNK_ID_INDEX 0x58444e49

// ASCII-encoded text "FORM", in little-endian order
#define CHUNK_ID_FORMAT 0x4d524f46

#define CRC_TABLE_CAP 256


#pragma pack(push, atr, 1)

typedef struct ChunkHeader
{
    union
    {
        uint32_t id;
        char id_ascii[4];
    };
    uint32_t size;
} ChunkHeader;

typedef struct FileHeader
{
    union
    {
        uint64_t signature;
        char signature_ascii[8];
    };
    uint32_t checksum;
    uint16_t version;
} FileHeader;

typedef struct FormatChunk
{
    uint32_t default_value;
    char32_t high_end;
} FormatChunk;

#pragma pack(pop, atr)

typedef struct CrcTable
{
    uint32_t values[CRC_TABLE_CAP];
} CrcTable;

typedef struct Loader
{
    CrcTable crc_table;
    File* file;
    Heap* heap;
    UnicodeTrie* trie;
    uint32_t checksum;
    uint32_t header_checksum;
} Loader;


// ASCII-encoded text "ARBOTRIE", in little-endian order
static const uint64_t atr_signature = 0x454952544f425241;

static const uint16_t atr_version = 0;


static void crc_table_set_up(CrcTable* table)
{
    for(int value_index = 0; value_index < CRC_TABLE_CAP; value_index += 1)
    {
        uint32_t x = value_index;
        for(int iteration = 0; iteration < 8; iteration += 1)
        {
            uint32_t y = (x & 1) ? 0 : UINT32_C(0xedb88320);
            x = y ^ x >> 1;
        }
        table->values[value_index] = x ^ UINT32_C(0xff000000);
    }
}

static void crc32(CrcTable* table, const void* data, int bytes,
        uint32_t* running_code)
{
    uint32_t code = *running_code;
    uint8_t* data_bytes = (uint8_t*) data;
    for(int data_index = 0; data_index < bytes; data_index += 1)
    {
        uint8_t value_index = (uint8_t) code ^ data_bytes[data_index];
        code = table->values[value_index] ^ code >> 8;
    }
    *running_code = code;
}

static bool load_header(Loader* loader)
{
    FileHeader header = {0};
    uint64_t header_size = sizeof(header);
    uint64_t bytes_read = 0;
    bool read = read_file(loader->file, &header, header_size, &bytes_read);
    if(!read || bytes_read != header_size)
    {
        return false;
    }

    if(header.signature != atr_signature)
    {
        return false;
    }

    if(header.version != atr_version)
    {
        return false;
    }

    int rest_of_header = sizeof(FileHeader) - offsetof(FileHeader, version);
    crc32(&loader->crc_table, &header.version, rest_of_header,
            &loader->checksum);

    loader->header_checksum = header.checksum;

    return true;
}

static bool load_data_chunk(Loader* loader, uint8_t* chunk_data,
        uint32_t chunk_size)
{
    uint32_t* data = (uint32_t*) chunk_data;

    int data_count = chunk_size / sizeof(uint32_t);

    UnicodeTrie* trie = loader->trie;
    trie->data = HEAP_ALLOCATE(loader->heap, uint32_t, data_count + 8);
    COPY_ARRAY(trie->data, data, data_count);

    return true;
}

static bool load_format_chunk(Loader* loader, uint8_t* chunk_data,
        uint32_t chunk_size)
{
    if(chunk_size != sizeof(FormatChunk))
    {
        return false;
    }

    FormatChunk* chunk = (FormatChunk*) chunk_data;
    UnicodeTrie* trie = loader->trie;
    trie->default_value = chunk->default_value;
    trie->high_end = chunk->high_end;

    return true;
}

static bool load_index_chunk(Loader* loader, uint8_t* chunk_data,
        uint32_t chunk_size)
{
    uint16_t* indices = (uint16_t*) chunk_data;

    int indices_count = chunk_size / sizeof(uint16_t);

    UnicodeTrie* trie = loader->trie;
    trie->indices = HEAP_ALLOCATE(loader->heap, uint16_t, indices_count + 8);
    COPY_ARRAY(trie->indices, indices, indices_count);

    return true;
}

static bool load_chunks(Loader* loader)
{
    uint8_t* chunk_data = NULL;
    bool success = true;

    while(success)
    {
        ChunkHeader header = {0};
        uint64_t header_size = sizeof(header);
        uint64_t bytes_read = 0;
        bool read = read_file(loader->file, &header, header_size, &bytes_read);
        if(!read || bytes_read != header_size)
        {
            break;
        }

        crc32(&loader->crc_table, &header, sizeof(header), &loader->checksum);

        uint64_t chunk_size = header.size;
        chunk_data = HEAP_REALLOCATE(loader->heap, chunk_data, uint8_t,
                chunk_size);
        read = read_file(loader->file, chunk_data, chunk_size, &bytes_read);
        if(!read || bytes_read != chunk_size)
        {
            success = false;
            break;
        }

        crc32(&loader->crc_table, chunk_data, header.size, &loader->checksum);

        switch(header.id)
        {
            case CHUNK_ID_DATA:
            {
                success = load_data_chunk(loader, chunk_data, header.size);
                break;
            }
            case CHUNK_ID_FORMAT:
            {
                success = load_format_chunk(loader, chunk_data, header.size);
                break;
            }
            case CHUNK_ID_INDEX:
            {
                success = load_index_chunk(loader, chunk_data, header.size);
                break;
            }
        }
    }

    HEAP_DEALLOCATE(loader->heap, chunk_data);

    return success;
}

bool atr_load_file(UnicodeTrie* trie, const char* path, Heap* heap)
{
    zero_memory(trie, sizeof(*trie));

    File* file = open_file(path, FILE_OPEN_MODE_READ, heap);
    if(!file)
    {
        return false;
    }

    Loader loader =
    {
        .file = file,
        .heap = heap,
        .trie = trie,
        .checksum = 0xffffffff,
    };
    crc_table_set_up(&loader.crc_table);

    bool success = load_header(&loader);
    if(success)
    {
        success = load_chunks(&loader);

        if(success && loader.checksum != loader.header_checksum)
        {
            success = false;
        }
    }

    close_file(file);

    if(!success)
    {
        unicode_trie_destroy(trie, heap);
    }

    return success;
}
