#ifndef BITMAP_H_
#define BITMAP_H_

#include "gl_core_3_3.h"
#include "sized_types.h"

struct Pixel8
{
    u8 r;
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

#endif // BITMAP_H_
