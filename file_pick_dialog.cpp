#include "file_pick_dialog.h"

#include "array2.h"
#include "assert.h"
#include "colours.h"
#include "logging.h"
#include "loop_macros.h"
#include "obj.h"
#include "platform.h"
#include "sorting.h"
#include "string_build.h"
#include "string_utilities.h"

static bool record_is_before(DirectoryRecord a, DirectoryRecord b)
{
    if(a.type == DirectoryRecordType::Directory && b.type == DirectoryRecordType::File)
    {
        return true;
    }
    if(a.type == DirectoryRecordType::File && b.type == DirectoryRecordType::Directory)
    {
        return false;
    }
    return compare_alphabetic_ascii(a.name, b.name) < 0;
}

DEFINE_QUICK_SORT(DirectoryRecord, record_is_before, by_filename);

static void filter_directory(Directory* directory, const char** extensions, int extensions_count, bool show_hidden, Heap* heap)
{
    DirectoryRecord* filtered = nullptr;

    FOR_N(i, directory->records_count)
    {
        DirectoryRecord record = directory->records[i];
        if(!show_hidden && record.hidden)
        {
            continue;
        }
        if(record.type == DirectoryRecordType::File)
        {
            FOR_N(j, extensions_count)
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
    directory->records_count = ARRAY_COUNT(filtered);
}

static void list_directory(FilePickDialog* dialog, const char* directory, bmfont::Font* font, ui::Context* context, Platform* platform, Heap* heap)
{
    // Attempt to list the directory, first.
    Directory new_directory = {};
    bool listed = list_files_in_directory(directory, &new_directory, heap);
    if(!listed)
    {
        LOG_ERROR("Failed to open the directory %s.", directory);
        destroy_directory(&new_directory, heap);
        return;
    }

    // Clean up a previous directory if there was one.
    ui::Item* panel = dialog->panel;

    if(dialog->path_buttons_count > 0)
    {
        empty_item(context, &panel->container.items[0]);
    }
    if(dialog->directory.records_count > 0)
    {
        empty_item(context, &panel->container.items[1]);
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
    dialog->path_buttons = HEAP_ALLOCATE(heap, ui::Id, buttons_in_row);

    ui::Item* path_bar = &panel->container.items[0];
    path_bar->type = ui::ItemType::Container;
    path_bar->container.background_colour = vector4_yellow;
    ui::add_row(&path_bar->container, buttons_in_row, context, heap);

    // Add buttons to the path bar.
    int i = 0;
    for(const char* path = dialog->path; *path; i += 1)
    {
        ui::Item* item = &path_bar->container.items[i];
        item->type = ui::ItemType::Button;

        ui::Button* button = &item->button;
        button->enabled = true;
        button->text_block.padding = {4.0f, 4.0f, 4.0f, 4.0f};
        button->text_block.font = font;
        button->text_block.text_overflow = ui::TextOverflow::Ellipsize_End;
        dialog->path_buttons[i] = item->id;

        int found_index = find_char(path, '/');
        if(found_index == 0)
        {
            // root directory
            const char* name = platform->localized_text.file_pick_dialog_filesystem;
            ui::set_text(&button->text_block, name, heap);
        }
        else if(found_index == invalid_index)
        {
            // final path segment
            ui::set_text(&button->text_block, path, heap);
            break;
        }
        else
        {
            char* temp = copy_chars_to_heap(path, found_index, heap);
            ui::set_text(&button->text_block, temp, heap);
            HEAP_DEALLOCATE(heap, temp);
        }
        path += found_index + 1;
    }

    // Set up the directory listing.
    ui::Item* file_list = &panel->container.items[1];
    file_list->type = ui::ItemType::List;
    file_list->growable = true;

    ui::List* list = &file_list->list;
    list->item_spacing = 2.0f;
    list->side_margin = 2.0f;
    list->scroll_top = 0.0f;

    const int extensions_count = 1;
    const char* extensions[extensions_count] = {".obj"};
    filter_directory(&dialog->directory, extensions, extensions_count, dialog->show_hidden_records, heap);

    ui::create_items(file_list, dialog->directory.records_count, heap);

    if(dialog->directory.records_count == 0)
    {
        // Listing the directory succeeded, it's just empty.
    }
    else
    {
        quick_sort_by_filename(dialog->directory.records, dialog->directory.records_count);

        FOR_N(i, dialog->directory.records_count)
        {
            DirectoryRecord record = dialog->directory.records[i];
            ui::TextBlock* text_block = &list->items[i];
            text_block->padding = {1.0f, 1.0f, 1.0f, 1.0f};
            text_block->font = font;
            text_block->text_overflow = ui::TextOverflow::Ellipsize_End;
            ui::set_text(text_block, record.name, heap);
        }
    }

    ui::focus_on_item(context, file_list);
}

static void open_directory(FilePickDialog* dialog, const char* directory, bmfont::Font* font, ui::Context* context, Platform* platform, Heap* heap)
{
    dialog->enabled = true;

    // Create the panel for the dialog box.
    ui::Item* panel = ui::create_toplevel_container(context, heap);
    panel->type = ui::ItemType::Container;
    panel->growable = true;
    panel->container.alignment = ui::Alignment::Stretch;
    dialog->panel = panel;

    ui::add_column(&panel->container, 3, context, heap);

    list_directory(dialog, directory, font, context, platform, heap);

    // Set up the footer.
    ui::Item* footer = &panel->container.items[2];
    footer->type = ui::ItemType::Container;
    footer->container.background_colour = vector4_blue;
    footer->container.justification = ui::Justification::Space_Between;

    ui::add_row(&footer->container, 2, context, heap);

    ui::Item* file_readout = &footer->container.items[0];
    file_readout->type = ui::ItemType::Text_Block;
    file_readout->text_block.padding = {4.0f, 4.0f, 4.0f, 4.0f};
    file_readout->text_block.font = font;
    ui::set_text(&file_readout->text_block, " ", heap);
    dialog->file_readout = &file_readout->text_block;

    ui::Item* pick = &footer->container.items[1];
    pick->type = ui::ItemType::Button;
    pick->button.text_block.padding = {4.0f, 4.0f, 4.0f, 4.0f};
    pick->button.text_block.font = font;
    ui::set_text(&pick->button.text_block, platform->localized_text.file_pick_dialog_import, heap);
    dialog->pick_button = pick->id;
    dialog->pick = &pick->button;
}

void open_dialog(FilePickDialog* dialog, bmfont::Font* font, ui::Context* context, Platform* platform, Heap* heap)
{
    const char* default_path = get_documents_folder(heap);
    open_directory(dialog, default_path, font, context, platform, heap);
}

void close_dialog(FilePickDialog* dialog, ui::Context* context, Heap* heap)
{
    if(dialog)
    {
        SAFE_HEAP_DEALLOCATE(heap, dialog->path);
        SAFE_HEAP_DEALLOCATE(heap, dialog->path_buttons);

        if(dialog->panel)
        {
            ui::destroy_toplevel_container(context, dialog->panel, heap);
            dialog->panel = nullptr;
        }

        destroy_directory(&dialog->directory, heap);
        dialog->enabled = false;
    }
}

static void touch_record(FilePickDialog* dialog, int record_index, bool expand, bmfont::Font* font, ui::Context* context, Platform* platform, Heap* heap)
{
    DirectoryRecord record = dialog->directory.records[record_index];
    switch(record.type)
    {
        case DirectoryRecordType::Directory:
        {
            ui::set_text(dialog->file_readout, "", heap);
            dialog->record_selected = invalid_index;
            dialog->pick->enabled = false;

            if(expand)
            {
                char* path = append_to_path(dialog->path, record.name, heap);

                list_directory(dialog, path, font, context, platform, heap);

                HEAP_DEALLOCATE(heap, path);
            }
            break;
        }
        case DirectoryRecordType::File:
        {
            dialog->record_selected = record_index;
            dialog->pick->enabled = true;
            const char* filename = dialog->directory.records[record_index].name;
            ui::set_text(dialog->file_readout, filename, heap);
            break;
        }
        default:
        {
            break;
        }
    }
}

static void open_parent_directory(FilePickDialog* dialog, int segment, bmfont::Font* font, ui::Context* context, Platform* platform, Heap* heap)
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
            slash = 2;
        }
    }
    else
    {
        FOR_N(i, segment + 1)
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

    list_directory(dialog, subpath, font, context, platform, heap);

    HEAP_DEALLOCATE(heap, subpath);
}

static void pick_file(FilePickDialog* dialog, const char* name, ObjectLady* lady, History* history, ui::Context* context, Heap* heap, Stack* stack)
{
    char* path = append_to_path(dialog->path, name, heap);
    jan::Mesh mesh;
    bool loaded = obj::load_file(path, &mesh, heap, stack);
    HEAP_DEALLOCATE(heap, path);

    if(!loaded)
    {
        // TODO: Report this to the user, not the log!
        LOG_DEBUG("Failed to load the file %s as an .obj.", name);
    }
    else
    {
        Object* imported_model = add_object(lady, heap);
        imported_model->mesh = mesh;
        object_set_position(imported_model, {-2.0f, 0.0f, 0.0f});

        jan::colour_all_faces(&imported_model->mesh, vector3_magenta);
        video::Object* video_object = video::get_object(imported_model->video_object);
        video::object_update_mesh(video_object, &imported_model->mesh, heap);

        add_object_to_history(history, imported_model, heap);

        Change change;
        change.type = ChangeType::Create_Object;
        change.create_object.object_id = imported_model->id;
        history_add(history, change);

        close_dialog(dialog, context, heap);
    }
}

void handle_input(FilePickDialog* dialog, ui::Event event, bmfont::Font* font, ObjectLady* lady, History* history, ui::Context* context, Platform* platform, Heap* heap, Stack* stack)
{
    switch(event.type)
    {
        case ui::EventType::Button:
        {
            ui::Id id = event.button.id;

            FOR_N(i, dialog->path_buttons_count)
            {
                if(id == dialog->path_buttons[i])
                {
                    open_parent_directory(dialog, i, font, context, platform, heap);
                }
            }

            if(id == dialog->pick_button)
            {
                int selected = dialog->record_selected;
                DirectoryRecord record = dialog->directory.records[selected];
                pick_file(dialog, record.name, lady, history, context, heap, stack);
            }
            break;
        }
        case ui::EventType::Focus_Change:
        {
            ui::Id id = event.focus_change.current_scope;
            if(id != dialog->panel->id)
            {
                close_dialog(dialog, context, heap);
            }
            break;
        }
        case ui::EventType::List_Selection:
        {
            int index = event.list_selection.index;
            bool expand = event.list_selection.expand;
            touch_record(dialog, index, expand, font, context, platform, heap);
            break;
        }
    }
}
