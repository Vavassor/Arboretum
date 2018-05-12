#ifndef BITMAP_H_
#define BITMAP_H_

#include "gl_core_3_3.h"
#include "sized_types.h"

struct Heap;

struct Pixel8
{
    u8 r;
};

struct Pixel16
{
    u8 r, g;
};

struct Pixel24
{
    u8 r, g, b;
};

struct Pixel32
{
    u8 a, r, g, b;
};

struct Bitmap
{
    void* pixels;
    int width;
    int height;
    int bytes_per_pixel;
};

GLuint upload_bitmap(Bitmap* bitmap);
GLuint upload_bitmap_with_mipmaps(Bitmap* bitmap, Heap* heap);
int get_mipmap_level_count(int width, int height);
Bitmap generate_mipmap(Bitmap* bitmap, Heap* heap);

#endif // BITMAP_H_
