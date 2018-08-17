#include "file_pick_dialog.h"

#include "array2.h"
#include "ascii.h"
#include "assert.h"
#include "colours.h"
#include "obj.h"
#include "platform.h"
#include "sorting.h"
#include "string_build.h"
#include "string_utilities.h"
#include "video.h"

static bool record_is_before(DirectoryRecord a, DirectoryRecord b)
{
    if(a.type == DIRECTORY_RECORD_TYPE_DIRECTORY && b.type == DIRECTORY_RECORD_TYPE_FILE)
    {
        return true;
    }
    if(a.type == DIRECTORY_RECORD_TYPE_FILE && b.type == DIRECTORY_RECORD_TYPE_DIRECTORY)
    {
        return false;
    }
    return ascii_compare_alphabetic(a.name, b.name) < 0;
}

DEFINE_QUICK_SORT(DirectoryRecord, record_is_before, by_filename);

static void filter_directory(Directory* directory, const char** extensions, int extensions_count, bool show_hidden, Heap* heap)
{
    DirectoryRecord* filtered = NULL;

    for(int i = 0; i < directory->records_count; i += 1)
    {
        DirectoryRecord record = directory->records[i];
        if(!show_hidden && record.hidden)
        {
            continue;
        }
        if(record.type == DIRECTORY_RECORD_TYPE_FILE)
        {
            for(int j = 0; j < extensions_count; j += 1)
            {
                if(string_ends_with(record.name, extensions[j]))
                {
                    ARRAY_ADD(filtered, record, heap);
                    break;
                }
            }
        }
        else
        {
            ARRAY_ADD(filtered, record, heap);
        }
    }

    ARRAY_DESTROY(directory->records, heap);

    directory->records = filtered;
    directory->records_count = array_count(filtered);
}

#define EXTENSIONS_COUNT 1

static void list_directory(FilePickDialog* dialog, const char* directory, UiContext* context, Platform* platform, Heap* heap)
{
    // Attempt to list the directory, first.
    Directory new_directory = {0};
    bool listed = list_files_in_directory(directory, &new_directory, heap);
    if(!listed)
    {
        destroy_directory(&new_directory, heap);
        return;
    }

    // Clean up a previous directory if there was one.
    UiItem* panel = dialog->panel;

    if(dialog->path_buttons_count > 0)
    {
        ui_empty_item(context, &panel->container.items[0]);
    }
    if(dialog->directory.records_count > 0)
    {
        ui_empty_item(context, &panel->container.items[1]);
    }

    SAFE_HEAP_DEALLOCATE(heap, dialog->path);
    SAFE_HEAP_DEALLOCATE(heap, dialog->path_buttons);
    destroy_directory(&dialog->directory, heap);

    // Set the new directory only after the previous was destroyed.
    dialog->directory = new_directory;

    // Set up the path bar at the top of the dialog.
    dialog->path = copy_string_to_heap(directory, heap);

    int slashes = count_char_occurrences(dialog->path, '/');
    int buttons_in_row;
    if(string_size(dialog->path) == 1)
    {
        ASSERT(dialog->path[0] == '/');
        buttons_in_row = slashes;
    }
    else
    {
        buttons_in_row = slashes + 1;
    }
    dialog->path_buttons_count = buttons_in_row;
    dialog->path_buttons = HEAP_ALLOCATE(heap, UiId, buttons_in_row);

    UiItem* path_bar = &panel->container.items[0];
    path_bar->type = UI_ITEM_TYPE_CONTAINER;
    path_bar->container.style_type = UI_STYLE_TYPE_PATH_BAR;
    ui_add_row(&path_bar->container, buttons_in_row, context, heap);

    // Add buttons to the path bar.
    int i = 0;
    for(const char* path = dialog->path; *path; i += 1)
    {
        UiItem* item = &path_bar->container.items[i];
        item->type = UI_ITEM_TYPE_BUTTON;

        UiButton* button = &item->button;
        button->enabled = true;
        button->text_block.padding = (UiPadding){{4.0f, 4.0f, 4.0f, 4.0f}};
        button->text_block.text_overflow = UI_TEXT_OVERFLOW_ELLIPSIZE_END;
        dialog->path_buttons[i] = item->id;

        int found_index = find_char(path, '/');
        if(found_index == 0)
        {
            // root directory
            const char* name = platform->localized_text.file_pick_dialog_filesystem;
            ui_set_text(&button->text_block, name, heap);
        }
        else if(found_index == invalid_index)
        {
            // final path segment
            ui_set_text(&button->text_block, path, heap);
            break;
        }
        else
        {
            char* temp = copy_chars_to_heap(path, found_index, heap);
            ui_set_text(&button->text_block, temp, heap);
            HEAP_DEALLOCATE(heap, temp);
        }
        path += found_index + 1;
    }

    // Set up the directory listing.
    UiItem* file_list = &panel->container.items[1];
    file_list->type = UI_ITEM_TYPE_LIST;
    file_list->growable = true;

    UiList* list = &file_list->list;
    list->item_spacing = 2.0f;
    list->side_margin = 2.0f;
    list->scroll_top = 0.0f;

    const char* extensions[EXTENSIONS_COUNT] = {".obj"};
    filter_directory(&dialog->directory, extensions, EXTENSIONS_COUNT, dialog->show_hidden_records, heap);

    ui_create_items(file_list, dialog->directory.records_count, heap);

    if(dialog->directory.records_count == 0)
    {
        // Listing the directory succeeded, it's just empty.
    }
    else
    {
        quick_sort_by_filename(dialog->directory.records, dialog->directory.records_count);

        for(int i = 0; i < dialog->directory.records_count; i += 1)
        {
            DirectoryRecord record = dialog->directory.records[i];
            UiTextBlock* text_block = &list->items[i];
            text_block->padding = (UiPadding){{1.0f, 1.0f, 1.0f, 1.0f}};
            text_block->text_overflow = UI_TEXT_OVERFLOW_ELLIPSIZE_END;
            ui_set_text(text_block, record.name, heap);
        }
    }

    ui_focus_on_item(context, file_list);
}

