#include "bmfont.h"

#include "ascii.h"
#include "filesystem.h"
#include "memory.h"
#include "string_utilities.h"

void bmf_destroy_font(BmfFont* font, Heap* heap)
{
    if(font)
    {
        for(int i = 0; i < font->pages_count; i += 1)
        {
            HEAP_DEALLOCATE(heap, font->pages[i].bitmap_filename);
        }
        HEAP_DEALLOCATE(heap, font->pages);
        HEAP_DEALLOCATE(heap, font->glyphs);
        HEAP_DEALLOCATE(heap, font->kerning_pairs);
    }
}

typedef struct Stream
{
    const char* buffer;
} Stream;

static bool stream_has_more(Stream* stream)
{
    return *stream->buffer;
}

static void skip_spacing(Stream* stream)
{
    const char* s;
    for(s = stream->buffer; ascii_is_space_or_tab(*s); s += 1);
    stream->buffer = s;
}

static void next_line(Stream* stream)
{
    const char* s;
    for(s = stream->buffer; *s && !ascii_is_newline(*s); s += 1);
    if(*s)
    {
        s += 1;
    }
    stream->buffer = s;
}

static char* next_token(Stream* stream, Stack* stack)
{
    skip_spacing(stream);

    const char* first = stream->buffer;
    const char* s;
    for(s = first; *s && !ascii_is_whitespace(*s); ++s);
    int token_size = (int) (s - first);

    if(token_size == 0)
    {
        return NULL;
    }

    char* token = STACK_ALLOCATE(stack, char, token_size + 1);
    copy_string(token, token_size + 1, stream->buffer);
    stream->buffer += token_size;

    return token;
}

typedef struct Pair
{
    char value[32];
    char key[16];
} Pair;

static Pair next_pair(Stream* stream, Stack* stack)
{
    skip_spacing(stream);

    Pair result = {0};
    const char* s;
    int i;

    if(ascii_is_newline(*stream->buffer))
    {
        return result;
    }

    // Get the key.
    i = 0;
    for(s = stream->buffer; *s && *s != '='; s += 1)
    {
        result.key[i] = *s;
        i += 1;
    }
    if(*s == '=')
    {
        s += 1;
    }
    result.key[i] = '\0';

    // Get the value.
    i = 0;
    if(*s == '"')
    {
        s += 1;
        for(; *s && *s != '"'; s += 1)
        {
            result.value[i] = *s;
            i += 1;
        }
        if(*s == '"')
        {
            s += 1;
        }
    }
    else
    {
        for(; *s && !ascii_is_whitespace(*s); s += 1)
        {
            result.value[i] = *s;
            i += 1;
        }
    }
    result.value[i] = '\0';

    stream->buffer = s;

    return result;
}

