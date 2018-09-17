#include "bmp.h"

#include "filesystem.h"
#include "memory.h"

#pragma pack(push, bmp, 1)

typedef struct BmpFileHeader
{
    char type[2];
    uint32_t size;
    uint16_t reserved1;
    uint16_t reserved2;
    uint32_t offset;
} BmpFileHeader;

typedef struct BmpInfoHeader
{
    uint32_t size;
    int32_t width;
    int32_t height;
    uint16_t planes;
    uint16_t bits_per_pixel;
    uint32_t compression;
    uint32_t image_size;
    int32_t pixels_per_meter_x;
    int32_t pixels_per_meter_y;
    uint32_t colours_used;
    uint32_t important_colours;
} BmpInfoHeader;

#pragma pack(pop, bmp)

typedef enum Compression
{
    COMPRESSION_NONE = 0,
} Compression;

static unsigned int pad_multiple_of_four(unsigned int x)
{
    return (x + 3) & ~0x3;
}

bool bmp_write_file(const char* path, const uint8_t* pixels, int width, int height, Stack* stack)
{
    unsigned int bytes_per_pixel = 3;
    unsigned int padded_width = pad_multiple_of_four(width);
    unsigned int pixel_data_size = bytes_per_pixel * padded_width * height;
    int row_padding = padded_width - width;

    BmpInfoHeader info;
    info.size = sizeof(info);
    info.width = width;
    info.height = height; // negative indicates rows are ordered top-to-bottom
    info.planes = 1;
    info.bits_per_pixel = 8 * bytes_per_pixel;
    info.compression = COMPRESSION_NONE;
    info.image_size = width * height * info.bits_per_pixel;
    info.pixels_per_meter_x = 0;
    info.pixels_per_meter_y = 0;
    info.colours_used = 0;
    info.important_colours = 0;

    BmpFileHeader header;
    header.type[0] = 'B';
    header.type[1] = 'M';
    header.size = sizeof(header) + sizeof(info) + pixel_data_size;
    header.reserved1 = 0;
    header.reserved2 = 0;
    header.offset = sizeof(header) + sizeof(info);

    // Create the file in-memory;
    uint64_t file_size = header.size;
    uint8_t* file_contents = (uint8_t*) virtual_allocate(file_size);
    uint8_t* hand = file_contents;

    copy_memory(hand, &header, sizeof(header));
    hand += sizeof(header);
    copy_memory(hand, &info, sizeof(info));
    hand += sizeof(info);

    for(int i = 0; i < height; ++i)
    {
        uint64_t row_size = bytes_per_pixel * width;
        copy_memory(hand, &pixels[bytes_per_pixel * width * i], row_size);
        hand += row_size;
        zero_memory(hand, row_padding);
        hand += row_padding;
    }

    save_whole_file(path, file_contents, file_size, stack);

    virtual_deallocate(file_contents);

    return true;
}
