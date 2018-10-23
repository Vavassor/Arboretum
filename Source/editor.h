#ifndef EDITOR_H_
#define EDITOR_H_

#include "int2.h"
#include "object.h"
#include "platform.h"

typedef struct Editor Editor;

Editor* editor_start_up(Platform* platform);
void editor_shut_down(Editor* editor);
void editor_update(Editor* editor, Platform* platform);
void editor_destroy_clipboard_copy(Editor* editor, char* clipboard);
void editor_paste_from_clipboard(Editor* editor, Platform* platform, char* clipboard);
void resize_viewport(Editor* editor, Int2 dimensions, double dots_per_millimeter);
void clear_object_from_hover_and_selection(Editor* editor, ObjectId object_id, Platform* platform);

#endif // EDITOR_H_
