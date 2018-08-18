#include "platform.h"

#include "asset_paths.h"
#include "loc.h"

void create_stack(Platform* platform)
{
    stack_create(&platform->stack, ezlabytes(32));
}

void destroy_stack(Platform* platform)
{
    stack_destroy(&platform->stack);
}

void platform_create_heap(Platform* platform)
{
    heap_create(&platform->heap, ezlabytes(8));
}

void platform_destroy_heap(Platform* platform)
{
    heap_destroy(&platform->heap);
}

static const char* get_filename_for_locale_id(LocaleId locale_id)
{
    switch(locale_id)
    {
        default:
        case LOCALE_ID_en_US:
        case LOCALE_ID_DEFAULT: return "default.loc";
    }
}

bool load_localized_text(Platform* platform)
{
    platform->localized_text.file_pick_dialog_filesystem = "Filesystem";
    platform->localized_text.file_pick_dialog_export = "Export";
    platform->localized_text.file_pick_dialog_import = "Import";
    platform->localized_text.main_menu_enter_edge_mode = "Edge Mode";
    platform->localized_text.main_menu_enter_face_mode = "Face Mode";
    platform->localized_text.main_menu_enter_object_mode = "Object Mode";
    platform->localized_text.main_menu_enter_vertex_mode = "Vertex Mode";
    platform->localized_text.main_menu_export_file = "Export .obj";
    platform->localized_text.main_menu_import_file = "Import .obj";

    platform->nonlocalized_text.app_name = "Arboretum";

    const char* filename = get_filename_for_locale_id(platform->locale_id);
    char* path = get_locale_path_by_name(filename, &platform->stack);
    bool loaded = loc_load_file(platform, path);

    return loaded;
}