void open_dialog(FilePickDialog* dialog, UiContext* context, Platform* platform, Heap* heap)
{
    const char* default_path = get_user_folder(USER_FOLDER_DOCUMENTS, heap);

    dialog->enabled = true;

    // Create the panel for the dialog box.
    UiItem* panel = ui_create_toplevel_container(context, heap);
    panel->type = UI_ITEM_TYPE_CONTAINER;
    panel->growable = true;
    panel->container.alignment = UI_ALIGNMENT_STRETCH;
    dialog->panel = panel;

    ui_add_column(&panel->container, 3, context, heap);

    list_directory(dialog, default_path, context, platform, heap);

    // Set up the footer.
    UiItem* footer = &panel->container.items[2];
    footer->type = UI_ITEM_TYPE_CONTAINER;
    footer->container.style_type = UI_STYLE_TYPE_FOOTER;
    footer->container.justification = UI_JUSTIFICATION_SPACE_BETWEEN;

    ui_add_row(&footer->container, 2, context, heap);

    const char* pick_button_text;
    switch(dialog->type)
    {
        default:
        case DIALOG_TYPE_EXPORT:
        {
            UiItem* filename_field = &footer->container.items[0];
            filename_field->type = UI_ITEM_TYPE_TEXT_INPUT;
            filename_field->text_input.text_block.padding = (UiPadding){{4.0f, 4.0f, 4.0f, 4.0f}};
            filename_field->text_input.label.padding = (UiPadding){{4.0f, 4.0f, 4.0f, 4.0f}};
            filename_field->growable = true;
            ui_set_text(&filename_field->text_input.text_block, "", heap);
            ui_set_text(&filename_field->text_input.label, "enter filename", heap);
            dialog->filename_field_id = filename_field->id;
            dialog->filename_field = &filename_field->text_input;

            pick_button_text = platform->localized_text.file_pick_dialog_export;

            break;
        }
        case DIALOG_TYPE_IMPORT:
        {
            UiItem* file_readout = &footer->container.items[0];
            file_readout->type = UI_ITEM_TYPE_TEXT_BLOCK;
            file_readout->text_block.padding = (UiPadding){{4.0f, 4.0f, 4.0f, 4.0f}};
            ui_set_text(&file_readout->text_block, "", heap);
            dialog->file_readout = &file_readout->text_block;

            pick_button_text = platform->localized_text.file_pick_dialog_import;

            break;
        }
    }

    UiItem* pick = &footer->container.items[1];
    pick->type = UI_ITEM_TYPE_BUTTON;
    pick->button.text_block.padding = (UiPadding){{4.0f, 4.0f, 4.0f, 4.0f}};
    ui_set_text(&pick->button.text_block, pick_button_text, heap);
    dialog->pick_button = pick->id;
    dialog->pick = &pick->button;
}

