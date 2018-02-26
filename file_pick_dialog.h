#ifndef FILE_PICK_DIALOG_H_
#define FILE_PICK_DIALOG_H_

#include "bmfont.h"
#include "filesystem.h"
#include "history.h"
#include "memory.h"
#include "object_lady.h"
#include "ui.h"

struct FilePickDialog
{
    Directory directory;
    char* path;
    ui::Item* panel;
    ui::Id* path_buttons;
    ui::TextBlock* file_readout;
    ui::Id pick_button;
    ui::Button* pick;
    int path_buttons_count;
    int record_selected;
    bool show_hidden_records;
    bool enabled;
};

void open_dialog(FilePickDialog* dialog, bmfont::Font* font, ui::Context* context, Heap* heap);
void close_dialog(FilePickDialog* dialog, ui::Context* context, Heap* heap);
void handle_input(FilePickDialog* dialog, ui::Event event, bmfont::Font* font, ObjectLady* lady, History* history, ui::Context* context, Heap* heap, Stack* stack);

#endif // FILE_PICK_DIALOG_H_
