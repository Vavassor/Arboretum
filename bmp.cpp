#include "bmp.h"

#include "filesystem.h"
#include "memory.h"

namespace bmp {

static unsigned int pad_multiple_of_four(unsigned int x)
{
    return (x + 3) & ~0x3;
}

bool write_file(const char* path, const u8* pixels, int width, int height)
{
    unsigned int bytes_per_pixel = 3;
    unsigned int padded_width = pad_multiple_of_four(width);
    unsigned int pixel_data_size = bytes_per_pixel * padded_width * height;
    int row_padding = padded_width - width;

    InfoHeader info;
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

    FileHeader header;
    header.type[0] = 'B';
    header.type[1] = 'M';
    header.size = sizeof(header) + sizeof(info) + pixel_data_size;
    header.reserved1 = 0;
    header.reserved2 = 0;
    header.offset = sizeof(header) + sizeof(info);

    // Create the file in-memory;
    u64 file_size = header.size;
    u8* file_contents = static_cast<u8*>(virtual_allocate(file_size));
    u8* hand = file_contents;

    copy_memory(hand, &header, sizeof(header));
    hand += sizeof(header);
    copy_memory(hand, &info, sizeof(info));
    hand += sizeof(info);

    for(int i = 0; i < height; ++i)
    {
        u64 row_size = bytes_per_pixel * width;
        copy_memory(hand, &pixels[bytes_per_pixel * width * i], row_size);
        hand += row_size;
        set_memory(hand, 0, row_padding);
        hand += row_padding;
    }

    save_whole_file(path, file_contents, file_size);

    virtual_deallocate(file_contents);

    return true;
}

} // namespace bmp