void close_dialog(FilePickDialog* dialog, UiContext* context, Heap* heap)
{
    if(dialog)
    {
        SAFE_HEAP_DEALLOCATE(heap, dialog->path);
        SAFE_HEAP_DEALLOCATE(heap, dialog->path_buttons);

        if(dialog->panel)
        {
            ui_destroy_toplevel_container(context, dialog->panel, heap);
            dialog->panel = NULL;
        }

        destroy_directory(&dialog->directory, heap);
        dialog->enabled = false;
    }
}

static void touch_record(FilePickDialog* dialog, int record_index, bool expand, UiContext* context, Platform* platform, Heap* heap)
{
    DirectoryRecord record = dialog->directory.records[record_index];
    switch(record.type)
    {
        case DIRECTORY_RECORD_TYPE_DIRECTORY:
        {
            switch(dialog->type)
            {
                case DIALOG_TYPE_IMPORT:
                {
                    ui_set_text(dialog->file_readout, "", heap);
                    dialog->pick->enabled = false;
                    dialog->record_selected = invalid_index;
                    break;
                }
                case DIALOG_TYPE_EXPORT:
                {
                    // If the user typed in an arbitrary filename, it shouldn't
                    // be cleared.
                    if(dialog->record_selected != invalid_index)
                    {
                        ui_set_text(dialog->file_readout, "", heap);
                        dialog->pick->enabled = false;
                        dialog->record_selected = invalid_index;
                    }
                    break;
                }
            }

            if(expand)
            {
                char* path = append_to_path(dialog->path, record.name, heap);

                list_directory(dialog, path, context, platform, heap);

                HEAP_DEALLOCATE(heap, path);
            }
            break;
        }
        case DIRECTORY_RECORD_TYPE_FILE:
        {
            dialog->record_selected = record_index;
            dialog->pick->enabled = true;
            const char* filename = dialog->directory.records[record_index].name;

            switch(dialog->type)
            {
                case DIALOG_TYPE_IMPORT:
                {
                    ui_set_text(dialog->file_readout, filename, heap);
                    break;
                }
                case DIALOG_TYPE_EXPORT:
                {
                    ui_set_text(&dialog->filename_field->text_block, filename, heap);
                    break;
                }
            }

            break;
        }
        default:
        {
            break;
        }
    }
}

static void open_parent_directory(FilePickDialog* dialog, int segment, UiContext* context, Platform* platform, Heap* heap)
{
    const char* path = dialog->path;
    int slash = 0;
    if(segment == 0)
    {
        // the root directory case
        if(string_size(path) == 0)
        {
            slash = 1;
        }
        else
        {
            int found_index = find_char(path, '/');
            if(is_valid_index(found_index))
            {
                slash = found_index + 1;
            }
            else
            {
                slash = string_size(path) + 1;
            }
        }
    }
    else
    {
        for(int i = 0; i < segment + 1; i += 1)
        {
            int found_index = find_char(&path[slash], '/');
            if(is_valid_index(found_index))
            {
                slash += found_index + 1;
            }
            else
            {
                slash = string_size(path) + 1;
            }
        }
    }
    int subpath_size = slash;
    char* subpath = HEAP_ALLOCATE(heap, char, subpath_size);
    copy_string(subpath, subpath_size, path);

    list_directory(dialog, subpath, context, platform, heap);

    HEAP_DEALLOCATE(heap, subpath);
}

static void export_file(FilePickDialog* dialog, const char* name, ObjectLady* lady, int selected_object_index, UiContext* context, Heap* heap)
{
    char* path = append_to_path(dialog->path, name, heap);
    JanMesh* mesh = &lady->objects[selected_object_index].mesh;
    bool saved = obj_save_file(path, mesh, heap);
    HEAP_DEALLOCATE(heap, path);

    if(!saved)
    {
        // TODO: Report this to the user.
        ASSERT(false);
    }
    else
    {
        close_dialog(dialog, context, heap);
    }
}

