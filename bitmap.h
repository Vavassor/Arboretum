#ifndef BITMAP_H_
#define BITMAP_H_

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

Bitmap generate_mipmap(Bitmap* bitmap, Heap* heap);
Bitmap* generate_mipmap_array(Bitmap* bitmap, Heap* heap);
void bitmap_destroy_array(Bitmap* bitmaps, Heap* heap);
int bitmap_get_size(Bitmap* bitmap);

#endif // BITMAP_H_
