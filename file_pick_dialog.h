#ifndef FILE_PICK_DIALOG_H_
#define FILE_PICK_DIALOG_H_

#include "filesystem.h"
#include "history.h"
#include "memory.h"
#include "object_lady.h"
#include "ui.h"

enum class DialogType
{
    Import,
    Export,
};

struct FilePickDialog
{
    Directory directory;
    char* path;
    UiItem* panel;
    UiId* path_buttons;
    UiId pick_button;
    UiButton* pick;
    int path_buttons_count;
    int record_selected;
    DialogType type;
    bool show_hidden_records;
    bool enabled;
    union
    {
        UiTextBlock* file_readout;
        struct
        {
            UiTextInput* filename_field;
            UiId filename_field_id;
        };
    };
};

void open_dialog(FilePickDialog* dialog, UiContext* context, Platform* platform, Heap* heap);
void close_dialog(FilePickDialog* dialog, UiContext* context, Heap* heap);
void handle_input(FilePickDialog* dialog, UiEvent event, ObjectLady* lady, int selected_object_index, History* history, UiContext* context, Platform* platform, Heap* heap, Stack* stack);

#endif // FILE_PICK_DIALOG_H_