static void import_file(FilePickDialog* dialog, const char* name, ObjectLady* lady, History* history, VideoContext* video_context, UiContext* ui_context, Heap* heap, Stack* stack)
{
    char* path = append_to_path(dialog->path, name, heap);
    JanMesh mesh;
    bool loaded = obj_load_file(path, &mesh, heap, stack);
    HEAP_DEALLOCATE(heap, path);

    if(!loaded)
    {
        // TODO: Report this to the user!
        ASSERT(false);
    }
    else
    {
        Object* imported_model = object_lady_add_object(lady, heap, video_context);
        imported_model->mesh = mesh;
        object_set_position(imported_model, (Float3){{-2.0f, 0.0f, 0.0f}}, video_context);

        jan_colour_all_faces(&imported_model->mesh, float3_magenta);
        video_update_mesh(video_context, imported_model->video_object, &imported_model->mesh, heap);

        add_object_to_history(history, imported_model, heap);

        Change change;
        change.type = CHANGE_TYPE_CREATE_OBJECT;
        change.create_object.object_id = imported_model->id;
        history_add(history, change);

        close_dialog(dialog, ui_context, heap);
    }
}

static void pick_file(FilePickDialog* dialog, ObjectLady* lady, int selected_object_index, History* history, VideoContext* video_context, UiContext* ui_context, Heap* heap, Stack* stack)
{
    int selected = dialog->record_selected;

    switch(dialog->type)
    {
        case DIALOG_TYPE_EXPORT:
        {
            char* filename;

            if(is_valid_index(selected))
            {
                DirectoryRecord record = dialog->directory.records[selected];
                filename = copy_string_to_stack(record.name, stack);
            }
            else
            {
                const char* name = dialog->filename_field->text_block.text;
                const char* extension = ".obj";

                if(string_ends_with(name, extension))
                {
                    filename = copy_string_to_stack(name, stack);
                }
                else
                {
                    int size = string_size(name) + string_size(extension) + 1;
                    filename = STACK_ALLOCATE(stack, char, size);
                    format_string(filename, size, "%s%s", name, extension);
                }
            }

            export_file(dialog, filename, lady, selected_object_index, ui_context, heap);
            STACK_DEALLOCATE(stack, filename);

            break;
        }
        case DIALOG_TYPE_IMPORT:
        {
            ASSERT(is_valid_index(selected));
            DirectoryRecord record = dialog->directory.records[selected];
            import_file(dialog, record.name, lady, history, video_context, ui_context, heap, stack);
            break;
        }
    }
}

void handle_input(FilePickDialog* dialog, UiEvent event, ObjectLady* lady, int selected_object_index, History* history, VideoContext* video_context, UiContext* ui_context, Platform* platform, Heap* heap, Stack* stack)
{
    switch(event.type)
    {
        case UI_EVENT_TYPE_BUTTON:
        {
            UiId id = event.button.id;

            for(int i = 0; i < dialog->path_buttons_count; i += 1)
            {
                if(id == dialog->path_buttons[i])
                {
                    open_parent_directory(dialog, i, ui_context, platform, heap);
                }
            }

            if(id == dialog->pick_button)
            {
                pick_file(dialog, lady, selected_object_index, history, video_context, ui_context, heap, stack);
            }
            break;
        }
        case UI_EVENT_TYPE_FOCUS_CHANGE:
        {
            UiId scope = event.focus_change.current_scope;
            if(scope != dialog->panel->id)
            {
                close_dialog(dialog, ui_context, heap);
            }

            if(dialog->type == DIALOG_TYPE_EXPORT)
            {
                UiId id = dialog->filename_field_id;

                UiId gained_focus = event.focus_change.now_focused;
                if(gained_focus == id)
                {
                    begin_composed_text(platform);
                }

                UiId lost_focus = event.focus_change.now_unfocused;
                if(lost_focus == id)
                {
                    end_composed_text(platform);
                }
            }
            break;
        }
        case UI_EVENT_TYPE_LIST_SELECTION:
        {
            int index = event.list_selection.index;
            bool expand = event.list_selection.expand;
            touch_record(dialog, index, expand, ui_context, platform, heap);
            break;
        }
        case UI_EVENT_TYPE_TEXT_CHANGE:
        {
            if(dialog->type == DIALOG_TYPE_EXPORT
                && event.text_change.id == dialog->filename_field_id)
            {
                int size = string_size(dialog->filename_field->text_block.text);
                dialog->pick->enabled = size != 0;

                // If a file was selected and the user starts editing the text
                // then the user may no longer be referring to the file.
                dialog->record_selected = invalid_index;
            }
            break;
        }
    }
}
