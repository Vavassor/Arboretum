#ifndef BMP_H_
#define BMP_H_

#include "sized_types.h"

struct Stack;

// Bitmap Image File (.bmp)
namespace bmp {

#pragma pack(push, bmp, 1)

struct FileHeader
{
    char type[2];
    u32 size;
    u16 reserved1;
    u16 reserved2;
    u32 offset;
};

struct InfoHeader
{
    u32 size;
    s32 width;
    s32 height;
    u16 planes;
    u16 bits_per_pixel;
    u32 compression;
    u32 image_size;
    s32 pixels_per_meter_x;
    s32 pixels_per_meter_y;
    u32 colours_used;
    u32 important_colours;
};

#pragma pack(pop, bmp)

enum
{
    COMPRESSION_NONE = 0,
};

bool write_file(const char* path, const u8* pixels, int width, int height, Stack* stack);

} // namespace bmp

#endif // BMP_H_
