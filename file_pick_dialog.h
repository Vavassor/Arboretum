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
    ui::Item* panel;
    ui::Id* path_buttons;
    ui::Id pick_button;
    ui::Button* pick;
    int path_buttons_count;
    int record_selected;
    DialogType type;
    bool show_hidden_records;
    bool enabled;
    union
    {
        ui::TextBlock* file_readout;
        struct
        {
            ui::TextInput* filename_field;
            ui::Id filename_field_id;
        };
    };
};

void open_dialog(FilePickDialog* dialog, ui::Context* context, Platform* platform, Heap* heap);
void close_dialog(FilePickDialog* dialog, ui::Context* context, Heap* heap);
void handle_input(FilePickDialog* dialog, ui::Event event, ObjectLady* lady, History* history, ui::Context* context, Platform* platform, Heap* heap, Stack* stack);

#endif // FILE_PICK_DIALOG_H_
