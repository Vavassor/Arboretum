#include "bitmap.h"

#include "float_utilities.h"
#include "int_utilities.h"
#include "math_basics.h"
#include "memory.h"

static void upload_pixels(Bitmap* bitmap, GLint level)
{
    switch(bitmap->bytes_per_pixel)
    {
        case 1:
        {
            glTexImage2D(GL_TEXTURE_2D, level, GL_R8, bitmap->width, bitmap->height, 0, GL_RED, GL_UNSIGNED_BYTE, bitmap->pixels);
            break;
        }
        case 2:
        {
            glTexImage2D(GL_TEXTURE_2D, level, GL_RG8, bitmap->width, bitmap->height, 0, GL_RG, GL_UNSIGNED_BYTE, bitmap->pixels);
            break;
        }
        case 3:
        {
            glTexImage2D(GL_TEXTURE_2D, level, GL_RGB8, bitmap->width, bitmap->height, 0, GL_RGB, GL_UNSIGNED_BYTE, bitmap->pixels);
            break;
        }
        case 4:
        {
            glTexImage2D(GL_TEXTURE_2D, level, GL_RGBA8, bitmap->width, bitmap->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, bitmap->pixels);
            break;
        }
    }
}

GLuint upload_bitmap(Bitmap* bitmap)
{
    GLuint texture;

    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    upload_pixels(bitmap, 0);

    return texture;
}

int get_mipmap_level_count(int width, int height)
{
    return floor(log2(MAX(width, height))) + 1;
}

static Pixel8 lerp(Pixel8 p0, Pixel8 p1, float t)
{
    Pixel8 result;
    result.r = lerp(p0.r, p1.r, t);
    return result;
}

static Pixel16 lerp(Pixel16 p0, Pixel16 p1, float t)
{
    Pixel16 result;
    result.r = lerp(p0.r, p1.r, t);
    result.g = lerp(p0.g, p1.g, t);
    return result;
}

static Pixel24 lerp(Pixel24 p0, Pixel24 p1, float t)
{
    Pixel24 result;
    result.r = lerp(p0.r, p1.r, t);
    result.g = lerp(p0.g, p1.g, t);
    result.b = lerp(p0.b, p1.b, t);
    return result;
}

static Pixel32 lerp(Pixel32 p0, Pixel32 p1, float t)
{
    Pixel32 result;
    result.r = lerp(p0.r, p1.r, t);
    result.g = lerp(p0.g, p1.g, t);
    result.b = lerp(p0.b, p1.b, t);
    result.a = lerp(p0.a, p1.a, t);
    return result;
}

static void scale_pixels(Bitmap* source, Pixel8* pixels, int width, int height)
{
    Pixel8* from = static_cast<Pixel8*>(source->pixels);
    int source_width = source->width;
    int source_height = source->height;

    float width_divisor = MAX(width - 1, 1);
    float height_divisor = MAX(height - 1, 1);

    for(int i = 0; i < height; i += 1)
    {
        float y = i * (source_height - 1) / height_divisor;
        int y_int = y;
        float y_fraction = y - y_int;
        int m = MIN(y_int + 1, source_height - 1);

        for(int j = 0; j < width; j += 1)
        {
            float x = j * (source_width - 1) / width_divisor;
            int x_int = x;
            float x_fraction = x - x_int;
            int k = MIN(x_int + 1, source_width - 1);

            Pixel8 p0 = from[(source_width * y_int) + x_int];
            Pixel8 p1 = from[(source_width * y_int) + k];
            Pixel8 p2 = from[(source_width * m) + x_int];
            Pixel8 p3 = from[(source_width * m) + k];

            Pixel8 v0 = lerp(p0, p1, x_fraction);
            Pixel8 v1 = lerp(p2, p3, x_fraction);
            Pixel8 pixel = lerp(v0, v1, y_fraction);

            pixels[(width * i) + j] = pixel;
        }
    }
}

static void scale_pixels(Bitmap* source, Pixel16* pixels, int width, int height)
{
    Pixel16* from = static_cast<Pixel16*>(source->pixels);
    int source_width = source->width;
    int source_height = source->height;

    float width_divisor = MAX(width - 1, 1);
    float height_divisor = MAX(height - 1, 1);

    for(int i = 0; i < height; i += 1)
    {
        float y = i * (source_height - 1) / height_divisor;
        int y_int = y;
        float y_fraction = y - y_int;
        int m = MIN(y_int + 1, source_height - 1);

        for(int j = 0; j < width; j += 1)
        {
            float x = j * (source_width - 1) / width_divisor;
            int x_int = x;
            float x_fraction = x - x_int;
            int k = MIN(x_int + 1, source_width - 1);

            Pixel16 p0 = from[(source_width * y_int) + x_int];
            Pixel16 p1 = from[(source_width * y_int) + k];
            Pixel16 p2 = from[(source_width * m) + x_int];
            Pixel16 p3 = from[(source_width * m) + k];

            Pixel16 v0 = lerp(p0, p1, x_fraction);
            Pixel16 v1 = lerp(p2, p3, x_fraction);
            Pixel16 pixel = lerp(v0, v1, y_fraction);

            pixels[(width * i) + j] = pixel;
        }
    }
}

