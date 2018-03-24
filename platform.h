#ifndef PLATFORM_H_
#define PLATFORM_H_

#include "memory.h"

enum class CursorType
{
    Arrow,
    Hand_Pointing,
    I_Beam,
    Prohibition_Sign,
};

enum class LocaleId
{
    Default,
    en_US,
};

struct Platform
{
    struct
    {
        const char* file_pick_dialog_filesystem;
        const char* file_pick_dialog_import;
        const char* main_menu_enter_face_mode;
        const char* main_menu_enter_object_mode;
        const char* main_menu_export_file;
        const char* main_menu_import_file;
    } localized_text;

    struct
    {
        const char* app_name;
    } nonlocalized_text;

    Stack stack;

    LocaleId locale_id;
};

void change_cursor(Platform* platform, CursorType type);
void begin_composed_text(Platform* platform);
void end_composed_text(Platform* platform);
bool copy_to_clipboard(Platform* platform, char* clipboard);
void request_paste_from_clipboard(Platform* platform);

void create_stack(Platform* platform);
void destroy_stack(Platform* platform);
bool load_localized_text(Platform* platform);

#endif // PLATFORM_H_
