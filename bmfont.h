#ifndef BMFONT_H_
#define BMFONT_H_

#include "vector_math.h"
#include "geometry.h"

struct Stack;
struct Heap;

namespace bmfont {

struct Page
{
	char* bitmap_filename;
};

struct Glyph
{
	Rect rect;
	Vector2 offset;
	float x_advance;
	char32_t codepoint;
	int page;
};

struct KerningPair
{
	char32_t first;
	char32_t second;
	float kerning;
};

struct Font
{
	Page* pages;
	Glyph* glyphs;
	KerningPair* kerning_pairs;
	int pages_count;
	int glyphs_count;
	int kerning_pairs_count;
	float line_height;
	float baseline;
	int size;
	int image_width;
	int image_height;
	int missing_glyph_index;
};

void destroy_font(Font* font, Heap* heap);
bool load_font(Font* font, const char* path, Heap* heap, Stack* stack);
Glyph* find_glyph(Font* font, char32_t c);
float lookup_kerning(Font* font, char32_t prior, char32_t current);

} // namespace bmfont

#endif // BMFONT_H_
