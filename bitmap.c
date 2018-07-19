#include "bitmap.h"

#include "array2.h"
#include "float_utilities.h"
#include "int_utilities.h"
#include "math_basics.h"
#include "memory.h"

int get_mip_level_count(int width, int height)
{
    return floor(log2(imax(width, height))) + 1;
}

static Pixel8 lerp8(Pixel8 p0, Pixel8 p1, float t)
{
    Pixel8 result;
    result.r = lerp(p0.r, p1.r, t);
    return result;
}

static Pixel16 lerp16(Pixel16 p0, Pixel16 p1, float t)
{
    Pixel16 result;
    result.r = lerp(p0.r, p1.r, t);
    result.g = lerp(p0.g, p1.g, t);
    return result;
}

static Pixel24 lerp24(Pixel24 p0, Pixel24 p1, float t)
{
    Pixel24 result;
    result.r = lerp(p0.r, p1.r, t);
    result.g = lerp(p0.g, p1.g, t);
    result.b = lerp(p0.b, p1.b, t);
    return result;
}

static Pixel32 lerp32(Pixel32 p0, Pixel32 p1, float t)
{
    Pixel32 result;
    result.r = lerp(p0.r, p1.r, t);
    result.g = lerp(p0.g, p1.g, t);
    result.b = lerp(p0.b, p1.b, t);
    result.a = lerp(p0.a, p1.a, t);
    return result;
}

static void scale_pixel8s(Bitmap* source, Pixel8* pixels, int width, int height)
{
    Pixel8* from = (Pixel8*) source->pixels;
    int source_width = source->width;
    int source_height = source->height;

    float width_divisor = imax(width - 1, 1);
    float height_divisor = imax(height - 1, 1);

    for(int i = 0; i < height; i += 1)
    {
        float y = i * (source_height - 1) / height_divisor;
        int y_int = y;
        float y_fraction = y - y_int;
        int m = imin(y_int + 1, source_height - 1);

        for(int j = 0; j < width; j += 1)
        {
            float x = j * (source_width - 1) / width_divisor;
            int x_int = x;
            float x_fraction = x - x_int;
            int k = imin(x_int + 1, source_width - 1);

            Pixel8 p0 = from[(source_width * y_int) + x_int];
            Pixel8 p1 = from[(source_width * y_int) + k];
            Pixel8 p2 = from[(source_width * m) + x_int];
            Pixel8 p3 = from[(source_width * m) + k];

            Pixel8 v0 = lerp8(p0, p1, x_fraction);
            Pixel8 v1 = lerp8(p2, p3, x_fraction);
            Pixel8 pixel = lerp8(v0, v1, y_fraction);

            pixels[(width * i) + j] = pixel;
        }
    }
}

static void scale_pixel16s(Bitmap* source, Pixel16* pixels, int width, int height)
{
    Pixel16* from = (Pixel16*) source->pixels;
    int source_width = source->width;
    int source_height = source->height;

    float width_divisor = imax(width - 1, 1);
    float height_divisor = imax(height - 1, 1);

    for(int i = 0; i < height; i += 1)
    {
        float y = i * (source_height - 1) / height_divisor;
        int y_int = y;
        float y_fraction = y - y_int;
        int m = imin(y_int + 1, source_height - 1);

        for(int j = 0; j < width; j += 1)
        {
            float x = j * (source_width - 1) / width_divisor;
            int x_int = x;
            float x_fraction = x - x_int;
            int k = imin(x_int + 1, source_width - 1);

            Pixel16 p0 = from[(source_width * y_int) + x_int];
            Pixel16 p1 = from[(source_width * y_int) + k];
            Pixel16 p2 = from[(source_width * m) + x_int];
            Pixel16 p3 = from[(source_width * m) + k];

            Pixel16 v0 = lerp16(p0, p1, x_fraction);
            Pixel16 v1 = lerp16(p2, p3, x_fraction);
            Pixel16 pixel = lerp16(v0, v1, y_fraction);

            pixels[(width * i) + j] = pixel;
        }
    }
}