static void scale_pixels(Bitmap* source, Pixel24* pixels, int width, int height)
{
    Pixel24* from = static_cast<Pixel24*>(source->pixels);
    int source_width = source->width;
    int source_height = source->height;

    float width_divisor = MAX(width - 1, 1);
    float height_divisor = MAX(height - 1, 1);

    for(int i = 0; i < height; i += 1)
    {
        float y = i * (source_height - 1) / height_divisor;
        int y_int = y;
        float y_fraction = y - y_int;
        int m = MIN(y_int + 1, source_height - 1);

        for(int j = 0; j < width; j += 1)
        {
            float x = j * (source_width - 1) / width_divisor;
            int x_int = x;
            float x_fraction = x - x_int;
            int k = MIN(x_int + 1, source_width - 1);

            Pixel24 p0 = from[(source_width * y_int) + x_int];
            Pixel24 p1 = from[(source_width * y_int) + k];
            Pixel24 p2 = from[(source_width * m) + x_int];
            Pixel24 p3 = from[(source_width * m) + k];

            Pixel24 v0 = lerp(p0, p1, x_fraction);
            Pixel24 v1 = lerp(p2, p3, x_fraction);
            Pixel24 pixel = lerp(v0, v1, y_fraction);

            pixels[(width * i) + j] = pixel;
        }
    }
}

static void scale_pixels(Bitmap* source, Pixel32* pixels, int width, int height)
{
    Pixel32* from = static_cast<Pixel32*>(source->pixels);
    int source_width = source->width;
    int source_height = source->height;

    float width_divisor = MAX(width - 1, 1);
    float height_divisor = MAX(height - 1, 1);

    for(int i = 0; i < height; i += 1)
    {
        float y = i * (source_height - 1) / height_divisor;
        int y_int = y;
        float y_fraction = y - y_int;
        int m = MIN(y_int + 1, source_height - 1);

        for(int j = 0; j < width; j += 1)
        {
            float x = j * (source_width - 1) / width_divisor;
            int x_int = x;
            float x_fraction = x - x_int;
            int k = MIN(x_int + 1, source_width - 1);

            Pixel32 p0 = from[(source_width * y_int) + x_int];
            Pixel32 p1 = from[(source_width * y_int) + k];
            Pixel32 p2 = from[(source_width * m) + x_int];
            Pixel32 p3 = from[(source_width * m) + k];

            Pixel32 v0 = lerp(p0, p1, x_fraction);
            Pixel32 v1 = lerp(p2, p3, x_fraction);
            Pixel32 pixel = lerp(v0, v1, y_fraction);

            pixels[(width * i) + j] = pixel;
        }
    }
}

Bitmap generate_mipmap(Bitmap* bitmap, Heap* heap)
{
    int width = MAX(bitmap->width / 2, 1);
    int height = MAX(bitmap->height / 2, 1);

    Bitmap result;
    result.width = width;
    result.height = height;
    result.bytes_per_pixel = bitmap->bytes_per_pixel;

    switch(bitmap->bytes_per_pixel)
    {
        case 1:
        {
            Pixel8* pixels = HEAP_ALLOCATE(heap, Pixel8, width * height);
            scale_pixels(bitmap, pixels, width, height);
            result.pixels = pixels;
            break;
        }
        case 2:
        {
            Pixel16* pixels = HEAP_ALLOCATE(heap, Pixel16, width * height);
            scale_pixels(bitmap, pixels, width, height);
            result.pixels = pixels;
            break;
        }
        case 3:
        {
            Pixel24* pixels = HEAP_ALLOCATE(heap, Pixel24, width * height);
            scale_pixels(bitmap, pixels, width, height);
            result.pixels = pixels;
            break;
        }
        case 4:
        {
            Pixel32* pixels = HEAP_ALLOCATE(heap, Pixel32, width * height);
            scale_pixels(bitmap, pixels, width, height);
            result.pixels = pixels;
            break;
        }
    }

    return result;
}

GLuint upload_bitmap_with_mipmaps(Bitmap* bitmap, Heap* heap)
{
    GLuint texture;

    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    int levels = get_mipmap_level_count(bitmap->width, bitmap->height);

    upload_pixels(bitmap, 0);
    Bitmap prior = *bitmap;
    for(int i = 1; i < levels; i += 1)
    {
        Bitmap mipmap = generate_mipmap(&prior, heap);
        if(i > 1)
        {
            HEAP_DEALLOCATE(heap, prior.pixels);
        }
        upload_pixels(&mipmap, i);
        prior = mipmap;
    }
    if(levels > 1)
    {
        HEAP_DEALLOCATE(heap, prior.pixels);
    }

    return texture;
}
