#include "loc.h"

#include "filesystem.h"
#include "memory.h"
#include "platform.h"
#include "string_utilities.h"

namespace loc {

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

struct Stream
{
    const char* buffer;
    bool error_occurred;
};

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

    char* name = STACK_ALLOCATE(stack, char, size + 1);
    copy_string(name, size + 1, stream->buffer);
    stream->buffer += size;

    return name;
}

static char* get_entry(Stream* stream, Stack* stack)
{
    char* entry = nullptr;
    int added = 0;

    for(int i = 0; stream->buffer[i]; i += 1)
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
                entry = STACK_REALLOCATE(stack, entry, added + 2);
                entry[added] = '\\';
                entry[added + 1] = c;
                added += 2;

                do
                {
                    i += 1;
                    c = stream->buffer[i];

                    if(is_valid_in_name(c))
                    {
                        entry = STACK_REALLOCATE(stack, entry, added + 2);
                        entry[added] = c;
                        added += 1;
                        continue;
                    }
                    else if(c == '\\')
                    {
                        entry = STACK_REALLOCATE(stack, entry, added + 2);
                        entry[added] = c;
                        added += 1;
                        break;
                    }
                    else
                    {
                        STACK_DEALLOCATE(stack, entry);
                        return nullptr;
                    }
                } while(stream->buffer[i]);
            }
            else if(c == '\\' || c == ']')
            {
                entry = STACK_REALLOCATE(stack, entry, added + 2);
                entry[added] = c;
                added += 1;
            }
            else if(c != '\n')
            {
                STACK_DEALLOCATE(stack, entry);
                return nullptr;
            }
        }
        else
        {
            entry = STACK_REALLOCATE(stack, entry, added + 2);
            entry[added] = c;
            added += 1;
        }
    }

    if(entry)
    {
        entry[added] = '\0';
    }

    return entry;
}

static bool add_localized_text(Platform* platform, const char* name, const char* entry)
{
    struct
    {
        const char* name;
        const char** text;
    } table[6] =
    {
        {"file_pick_dialog_import",     &platform->localized_text.file_pick_dialog_import},
        {"file_pick_dialog_filesystem", &platform->localized_text.file_pick_dialog_filesystem},
        {"main_menu_enter_face_mode",   &platform->localized_text.main_menu_enter_face_mode},
        {"main_menu_enter_object_mode", &platform->localized_text.main_menu_enter_object_mode},
        {"main_menu_export_file",       &platform->localized_text.main_menu_export_file},
        {"main_menu_import_file",       &platform->localized_text.main_menu_import_file},
    };

    for(int i = 0; i < 6; i += 1)
    {
        if(strings_match(table[i].name, name))
        {
            *table[i].text = entry;
            return true;
        }
    }

    return false;
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

    if(start_found)
    {
        stream->buffer += 1;

        char* entry = get_entry(stream, stack);
        end_found = entry;

        if(end_found)
        {
            int size = string_size(entry) + 1;
            char* copy = STACK_ALLOCATE(&platform->stack, char, size);
            copy_string(copy, size, entry);

            added = add_localized_text(platform, name, copy);
            if(!added)
            {
                STACK_DEALLOCATE(&platform->stack, copy);
            }

            STACK_DEALLOCATE(stack, entry);
        }
    }

    STACK_DEALLOCATE(stack, name);

    return start_found && end_found && added;
}

static const char* get_filename_for_locale_id(LocaleId locale_id)
{
    switch(locale_id)
    {
        default:
        case LocaleId::Default: return "default.loc";
    }
}

bool load_file(Platform* platform)
{
    Stack stack = {};
    stack_create(&stack, KIBIBYTES(16));

    const char* path = get_filename_for_locale_id(platform->locale_id);

    void* contents;
    u64 file_size;
    bool loaded = load_whole_file(path, &contents, &file_size, &stack);

    if(loaded)
    {
        Stream stream;
        stream.buffer = static_cast<const char*>(contents);
        bool processed_fine = true;

        while(stream_has_more(&stream) && processed_fine && !stream.error_occurred)
        {
            processed_fine = process_next_entry(&stream, platform, &stack);
            next_line(&stream);
        }
    }

    stack_destroy(&stack);

    return loaded;
}

} // namespace loc
