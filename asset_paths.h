#ifndef ASSET_PATHS_H_
#define ASSET_PATHS_H_

#if defined(__cplusplus)
extern "C" {
#endif

#include "memory.h"

char* get_font_path_by_name(const char* asset_name, Stack* stack);
char* get_image_path_by_name(const char* asset_name, Stack* stack);
char* get_locale_path_by_name(const char* asset_name, Stack* stack);
char* get_model_path_by_name(const char* asset_name, Stack* stack);
char* get_shader_path_by_name(const char* asset_name, Stack* stack);

#if defined(__cplusplus)
} // extern "C"
#endif

#endif // ASSET_PATHS_H_
