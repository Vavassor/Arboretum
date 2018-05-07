#include "asset_paths.h"

#include "string_build.h"

namespace
{
    const char* assets_path = "/home/andrew/workspace/Arboretum/Assets";

    const char* fonts_folder = "Fonts";
    const char* images_folder = "Images";
    const char* locales_folder = "Locales";
    const char* models_folder = "Models";
    const char* shaders_folder = "Shaders";

    const char* shader_extension = ".glsl";
}

char* get_font_path_by_name(const char* asset_name, Stack* stack)
{
    char* path = nullptr;
    append_string(&path, assets_path, stack);
    append_string(&path, "/", stack);
    append_string(&path, fonts_folder, stack);
    append_string(&path, "/", stack);
    append_string(&path, asset_name, stack);
    return path;
}

char* get_image_path_by_name(const char* asset_name, Stack* stack)
{
    char* path = nullptr;
    append_string(&path, assets_path, stack);
    append_string(&path, "/", stack);
    append_string(&path, images_folder, stack);
    append_string(&path, "/", stack);
    append_string(&path, asset_name, stack);
    return path;
}

char* get_locale_path_by_name(const char* asset_name, Stack* stack)
{
    char* path = nullptr;
    append_string(&path, assets_path, stack);
    append_string(&path, "/", stack);
    append_string(&path, locales_folder, stack);
    append_string(&path, "/", stack);
    append_string(&path, asset_name, stack);
    return path;
}

char* get_model_path_by_name(const char* asset_name, Stack* stack)
{
    char* path = nullptr;
    append_string(&path, assets_path, stack);
    append_string(&path, "/", stack);
    append_string(&path, models_folder, stack);
    append_string(&path, "/", stack);
    append_string(&path, asset_name, stack);
    return path;
}

char* get_shader_path_by_name(const char* asset_name, Stack* stack)
{
    char* path = nullptr;
    append_string(&path, assets_path, stack);
    append_string(&path, "/", stack);
    append_string(&path, shaders_folder, stack);
    append_string(&path, "/", stack);
    append_string(&path, asset_name, stack);
    append_string(&path, shader_extension, stack);
    return path;
}
