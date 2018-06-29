#include "input.h"

#include "string_utilities.h"

#define KEYS_COUNT 256
#define COMPOSED_TEXT_BUFFER_SIZE 16
#define HOTKEYS_COUNT 128

struct
{
    bool buttons_pressed[3];
    int edge_counts[3];
    Int2 position;
    Int2 velocity;
    Int2 scroll_velocity;
} mouse;

static struct
{
    InputModifier modifiers[KEYS_COUNT];
    bool keys_pressed[KEYS_COUNT];
    int edge_counts[KEYS_COUNT];
} keyboard;

static struct
{
    char buffer[COMPOSED_TEXT_BUFFER_SIZE];
} composed_text;

static Hotkey hotkeys[HOTKEYS_COUNT][2];
static const int auto_repeat_frames = 30;

void input_key_press(InputKey key, bool pressed, InputModifier modifier)
{
    int index = key;
    keyboard.keys_pressed[index] = pressed;
    keyboard.edge_counts[index] = 0;
    keyboard.modifiers[index] = modifier;
}

bool input_get_key_pressed(InputKey key)
{
    int index = key;
    return keyboard.keys_pressed[index];
}

bool input_get_key_tapped(InputKey key)
{
    int index = key;
    return keyboard.keys_pressed[index] && keyboard.edge_counts[index] == 0;
}

bool input_get_key_modified_by_control(InputKey key)
{
    int index = key;
    return keyboard.modifiers[index].control;
}

bool input_get_key_modified_by_shift(InputKey key)
{
    int index = key;
    return keyboard.modifiers[index].shift;
}

bool input_get_key_modified_by_alt(InputKey key)
{
    int index = key;
    return keyboard.modifiers[index].alt;
}

bool input_get_key_auto_repeated(InputKey key)
{
    int index = key;
    return keyboard.keys_pressed[index] && (keyboard.edge_counts[index] == 0 || keyboard.edge_counts[index] >= auto_repeat_frames);
}

void input_mouse_click(MouseButton button, bool pressed, InputModifier modifier)
{
    int index = button;
    mouse.buttons_pressed[index] = pressed;
    mouse.edge_counts[index] = 0;
}

void input_mouse_scroll(Int2 velocity)
{
    mouse.scroll_velocity = velocity;
}

void input_mouse_move(Int2 position)
{
    mouse.velocity = int2_subtract(position, mouse.position);
    mouse.position = position;
}

Int2 input_get_mouse_position()
{
    return mouse.position;
}

Int2 input_get_mouse_velocity()
{
    return mouse.velocity;
}

Int2 input_get_mouse_scroll_velocity()
{
    return mouse.scroll_velocity;
}

bool input_get_mouse_pressed(MouseButton button)
{
    int index = button;
    return mouse.buttons_pressed[index];
}

bool input_get_mouse_clicked(MouseButton button)
{
    int index = button;
    return mouse.buttons_pressed[index] && mouse.edge_counts[index] == 0;
}

void input_set_primary_hotkey(InputFunction function, Hotkey hotkey)
{
    int index = function;
    hotkeys[index][0] = hotkey;
}

void input_set_secondary_hotkey(InputFunction function, Hotkey hotkey)
{
    int index = function;
    hotkeys[index][1] = hotkey;
}

bool input_get_hotkey_pressed(InputFunction function)
{
    int index = function;
    bool result = false;
    for(int i = 0; i < 2; i += 1)
    {
        Hotkey hotkey = hotkeys[index][i];
        if(hotkey.key == INPUT_KEY_UNKNOWN)
        {
            continue;
        }

        result |= input_get_key_pressed(hotkey.key);

        int key_index = hotkey.key;
        InputModifier modifier = keyboard.modifiers[key_index];
        switch(hotkey.modifier)
        {
            default:
            case MODIFIER_COMBO_NONE:
            {
                break;
            }
            case MODIFIER_COMBO_ALT:
            {
                result &= modifier.alt && !modifier.control && !modifier.shift;
                break;
            }
            case MODIFIER_COMBO_ALT_SHIFT:
            {
                result &= modifier.alt && !modifier.control && modifier.shift;
                break;
            }
            case MODIFIER_COMBO_CONTROL:
            {
                result &= !modifier.alt && modifier.control && !modifier.shift;
                break;
            }
            case MODIFIER_COMBO_CONTROL_SHIFT:
            {
                result &= !modifier.alt && modifier.control && modifier.shift;
                break;
            }
            case MODIFIER_COMBO_SHIFT:
            {
                result &= !modifier.alt && !modifier.control && modifier.shift;
                break;
            }
        }
    }
    return result;
}

