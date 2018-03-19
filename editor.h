#ifndef EDITOR_H_
#define EDITOR_H_

#include "object.h"

struct Platform;

bool editor_start_up(Platform* platform);
void editor_shut_down();
void editor_update(Platform* platform);
void editor_destroy_clipboard_copy(char* clipboard);
void editor_paste_from_clipboard(char* clipboard);
void resize_viewport(int width, int height, double dots_per_millimeter);
void clear_object_from_hover_and_selection(ObjectId object_id, Platform* platform);

#endif // EDITOR_H_
