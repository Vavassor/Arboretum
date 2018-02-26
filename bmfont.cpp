#include "bmfont.h"

#include "filesystem.h"
#include "string_utilities.h"
#include "memory.h"

namespace bmfont {

void destroy_font(Font* font, Heap* heap)
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

struct Stream
{
    const char* buffer;
};

static bool is_whitespace(char c)
{
    return c == ' ' || c - 9 <= 5;
}

static bool is_space_or_tab(char c)
{
    return c == ' ' || c == '\t';
}

static bool is_newline(char c)
{
    return c == '\n' || c == '\r';
}

static bool stream_has_more(Stream* stream)
{
    return *stream->buffer;
}

static void skip_spacing(Stream* stream)
{
    const char* s;
    for(s = stream->buffer; is_space_or_tab(*s); s += 1);
    stream->buffer = s;
}

static void next_line(Stream* stream)
{
    const char* s;
    for(s = stream->buffer; *s && !is_newline(*s); s += 1);
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
    for(s = first; *s && !is_whitespace(*s); ++s);
    int token_size = s - first;

    if(token_size == 0)
    {
        return nullptr;
    }

    char* token = STACK_ALLOCATE(stack, char, token_size + 1);
    copy_string(token, token_size + 1, stream->buffer);
    stream->buffer += token_size;

    return token;
}

struct Pair
{
    char value[32];
    char key[16];
};

static Pair next_pair(Stream* stream, Stack* stack)
{
    skip_spacing(stream);

    Pair result = {};
    const char* s;
    int i;

    if(is_newline(*stream->buffer))
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
        for(; *s && !is_whitespace(*s); s += 1)
        {
            result.value[i] = *s;
            i += 1;
        }
    }
    result.value[i] = '\0';

    stream->buffer = s;

    return result;
}