bool input_get_hotkey_tapped(InputFunction function)
{
    int index = function;
    bool result = false;
    for(int i = 0; i < 2; i += 1)
    {
        Hotkey hotkey = hotkeys[index][i];
        if(hotkey.key == INPUT_KEY_UNKNOWN)
        {
            continue;
        }

        result |= input_get_key_tapped(hotkey.key);

        int key_index = hotkey.key;
        InputModifier modifier = keyboard.modifiers[key_index];
        switch(hotkey.modifier)
        {
            default:
            case MODIFIER_COMBO_NONE:
            {
                break;
            }
            case MODIFIER_COMBO_ALT:
            {
                result &= modifier.alt && !modifier.control && !modifier.shift;
                break;
            }
            case MODIFIER_COMBO_ALT_SHIFT:
            {
                result &= modifier.alt && !modifier.control && modifier.shift;
                break;
            }
            case MODIFIER_COMBO_CONTROL:
            {
                result &= !modifier.alt && modifier.control && !modifier.shift;
                break;
            }
            case MODIFIER_COMBO_CONTROL_SHIFT:
            {
                result &= !modifier.alt && modifier.control && modifier.shift;
                break;
            }
            case MODIFIER_COMBO_SHIFT:
            {
                result &= !modifier.alt && !modifier.control && modifier.shift;
                break;
            }
        }
    }
    return result;
}

void input_composed_text_entered(char* text)
{
    copy_string(composed_text.buffer, COMPOSED_TEXT_BUFFER_SIZE, text);
}

char* input_get_composed_text()
{
    return composed_text.buffer;
}

void input_system_start_up()
{
    Hotkey copy;
    copy.key = INPUT_KEY_C;
    copy.modifier = MODIFIER_COMBO_CONTROL;
    input_set_primary_hotkey(INPUT_FUNCTION_COPY, copy);

    Hotkey cut;
    cut.key = INPUT_KEY_X;
    cut.modifier = MODIFIER_COMBO_CONTROL;
    input_set_primary_hotkey(INPUT_FUNCTION_CUT, cut);

    Hotkey delete_hotkey;
    delete_hotkey.key = INPUT_KEY_DELETE;
    delete_hotkey.modifier = MODIFIER_COMBO_NONE;
    input_set_primary_hotkey(INPUT_FUNCTION_DELETE, delete_hotkey);

    Hotkey paste;
    paste.key = INPUT_KEY_V;
    paste.modifier = MODIFIER_COMBO_CONTROL;
    input_set_primary_hotkey(INPUT_FUNCTION_PASTE, paste);

    Hotkey redo;
    redo.key = INPUT_KEY_Z;
    redo.modifier = MODIFIER_COMBO_CONTROL_SHIFT;
    input_set_primary_hotkey(INPUT_FUNCTION_REDO, redo);

    Hotkey select_all;
    select_all.key = INPUT_KEY_A;
    select_all.modifier = MODIFIER_COMBO_CONTROL;
    input_set_primary_hotkey(INPUT_FUNCTION_SELECT_ALL, select_all);

    Hotkey undo;
    undo.key = INPUT_KEY_Z;
    undo.modifier = MODIFIER_COMBO_CONTROL;
    input_set_primary_hotkey(INPUT_FUNCTION_UNDO, undo);
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
    for(int i = 0; i < KEYS_COUNT; i += 1)
    {
        keyboard.edge_counts[i] += 1;
    }
}

static void update_composed_text()
{
    composed_text.buffer[0] = '\0';
}

void input_system_update()
{
    update_button_change_counts();
    update_key_change_counts();
    update_composed_text();
}
