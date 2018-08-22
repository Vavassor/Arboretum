// This file is for loading and using font data from BMFont by AngelCode
// <http://www.angelcode.com/products/bmfont/>.
//
// They consist of pre-made images with glyphs drawn on them. And .fnt text
// files, which describe which glyph is what part of the image as well as
// additional information about how it should be put together with other glyphs.

#ifndef BMFONT_H_
#define BMFONT_H_

#include "vector_math.h"
#include "geometry.h"
#include "memory.h"

#include <uchar.h>

typedef struct BmfPage
{
    char* bitmap_filename;
} BmfPage;

typedef struct BmfGlyph
{
    Rect rect;
    Float2 offset;
    float x_advance;
    char32_t codepoint;
    int page;
} BmfGlyph;

typedef struct BmfKerningPair
{
    char32_t first;
    char32_t second;
    float kerning;
} BmfKerningPair;

typedef struct BmfFont
{
    BmfPage* pages;
    BmfGlyph* glyphs;
    BmfKerningPair* kerning_pairs;
    int pages_count;
    int glyphs_count;
    int kerning_pairs_count;
    float line_height;
    float baseline;
    int size;
    int image_width;
    int image_height;
    int missing_glyph_index;
} BmfFont;

void bmf_destroy_font(BmfFont* font, Heap* heap);
bool bmf_load_font(BmfFont* font, const char* path, Heap* heap, Stack* stack);
BmfGlyph* bmf_find_glyph(BmfFont* font, char32_t c);
float bmf_lookup_kerning(BmfFont* font, char32_t prior, char32_t current);

#endif // BMFONT_H_