bool load_font(Font* font, const char* path, Heap* heap, Stack* stack)
{
    void* contents;
    u64 bytes;
    bool contents_loaded = load_whole_file(path, &contents, &bytes, stack);
    if(!contents_loaded)
    {
        return false;
    }

    Font output_font;
    output_font.missing_glyph_index = 0;
    int glyphs_index = 0;
    int kerning_pairs_index = 0;
    bool error = false;

    Stream stream = {};
    stream.buffer = static_cast<const char*>(contents);
    for(; stream_has_more(&stream) && !error; next_line(&stream))
    {
        char* keyword = next_token(&stream, stack);

        if(strings_match(keyword, "info"))
        {
            for(Pair pair = next_pair(&stream, stack); *pair.key; pair = next_pair(&stream, stack))
            {
                if(strings_match(pair.key, "size"))
                {
                    bool success = string_to_int(pair.value, &output_font.size);
                    if(!success)
                    {
                        error = true;
                        break;
                    }
                }
            }
        }
        else if(strings_match(keyword, "common"))
        {
            for(Pair pair = next_pair(&stream, stack); *pair.key; pair = next_pair(&stream, stack))
            {
                if(strings_match(pair.key, "lineHeight"))
                {
                    int line_height;
                    bool success = string_to_int(pair.value, &line_height);
                    if(!success)
                    {
                        error = true;
                        break;
                    }
                    output_font.line_height = line_height;
                }
                else if(strings_match(pair.key, "base"))
                {
                    int baseline;
                    bool success = string_to_int(pair.value, &baseline);
                    if(!success)
                    {
                        error = true;
                        break;
                    }
                    output_font.baseline = baseline;
                }
                else if(strings_match(pair.key, "scaleW"))
                {
                    bool success = string_to_int(pair.value, &output_font.image_width);
                    if(!success)
                    {
                        error = true;
                        break;
                    }
                }
                else if(strings_match(pair.key, "scaleH"))
                {
                    bool success = string_to_int(pair.value, &output_font.image_height);
                    if(!success)
                    {
                        error = true;
                        break;
                    }
                }
                else if(strings_match(pair.key, "pages"))
                {
                    bool success = string_to_int(pair.value, &output_font.pages_count);
                    if(!success)
                    {
                        error = true;
                        break;
                    }
                    output_font.pages = HEAP_ALLOCATE(heap, Page, output_font.pages_count);
                }
            }
        }
        else if(strings_match(keyword, "page"))
        {
            Pair id = next_pair(&stream, stack);
            Pair filename = next_pair(&stream, stack);
            int filename_size = string_size(filename.value) + 1;

            int page_index;
            bool success = string_to_int(id.value, &page_index);
            if(!success)
            {
                error = true;
            }
            else
            {
                Page* page = &output_font.pages[page_index];
                page->bitmap_filename = HEAP_ALLOCATE(heap, char, filename_size);
                copy_string(page->bitmap_filename, filename_size, filename.value);
            }
        }
        else if(strings_match(keyword, "chars"))
        {
            Pair pair = next_pair(&stream, stack);
            bool success = string_to_int(pair.value, &output_font.glyphs_count);
            if(!success)
            {
                error = true;
            }
            else
            {
                output_font.glyphs = HEAP_ALLOCATE(heap, Glyph, output_font.glyphs_count);
            }
        }
        else if(strings_match(keyword, "char"))
        {
            Glyph* glyph = &output_font.glyphs[glyphs_index];

            for(Pair pair = next_pair(&stream, stack); *pair.key; pair = next_pair(&stream, stack))
            {
                if(strings_match(pair.key, "id"))
                {
                    int codepoint;
                    bool success = string_to_int(pair.value, &codepoint);
                    if(!success)
                    {
                        error = true;
                        break;
                    }
                    glyph->codepoint = codepoint;
                }
                else if(strings_match(pair.key, "x"))
                {
                    int x;
                    bool success = string_to_int(pair.value, &x);
                    if(!success)
                    {
                        error = true;
                        break;
                    }
                    glyph->rect.bottom_left.x = x;
                }
                else if(strings_match(pair.key, "y"))
                {
                    int y;
                    bool success = string_to_int(pair.value, &y);
                    if(!success)
                    {
                        error = true;
                        break;
                    }
                    glyph->rect.bottom_left.y = y;
                }
                else if(strings_match(pair.key, "width"))
                {
                    int width;
                    bool success = string_to_int(pair.value, &width);
                    if(!success)
                    {
                        error = true;
                        break;
                    }
                    glyph->rect.dimensions.x = width;
                }
                else if(strings_match(pair.key, "height"))
                {
                    int height;
                    bool success = string_to_int(pair.value, &height);
                    if(!success)
                    {
                        error = true;
                        break;
                    }
                    glyph->rect.dimensions.y = height;
                }
                else if(strings_match(pair.key, "xoffset"))
                {
                    int x_offset;
                    bool success = string_to_int(pair.value, &x_offset);
                    if(!success)
                    {
                        error = true;
                        break;
                    }
                    glyph->offset.x = x_offset;
                }
                else if(strings_match(pair.key, "yoffset"))
                {
                    int y_offset;
                    bool success = string_to_int(pair.value, &y_offset);
                    if(!success)
                    {
                        error = true;
                        break;
                    }
                    glyph->offset.y = y_offset;
                }
                else if(strings_match(pair.key, "xadvance"))
                {
                    int x_advance;
                    bool success = string_to_int(pair.value, &x_advance);
                    if(!success)
                    {
                        error = true;
                        break;
                    }
                    glyph->x_advance = x_advance;
                }
                else if(strings_match(pair.key, "page"))
                {
                    bool success = string_to_int(pair.value, &glyph->page);
                    if(!success)
                    {
                        error = true;
                        break;
                    }
                }
            }

            glyphs_index += 1;
        }
        else if(strings_match(keyword, "kernings"))
        {
            Pair pair = next_pair(&stream, stack);
            bool success = string_to_int(pair.value, &output_font.kerning_pairs_count);
            if(!success)
            {
                error = true;
            }
            else
            {
                output_font.kerning_pairs = HEAP_ALLOCATE(heap, KerningPair, output_font.kerning_pairs_count);
            }
        }
        else if(strings_match(keyword, "kerning"))
        {
            KerningPair* kerning_pair = &output_font.kerning_pairs[kerning_pairs_index];

            for(Pair pair = next_pair(&stream, stack); *pair.key; pair = next_pair(&stream, stack))
            {
                if(strings_match(pair.key, "first"))
                {
                    int first;
                    bool success = string_to_int(pair.value, &first);
                    if(!success)
                    {
                        error = true;
                        break;
                    }
                    kerning_pair->first = first;
                }
                else if(strings_match(pair.key, "second"))
                {
                    int second;
                    bool success = string_to_int(pair.value, &second);
                    if(!success)
                    {
                        error = true;
                        break;
                    }
                    kerning_pair->second = second;
                }
                else if(strings_match(pair.key, "amount"))
                {
                    int kerning;
                    bool success = string_to_int(pair.value, &kerning);
                    if(!success)
                    {
                        error = true;
                        break;
                    }
                    kerning_pair->kerning = kerning;
                }
            }

            kerning_pairs_index += 1;
        }

        STACK_DEALLOCATE(stack, keyword);
    }

    STACK_DEALLOCATE(stack, contents);

    if(error)
    {
        destroy_font(&output_font, heap);
        return false;
    }
    else
    {
        *font = output_font;
        return true;
    }
}

Glyph* find_glyph(Font* font, char32_t c)
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

float lookup_kerning(Font* font, char32_t prior, char32_t current)
{
    for(int i = 0; i < font->kerning_pairs_count; i += 1)
    {
        KerningPair pair = font->kerning_pairs[i];
        if(pair.first == prior && pair.second == current)
        {
            return pair.kerning;
        }
    }
    return 0.0f;
}

} // namespace bmfont
