#include "input.h"

#include "string_utilities.h"

namespace input {

namespace
{
    struct
    {
        bool buttons_pressed[3];
        int edge_counts[3];
        Int2 position;
        Int2 velocity;
        Int2 scroll_velocity;
    } mouse;

    const int keys_count = 256;
    struct
    {
        Modifier modifiers[keys_count];
        bool keys_pressed[keys_count];
        int edge_counts[keys_count];
    } keyboard;

    const int composed_text_buffer_size = 16;
    struct
    {
        char buffer[composed_text_buffer_size];
    } composed_text;

    const int hotkeys_count = 128;
    Hotkey hotkeys[hotkeys_count][2];

    const int auto_repeat_frames = 30;
}

void key_press(Key key, bool pressed, Modifier modifier)
{
    int index = static_cast<int>(key);
    keyboard.keys_pressed[index] = pressed;
    keyboard.edge_counts[index] = 0;
    keyboard.modifiers[index] = modifier;
}

bool get_key_pressed(Key key)
{
    int index = static_cast<int>(key);
    return keyboard.keys_pressed[index];
}

bool get_key_tapped(Key key)
{
    int index = static_cast<int>(key);
    return keyboard.keys_pressed[index] && keyboard.edge_counts[index] == 0;
}

bool get_key_modified_by_control(Key key)
{
    int index = static_cast<int>(key);
    return keyboard.modifiers[index].control;
}

bool get_key_modified_by_shift(Key key)
{
    int index = static_cast<int>(key);
    return keyboard.modifiers[index].shift;
}

bool get_key_modified_by_alt(Key key)
{
    int index = static_cast<int>(key);
    return keyboard.modifiers[index].alt;
}

bool get_key_auto_repeated(Key key)
{
    int index = static_cast<int>(key);
    return keyboard.keys_pressed[index] && (keyboard.edge_counts[index] == 0 || keyboard.edge_counts[index] >= auto_repeat_frames);
}

void mouse_click(MouseButton button, bool pressed, Modifier modifier)
{
    int index = static_cast<int>(button);
    mouse.buttons_pressed[index] = pressed;
    mouse.edge_counts[index] = 0;
}

void mouse_scroll(Int2 velocity)
{
    mouse.scroll_velocity = velocity;
}

void mouse_move(Int2 position)
{
    mouse.velocity = int2_subtract(position, mouse.position);
    mouse.position = position;
}

Int2 get_mouse_position()
{
    return mouse.position;
}

Int2 get_mouse_velocity()
{
    return mouse.velocity;
}

Int2 get_mouse_scroll_velocity()
{
    return mouse.scroll_velocity;
}

bool get_mouse_pressed(MouseButton button)
{
    int index = static_cast<int>(button);
    return mouse.buttons_pressed[index];
}

bool get_mouse_clicked(MouseButton button)
{
    int index = static_cast<int>(button);
    return mouse.buttons_pressed[index] && mouse.edge_counts[index] == 0;
}

void set_primary_hotkey(Function function, Hotkey hotkey)
{
    int index = static_cast<int>(function);
    hotkeys[index][0] = hotkey;
}

void set_secondary_hotkey(Function function, Hotkey hotkey)
{
    int index = static_cast<int>(function);
    hotkeys[index][1] = hotkey;
}

bool get_hotkey_pressed(Function function)
{
    int index = static_cast<int>(function);
    bool result = false;
    for(int i = 0; i < 2; i += 1)
    {
        Hotkey hotkey = hotkeys[index][i];
        if(hotkey.key == Key::Unknown)
        {
            continue;
        }

        result |= get_key_pressed(hotkey.key);

        int key_index = static_cast<int>(hotkey.key);
        Modifier modifier = keyboard.modifiers[key_index];
        switch(hotkey.modifier)
        {
            default:
            case ModifierCombo::None:
            {
                break;
            }
            case ModifierCombo::Alt:
            {
                result &= modifier.alt && !modifier.control && !modifier.shift;
                break;
            }
            case ModifierCombo::Alt_Shift:
            {
                result &= modifier.alt && !modifier.control && modifier.shift;
                break;
            }
            case ModifierCombo::Control:
            {
                result &= !modifier.alt && modifier.control && !modifier.shift;
                break;
            }
            case ModifierCombo::Control_Shift:
            {
                result &= !modifier.alt && modifier.control && modifier.shift;
                break;
            }
            case ModifierCombo::Shift:
            {
                result &= !modifier.alt && !modifier.control && modifier.shift;
                break;
            }
        }
    }
    return result;
}

bool get_hotkey_tapped(Function function)
{
    int index = static_cast<int>(function);
    bool result = false;
    for(int i = 0; i < 2; i += 1)
    {
        Hotkey hotkey = hotkeys[index][i];
        if(hotkey.key == Key::Unknown)
        {
            continue;
        }

        result |= get_key_tapped(hotkey.key);

        int key_index = static_cast<int>(hotkey.key);
        Modifier modifier = keyboard.modifiers[key_index];
        switch(hotkey.modifier)
        {
            default:
            case ModifierCombo::None:
            {
                break;
            }
            case ModifierCombo::Alt:
            {
                result &= modifier.alt && !modifier.control && !modifier.shift;
                break;
            }
            case ModifierCombo::Alt_Shift:
            {
                result &= modifier.alt && !modifier.control && modifier.shift;
                break;
            }
            case ModifierCombo::Control:
            {
                result &= !modifier.alt && modifier.control && !modifier.shift;
                break;
            }
            case ModifierCombo::Control_Shift:
            {
                result &= !modifier.alt && modifier.control && modifier.shift;
                break;
            }
            case ModifierCombo::Shift:
            {
                result &= !modifier.alt && !modifier.control && modifier.shift;
                break;
            }
        }
    }
    return result;
}

void composed_text_entered(char* text)
{
    copy_string(composed_text.buffer, composed_text_buffer_size, text);
}

char* get_composed_text()
{
    return composed_text.buffer;
}

void system_start_up()
{
    Hotkey copy;
    copy.key = Key::C;
    copy.modifier = ModifierCombo::Control;
    set_primary_hotkey(Function::Copy, copy);

    Hotkey cut;
    cut.key = Key::X;
    cut.modifier = ModifierCombo::Control;
    set_primary_hotkey(Function::Cut, cut);

    Hotkey delete_hotkey;
    delete_hotkey.key = Key::Delete;
    delete_hotkey.modifier = ModifierCombo::None;
    set_primary_hotkey(Function::Delete, delete_hotkey);

    Hotkey paste;
    paste.key = Key::V;
    paste.modifier = ModifierCombo::Control;
    set_primary_hotkey(Function::Paste, paste);

    Hotkey redo;
    redo.key = Key::Z;
    redo.modifier = ModifierCombo::Control_Shift;
    set_primary_hotkey(Function::Redo, redo);

    Hotkey select_all;
    select_all.key = Key::A;
    select_all.modifier = ModifierCombo::Control;
    set_primary_hotkey(Function::Select_All, select_all);

    Hotkey undo;
    undo.key = Key::Z;
    undo.modifier = ModifierCombo::Control;
    set_primary_hotkey(Function::Undo, undo);
}

static void update_button_change_counts()
{
    for(int i = 0; i < 3; i += 1)
    {
        mouse.edge_counts[i] += 1;
    }
    mouse.velocity = int2_zero;
    mouse.scroll_velocity = int2_zero;
}

static void update_key_change_counts()
{
    for(int i = 0; i < keys_count; i += 1)
    {
        keyboard.edge_counts[i] += 1;
    }
}

static void update_composed_text()
{
    composed_text.buffer[0] = '\0';
}

void system_update()
{
    update_button_change_counts();
    update_key_change_counts();
    update_composed_text();
}

} // namespace input
