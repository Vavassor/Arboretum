#include "platform.h"

#include "loc.h"

void create_stack(Platform* platform)
{
    stack_create(&platform->stack, KIBIBYTES(1));
}

void destroy_stack(Platform* platform)
{
    stack_destroy(&platform->stack);
}

bool load_localized_text(Platform* platform)
{
    platform->localized_text.file_pick_dialog_filesystem = "Filesystem";
    platform->localized_text.file_pick_dialog_import = "Import";
    platform->localized_text.main_menu_enter_face_mode = "Face Mode";
    platform->localized_text.main_menu_enter_object_mode = "Object Mode";
    platform->localized_text.main_menu_export_file = "Export .obj";
    platform->localized_text.main_menu_import_file = "Import .obj";

    return loc::load_file(platform);
}
