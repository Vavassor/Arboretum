#include "loc.h"

#include "array2.h"
#include "filesystem.h"
#include "memory.h"
#include "string_build.h"
#include "string_utilities.h"

static bool is_space_or_tab_ascii(char c)
{
    return c == ' ' || c == '\t';
}

static bool is_newline_ascii(char c)
{
    return c >= '\n' && c <= '\r';
}

static bool is_numeric_ascii(char c)
{
    return c >= '0' && c <= '9';
}

static bool is_alphabetic_ascii(char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

static bool is_valid_in_name(char c)
{
    return is_alphabetic_ascii(c) || is_numeric_ascii(c) || c == '_';
}

typedef struct Stream
{
    const char* buffer;
    bool error_occurred;
} Stream;

static bool stream_has_more(Stream* stream)
{
    return *stream->buffer;
}

static void skip_spacing(Stream* stream)
{
    const char* s;
    for(s = stream->buffer; is_space_or_tab_ascii(*s); s += 1);
    stream->buffer = s;
}

static void next_line(Stream* stream)
{
    const char* s;
    for(s = stream->buffer; *s && !is_newline_ascii(*s); s += 1);
    if(*s)
    {
        s += 1;
    }
    stream->buffer = s;
}

static bool find_name(Stream* stream)
{
    skip_spacing(stream);

    char c = *stream->buffer;
    if(is_alphabetic_ascii(c))
    {
        return true;
    }
    else if(c == '#' || is_newline_ascii(c))
    {
        return false;
    }
    else
    {
        stream->error_occurred = true;
        return false;
    }
}

static char* get_name(Stream* stream, Stack* stack)
{
    int i;
    for(i = 1; is_valid_in_name(stream->buffer[i]); i += 1);
    int size = i;

    char* name = copy_chars_to_stack(stream->buffer, size, stack);
    stream->buffer += size;

    return name;
}

static char* get_entry(Stream* stream, Stack* stack)
{
    char* entry = NULL;

    int i;
    for(i = 0; stream->buffer[i]; i += 1)
    {
        char c = stream->buffer[i];
        if(c == ']')
        {
            break;
        }
        else if(c == '\\')
        {
            i += 1;
            c = stream->buffer[i];

            if(is_alphabetic_ascii(c))
            {
                ARRAY_ADD_STACK(entry, '\\', stack);
                ARRAY_ADD_STACK(entry, c, stack);

                do
                {
                    i += 1;
                    c = stream->buffer[i];

                    if(is_valid_in_name(c))
                    {
                        ARRAY_ADD_STACK(entry, c, stack);
                        continue;
                    }
                    else if(c == '\\')
                    {
                        ARRAY_ADD_STACK(entry, c, stack);
                        break;
                    }
                    else
                    {
                        ARRAY_DESTROY_STACK(entry, stack);
                        return NULL;
                    }
                } while(stream->buffer[i]);
            }
            else if(c == '\\' || c == ']')
            {
                ARRAY_ADD_STACK(entry, c, stack);
            }
            else if(c != '\n')
            {
                ARRAY_DESTROY_STACK(entry, stack);
                return NULL;
            }
        }
        else
        {
            ARRAY_ADD_STACK(entry, c, stack);
        }
    }

    stream->buffer += i;

    if(entry)
    {
        ARRAY_ADD_STACK(entry, '\0', stack);
        stream->buffer += 1;
    }

    return entry;
}

#define TABLE_COUNT 7

static bool add_localized_text(Platform* platform, const char* name, const char* entry)
{
    struct
    {
        const char* name;
        const char** text;
    } table[TABLE_COUNT] =
    {
        {"app_name",                    &platform->nonlocalized_text.app_name},
        {"file_pick_dialog_import",     &platform->localized_text.file_pick_dialog_import},
        {"file_pick_dialog_filesystem", &platform->localized_text.file_pick_dialog_filesystem},
        {"main_menu_enter_face_mode",   &platform->localized_text.main_menu_enter_face_mode},
        {"main_menu_enter_object_mode", &platform->localized_text.main_menu_enter_object_mode},
        {"main_menu_export_file",       &platform->localized_text.main_menu_export_file},
        {"main_menu_import_file",       &platform->localized_text.main_menu_import_file},
    };

    for(int i = 0; i < TABLE_COUNT; i += 1)
    {
        if(strings_match(table[i].name, name))
        {
            *table[i].text = entry;
            return true;
        }
    }

    return false;
}

static bool check_until_newline(Stream* stream)
{
    const char* s;
    for(s = stream->buffer; *s && is_space_or_tab_ascii(*s); s += 1);
    stream->buffer = s;

    return is_newline_ascii(*s);
}

static bool process_next_entry(Stream* stream, Platform* platform, Stack* stack)
{
    bool found = find_name(stream);
    if(!found)
    {
        return !stream->error_occurred;
    }

    char* name = get_name(stream, stack);
    if(!name)
    {
        return false;
    }

    skip_spacing(stream);

    bool start_found = *stream->buffer == '[';
    bool end_found = false;
    bool added = false;
    bool end_of_line_clean = false;

    if(start_found)
    {
        stream->buffer += 1;

        char* entry = get_entry(stream, stack);
        end_found = entry;

        if(end_found)
        {
            char* copy = copy_string_to_stack(entry, &platform->stack);

            added = add_localized_text(platform, name, copy);
            if(!added)
            {
                STACK_DEALLOCATE(&platform->stack, copy);
            }

            ARRAY_DESTROY_STACK(entry, stack);

            end_of_line_clean = check_until_newline(stream);
        }
    }

    STACK_DEALLOCATE(stack, name);

    return start_found && end_found && added && end_of_line_clean;
}

bool loc_load_file(Platform* platform, const char* path)
{
    Stack stack = {0};
    stack_create(&stack, (uint32_t) capobytes(16));

    void* contents;
    uint64_t file_size;
    bool loaded = load_whole_file(path, &contents, &file_size, &stack);

    if(loaded)
    {
        Stream stream;
        stream.buffer = (const char*) contents;
        stream.error_occurred = false;
        bool processed_fine = true;

        while(stream_has_more(&stream) && processed_fine && !stream.error_occurred)
        {
            processed_fine = process_next_entry(&stream, platform, &stack);
            next_line(&stream);
        }

        if(!processed_fine || stream.error_occurred)
        {
            loaded = false;
        }
    }

    stack_destroy(&stack);

    return loaded;
}
