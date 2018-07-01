#ifndef PLATFORM_H_
#define PLATFORM_H_

#include "memory.h"

typedef enum CursorType
{
    CURSOR_TYPE_ARROW,
    CURSOR_TYPE_HAND_POINTING,
    CURSOR_TYPE_I_BEAM,
    CURSOR_TYPE_PROHIBITION_SIGN,
} CursorType;

typedef enum LocaleId
{
    LOCALE_ID_DEFAULT,
    LOCALE_ID_en_US,
} LocaleId;

typedef struct Platform
{
    struct
    {
        const char* file_pick_dialog_filesystem;
        const char* file_pick_dialog_export;
        const char* file_pick_dialog_import;
        const char* main_menu_enter_edge_mode;
        const char* main_menu_enter_face_mode;
        const char* main_menu_enter_object_mode;
        const char* main_menu_enter_vertex_mode;
        const char* main_menu_export_file;
        const char* main_menu_import_file;
    } localized_text;

    struct
    {
        const char* app_name;
    } nonlocalized_text;

    Stack stack;

    LocaleId locale_id;
} Platform;

void change_cursor(Platform* platform, CursorType type);
void begin_composed_text(Platform* platform);
void end_composed_text(Platform* platform);
void set_composed_text_position(Platform* base, int x, int y);
bool copy_to_clipboard(Platform* platform, char* clipboard);
void request_paste_from_clipboard(Platform* platform);

void create_stack(Platform* platform);
void destroy_stack(Platform* platform);
bool load_localized_text(Platform* platform);

#endif // PLATFORM_H_
