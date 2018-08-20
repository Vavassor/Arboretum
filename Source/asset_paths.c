#include "asset_paths.h"

#include "arboretum_config.h"
#include "filesystem.h"
#include "platform_definitions.h"
#include "string_build.h"

#include <stddef.h>

//TODO: Make assets_path non-global.
static char* assets_path;

static const char* assets_folder = "Assets";
static const char* fonts_folder = "Fonts";
static const char* images_folder = "Images";
static const char* locales_folder = "Locales";
static const char* models_folder = "Models";
static const char* shaders_folder = "Shaders";
static const char* unicode_data_folder = "Unicode";

static const char* shader_extension = ".glsl";
static const char* unicode_data_extension = ".bin";

void set_asset_path(Heap* heap)
{
#if defined(OS_WINDOWS) || ARBORETUM_PORTABLE_APP
    char* path = get_executable_folder(heap);
    remove_basename(path);
#else
    char* path = "/usr/share/arboretum";
#endif
    assets_path = append_to_path(path, assets_folder, heap);
    HEAP_DEALLOCATE(heap, path);
}

char* get_font_path_by_name(const char* asset_name, Stack* stack)
{
    char* path = NULL;
    append_string(&path, assets_path, stack);
    append_string(&path, "/", stack);
    append_string(&path, fonts_folder, stack);
    append_string(&path, "/", stack);
    append_string(&path, asset_name, stack);
    return path;
}

char* get_image_path_by_name(const char* asset_name, Stack* stack)
{
    char* path = NULL;
    append_string(&path, assets_path, stack);
    append_string(&path, "/", stack);
    append_string(&path, images_folder, stack);
    append_string(&path, "/", stack);
    append_string(&path, asset_name, stack);
    return path;
}

char* get_locale_path_by_name(const char* asset_name, Stack* stack)
{
    char* path = NULL;
    append_string(&path, assets_path, stack);
    append_string(&path, "/", stack);
    append_string(&path, locales_folder, stack);
    append_string(&path, "/", stack);
    append_string(&path, asset_name, stack);
    return path;
}

char* get_model_path_by_name(const char* asset_name, Stack* stack)
{
    char* path = NULL;
    append_string(&path, assets_path, stack);
    append_string(&path, "/", stack);
    append_string(&path, models_folder, stack);
    append_string(&path, "/", stack);
    append_string(&path, asset_name, stack);
    return path;
}

char* get_shader_path_by_name(const char* asset_name, Stack* stack)
{
    char* path = NULL;
    append_string(&path, assets_path, stack);
    append_string(&path, "/", stack);
    append_string(&path, shaders_folder, stack);
    append_string(&path, "/", stack);
    append_string(&path, asset_name, stack);
    append_string(&path, shader_extension, stack);
    return path;
}

char* get_unicode_data_path_by_name(const char* asset_name, Stack* stack)
{
    char* path = NULL;
    append_string(&path, assets_path, stack);
    append_string(&path, "/", stack);
    append_string(&path, unicode_data_folder, stack);
    append_string(&path, "/", stack);
    append_string(&path, asset_name, stack);
    append_string(&path, unicode_data_extension, stack);
    return path;
}