bool bmf_load_font(BmfFont* font, const char* path, Heap* heap, Stack* stack)
{
    WholeFile whole_file = load_whole_file(path, stack);
    if(!whole_file.loaded)
    {
        return false;
    }

    BmfFont output_font;
    output_font.missing_glyph_index = 0;
    int glyphs_index = 0;
    int kerning_pairs_index = 0;
    bool error = false;

    Stream stream = {0};
    stream.buffer = (const char*) whole_file.contents;
    for(; stream_has_more(&stream) && !error; next_line(&stream))
    {
        char* keyword = next_token(&stream, stack);

        if(strings_match(keyword, "info"))
        {
            for(Pair pair = next_pair(&stream, stack); *pair.key; pair = next_pair(&stream, stack))
            {
                if(strings_match(pair.key, "size"))
                {
                    MaybeInt size = string_to_int(pair.value);
                    if(!size.valid)
                    {
                        error = true;
                        break;
                    }
                    output_font.size = size.value;
                }
            }
        }
        else if(strings_match(keyword, "common"))
        {
            for(Pair pair = next_pair(&stream, stack); *pair.key; pair = next_pair(&stream, stack))
            {
                if(strings_match(pair.key, "lineHeight"))
                {
                    MaybeInt line_height = string_to_int(pair.value);
                    if(!line_height.valid)
                    {
                        error = true;
                        break;
                    }
                    output_font.line_height = (float) line_height.value;
                }
                else if(strings_match(pair.key, "base"))
                {
                    MaybeInt baseline = string_to_int(pair.value);
                    if(!baseline.valid)
                    {
                        error = true;
                        break;
                    }
                    output_font.baseline = (float) baseline.value;
                }
                else if(strings_match(pair.key, "scaleW"))
                {
                    MaybeInt image_width = string_to_int(pair.value);
                    if(!image_width.valid)
                    {
                        error = true;
                        break;
                    }
                    output_font.image_width = image_width.value;
                }
                else if(strings_match(pair.key, "scaleH"))
                {
                    MaybeInt image_height = string_to_int(pair.value);
                    if(!image_height.valid)
                    {
                        error = true;
                        break;
                    }
                    output_font.image_height = image_height.value;
                }
                else if(strings_match(pair.key, "pages"))
                {
                    MaybeInt pages_count = string_to_int(pair.value);
                    if(!pages_count.valid)
                    {
                        error = true;
                        break;
                    }
                    output_font.pages_count = pages_count.value;
                    output_font.pages = HEAP_ALLOCATE(heap, BmfPage, output_font.pages_count);
                }
            }
        }
        else if(strings_match(keyword, "page"))
        {
            Pair id = next_pair(&stream, stack);
            Pair filename = next_pair(&stream, stack);
            int filename_size = string_size(filename.value) + 1;

            MaybeInt page_index = string_to_int(id.value);
            if(!page_index.valid)
            {
                error = true;
            }
            else
            {
                BmfPage* page = &output_font.pages[page_index.value];
                page->bitmap_filename = HEAP_ALLOCATE(heap, char, filename_size);
                copy_string(page->bitmap_filename, filename_size, filename.value);
            }
        }
        else if(strings_match(keyword, "chars"))
        {
            Pair pair = next_pair(&stream, stack);
            MaybeInt glyphs_count = string_to_int(pair.value);
            if(!glyphs_count.valid)
            {
                error = true;
            }
            else
            {
                output_font.glyphs_count = glyphs_count.value;
                output_font.glyphs = HEAP_ALLOCATE(heap, BmfGlyph, output_font.glyphs_count);
            }
        }
        else if(strings_match(keyword, "char"))
        {
            BmfGlyph* glyph = &output_font.glyphs[glyphs_index];

            for(Pair pair = next_pair(&stream, stack); *pair.key; pair = next_pair(&stream, stack))
            {
                if(strings_match(pair.key, "id"))
                {
                    MaybeInt codepoint = string_to_int(pair.value);
                    if(!codepoint.valid)
                    {
                        error = true;
                        break;
                    }
                    glyph->codepoint = codepoint.value;
                }
                else if(strings_match(pair.key, "x"))
                {
                    MaybeInt x = string_to_int(pair.value);
                    if(!x.valid)
                    {
                        error = true;
                        break;
                    }
                    glyph->rect.bottom_left.x = (float) x.value;
                }
                else if(strings_match(pair.key, "y"))
                {
                    MaybeInt y = string_to_int(pair.value);
                    if(!y.valid)
                    {
                        error = true;
                        break;
                    }
                    glyph->rect.bottom_left.y = (float) y.value;
                }
                else if(strings_match(pair.key, "width"))
                {
                    MaybeInt width = string_to_int(pair.value);
                    if(!width.valid)
                    {
                        error = true;
                        break;
                    }
                    glyph->rect.dimensions.x = (float) width.value;
                }
                else if(strings_match(pair.key, "height"))
                {
                    MaybeInt height = string_to_int(pair.value);
                    if(!height.valid)
                    {
                        error = true;
                        break;
                    }
                    glyph->rect.dimensions.y = (float) height.value;
                }
                else if(strings_match(pair.key, "xoffset"))
                {
                    MaybeInt x_offset = string_to_int(pair.value);
                    if(!x_offset.valid)
                    {
                        error = true;
                        break;
                    }
                    glyph->offset.x = (float) x_offset.value;
                }
                else if(strings_match(pair.key, "yoffset"))
                {
                    MaybeInt y_offset = string_to_int(pair.value);
                    if(!y_offset.valid)
                    {
                        error = true;
                        break;
                    }
                    glyph->offset.y = (float) y_offset.value;
                }
                else if(strings_match(pair.key, "xadvance"))
                {
                    MaybeInt x_advance = string_to_int(pair.value);
                    if(!x_advance.valid)
                    {
                        error = true;
                        break;
                    }
                    glyph->x_advance = (float) x_advance.value;
                }
                else if(strings_match(pair.key, "page"))
                {
                    MaybeInt page = string_to_int(pair.value);
                    if(!page.valid)
                    {
                        error = true;
                        break;
                    }
                    glyph->page = page.value;
                }
            }

            glyphs_index += 1;
        }
        else if(strings_match(keyword, "kernings"))
        {
            Pair pair = next_pair(&stream, stack);
            MaybeInt kerning_pairs_count = string_to_int(pair.value);
            if(!kerning_pairs_count.valid)
            {
                error = true;
            }
            else
            {
                output_font.kerning_pairs_count = kerning_pairs_count.value;
                output_font.kerning_pairs = HEAP_ALLOCATE(heap, BmfKerningPair, output_font.kerning_pairs_count);
            }
        }
        else if(strings_match(keyword, "kerning"))
        {
            BmfKerningPair* kerning_pair = &output_font.kerning_pairs[kerning_pairs_index];

            for(Pair pair = next_pair(&stream, stack); *pair.key; pair = next_pair(&stream, stack))
            {
                if(strings_match(pair.key, "first"))
                {
                    MaybeInt first = string_to_int(pair.value);
                    if(!first.valid)
                    {
                        error = true;
                        break;
                    }
                    kerning_pair->first = first.value;
                }
                else if(strings_match(pair.key, "second"))
                {
                    MaybeInt second = string_to_int(pair.value);
                    if(!second.valid)
                    {
                        error = true;
                        break;
                    }
                    kerning_pair->second = second.value;
                }
                else if(strings_match(pair.key, "amount"))
                {
                    MaybeInt kerning = string_to_int(pair.value);
                    if(!kerning.valid)
                    {
                        error = true;
                        break;
                    }
                    kerning_pair->kerning = (float) kerning.value;
                }
            }

            kerning_pairs_index += 1;
        }

        STACK_DEALLOCATE(stack, keyword);
    }

    STACK_DEALLOCATE(stack, whole_file.contents);

    if(error)
    {
        bmf_destroy_font(&output_font, heap);
        return false;
    }
    else
    {
        *font = output_font;
        return true;
    }
}

BmfGlyph* bmf_find_glyph(BmfFont* font, char32_t c)
{
    for(int i = 0; i < font->glyphs_count; i += 1)
    {
        if(font->glyphs[i].codepoint == c)
        {
            return &font->glyphs[i];
        }
    }
    return &font->glyphs[font->missing_glyph_index];
}

float bmf_lookup_kerning(BmfFont* font, char32_t prior, char32_t current)
{
    for(int i = 0; i < font->kerning_pairs_count; i += 1)
    {
        BmfKerningPair pair = font->kerning_pairs[i];
        if(pair.first == prior && pair.second == current)
        {
            return pair.kerning;
        }
    }
    return 0.0f;
}
