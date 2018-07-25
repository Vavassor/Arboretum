#ifndef UI_INTERNAL_H_
#define UI_INTERNAL_H_

float ui_get_list_item_height(UiList* list, UiTextBlock* text_block, float line_height);
bool ui_in_focus_scope(UiItem* outer, UiItem* item);
Float2 ui_compute_cursor_position(UiTextBlock* text_block, Float2 dimensions, float line_height, int index);

#endif // UI_INTERNAL_H_
