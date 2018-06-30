#ifndef BITMAP_H_
#define BITMAP_H_

#if defined(__cplusplus)
extern "C" {
#endif

#include "gl_core_3_3.h"
#include "memory.h"

#include <stdint.h>

typedef struct Pixel8
{
    uint8_t r;
} Pixel8;

typedef struct Pixel16
{
    uint8_t r, g;
} Pixel16;

typedef struct Pixel24
{
    uint8_t r, g, b;
} Pixel24;

typedef struct Pixel32
{
    uint8_t a, r, g, b;
} Pixel32;

typedef struct Bitmap
{
    void* pixels;
    int width;
    int height;
    int bytes_per_pixel;
} Bitmap;

GLuint upload_bitmap(Bitmap* bitmap);
GLuint upload_bitmap_with_mipmaps(Bitmap* bitmap, Heap* heap);
int get_mipmap_level_count(int width, int height);
Bitmap generate_mipmap(Bitmap* bitmap, Heap* heap);

#if defined(__cplusplus)
} // extern "C"
#endif

#endif // BITMAP_H_