static void scale_pixel24s(Bitmap* source, Pixel24* pixels, int width, int height)
{
    Pixel24* from = (Pixel24*) source->pixels;
    int source_width = source->width;
    int source_height = source->height;

    float width_divisor = imax(width - 1, 1);
    float height_divisor = imax(height - 1, 1);

    for(int i = 0; i < height; i += 1)
    {
        float y = i * (source_height - 1) / height_divisor;
        int y_int = y;
        float y_fraction = y - y_int;
        int m = imin(y_int + 1, source_height - 1);

        for(int j = 0; j < width; j += 1)
        {
            float x = j * (source_width - 1) / width_divisor;
            int x_int = x;
            float x_fraction = x - x_int;
            int k = imin(x_int + 1, source_width - 1);

            Pixel24 p0 = from[(source_width * y_int) + x_int];
            Pixel24 p1 = from[(source_width * y_int) + k];
            Pixel24 p2 = from[(source_width * m) + x_int];
            Pixel24 p3 = from[(source_width * m) + k];

            Pixel24 v0 = lerp24(p0, p1, x_fraction);
            Pixel24 v1 = lerp24(p2, p3, x_fraction);
            Pixel24 pixel = lerp24(v0, v1, y_fraction);

            pixels[(width * i) + j] = pixel;
        }
    }
}

static void scale_pixel32s(Bitmap* source, Pixel32* pixels, int width, int height)
{
    Pixel32* from = (Pixel32*) source->pixels;
    int source_width = source->width;
    int source_height = source->height;

    float width_divisor = imax(width - 1, 1);
    float height_divisor = imax(height - 1, 1);

    for(int i = 0; i < height; i += 1)
    {
        float y = i * (source_height - 1) / height_divisor;
        int y_int = y;
        float y_fraction = y - y_int;
        int m = imin(y_int + 1, source_height - 1);

        for(int j = 0; j < width; j += 1)
        {
            float x = j * (source_width - 1) / width_divisor;
            int x_int = x;
            float x_fraction = x - x_int;
            int k = imin(x_int + 1, source_width - 1);

            Pixel32 p0 = from[(source_width * y_int) + x_int];
            Pixel32 p1 = from[(source_width * y_int) + k];
            Pixel32 p2 = from[(source_width * m) + x_int];
            Pixel32 p3 = from[(source_width * m) + k];

            Pixel32 v0 = lerp32(p0, p1, x_fraction);
            Pixel32 v1 = lerp32(p2, p3, x_fraction);
            Pixel32 pixel = lerp32(v0, v1, y_fraction);

            pixels[(width * i) + j] = pixel;
        }
    }
}

Bitmap generate_mipmap(Bitmap* bitmap, Heap* heap)
{
    int width = imax(bitmap->width / 2, 1);
    int height = imax(bitmap->height / 2, 1);

    Bitmap result;
    result.width = width;
    result.height = height;
    result.bytes_per_pixel = bitmap->bytes_per_pixel;

    switch(bitmap->bytes_per_pixel)
    {
        case 1:
        {
            Pixel8* pixels = HEAP_ALLOCATE(heap, Pixel8, width * height);
            scale_pixel8s(bitmap, pixels, width, height);
            result.pixels = pixels;
            break;
        }
        case 2:
        {
            Pixel16* pixels = HEAP_ALLOCATE(heap, Pixel16, width * height);
            scale_pixel16s(bitmap, pixels, width, height);
            result.pixels = pixels;
            break;
        }
        case 3:
        {
            Pixel24* pixels = HEAP_ALLOCATE(heap, Pixel24, width * height);
            scale_pixel24s(bitmap, pixels, width, height);
            result.pixels = pixels;
            break;
        }
        case 4:
        {
            Pixel32* pixels = HEAP_ALLOCATE(heap, Pixel32, width * height);
            scale_pixel32s(bitmap, pixels, width, height);
            result.pixels = pixels;
            break;
        }
    }

    return result;
}

Bitmap* generate_mipmap_array(Bitmap* bitmap, Heap* heap)
{
    int levels = get_mip_level_count(bitmap->width, bitmap->height);

    Bitmap* bitmaps = NULL;

    Bitmap prior = *bitmap;
    for(int i = 1; i < levels; i += 1)
    {
        Bitmap mipmap = generate_mipmap(&prior, heap);
        ARRAY_ADD(bitmaps, mipmap, heap);
        prior = mipmap;
    }

    return bitmaps;
}

void bitmap_destroy_array(Bitmap* bitmaps, Heap* heap)
{
    FOR_ALL(Bitmap, bitmaps)
    {
        HEAP_DEALLOCATE(heap, it->pixels);
    }
    ARRAY_DESTROY(bitmaps, heap);
}

int bitmap_get_size(Bitmap* bitmap)
{
    return bitmap->width * bitmap->height * bitmap->bytes_per_pixel;
}
