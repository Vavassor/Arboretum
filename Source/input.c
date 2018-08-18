#include "input.h"

#include "string_utilities.h"

#define KEYS_COUNT 256
#define COMPOSED_TEXT_BUFFER_SIZE 16
#define HOTKEYS_COUNT 128

typedef struct ComposedText
{
    char buffer[COMPOSED_TEXT_BUFFER_SIZE];
} ComposedText;

typedef struct Keyboard
{
    InputModifier modifiers[KEYS_COUNT];
    bool keys_pressed[KEYS_COUNT];
    int edge_counts[KEYS_COUNT];
} Keyboard;

typedef struct Mouse
{
    bool buttons_pressed[3];
    int edge_counts[3];
    Int2 position;
    Int2 velocity;
    Int2 scroll_velocity;
} Mouse;

struct InputContext
{
    ComposedText composed_text;
    Keyboard keyboard;
    Mouse mouse;
    Hotkey hotkeys[HOTKEYS_COUNT][2];
};

static const int auto_repeat_frames = 30;

void input_key_press(InputContext* context, InputKey key, bool pressed, InputModifier modifier)
{
    Keyboard* keyboard = &context->keyboard;
    keyboard->keys_pressed[key] = pressed;
    keyboard->edge_counts[key] = 0;
    keyboard->modifiers[key] = modifier;
}

bool input_get_key_pressed(InputContext* context, InputKey key)
{
    return context->keyboard.keys_pressed[key];
}

bool input_get_key_tapped(InputContext* context, InputKey key)
{
    Keyboard* keyboard = &context->keyboard;
    return keyboard->keys_pressed[key] && keyboard->edge_counts[key] == 0;
}

bool input_get_key_modified_by_control(InputContext* context, InputKey key)
{
    Keyboard* keyboard = &context->keyboard;
    return keyboard->modifiers[key].control;
}

bool input_get_key_modified_by_shift(InputContext* context, InputKey key)
{
    Keyboard* keyboard = &context->keyboard;
    return keyboard->modifiers[key].shift;
}

bool input_get_key_modified_by_alt(InputContext* context, InputKey key)
{
    Keyboard* keyboard = &context->keyboard;
    return keyboard->modifiers[key].alt;
}

bool input_get_key_auto_repeated(InputContext* context, InputKey key)
{
    Keyboard* keyboard = &context->keyboard;
    return keyboard->keys_pressed[key] && (keyboard->edge_counts[key] == 0 || keyboard->edge_counts[key] >= auto_repeat_frames);
}

void input_mouse_click(InputContext* context, MouseButton button, bool pressed, InputModifier modifier)
{
    Mouse* mouse = &context->mouse;
    mouse->buttons_pressed[button] = pressed;
    mouse->edge_counts[button] = 0;
}

void input_mouse_move(InputContext* context, Int2 position)
{
    Mouse* mouse = &context->mouse;
    mouse->velocity = int2_subtract(position, mouse->position);
    mouse->position = position;
}

void input_mouse_scroll(InputContext* context, Int2 velocity)
{
    Mouse* mouse = &context->mouse;
    mouse->scroll_velocity = velocity;
}

Int2 input_get_mouse_position(InputContext* context)
{
    Mouse* mouse = &context->mouse;
    return mouse->position;
}

Int2 input_get_mouse_velocity(InputContext* context)
{
    Mouse* mouse = &context->mouse;
    return mouse->velocity;
}

Int2 input_get_mouse_scroll_velocity(InputContext* context)
{
    Mouse* mouse = &context->mouse;
    return mouse->scroll_velocity;
}

bool input_get_mouse_pressed(InputContext* context, MouseButton button)
{
    Mouse* mouse = &context->mouse;
    return mouse->buttons_pressed[button];
}

bool input_get_mouse_clicked(InputContext* context, MouseButton button)
{
    Mouse* mouse = &context->mouse;
    return mouse->buttons_pressed[button] && mouse->edge_counts[button] == 0;
}

void input_set_primary_hotkey(InputContext* context, InputFunction function, Hotkey hotkey)
{
    context->hotkeys[function][0] = hotkey;
}

void input_set_secondary_hotkey(InputContext* context, InputFunction function, Hotkey hotkey)
{
    context->hotkeys[function][1] = hotkey;
}

bool input_get_hotkey_pressed(InputContext* context, InputFunction function)
{
    Keyboard* keyboard = &context->keyboard;

    bool result = false;
    for(int i = 0; i < 2; i += 1)
    {
        Hotkey hotkey = context->hotkeys[function][i];
        if(hotkey.key == INPUT_KEY_UNKNOWN)
        {
            continue;
        }

        result |= input_get_key_pressed(context, hotkey.key);

        int key_index = hotkey.key;
        InputModifier modifier = keyboard->modifiers[key_index];
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

bool input_get_hotkey_tapped(InputContext* context, InputFunction function)
{
    Keyboard* keyboard = &context->keyboard;

    bool result = false;
    for(int i = 0; i < 2; i += 1)
    {
        Hotkey hotkey = context->hotkeys[function][i];
        if(hotkey.key == INPUT_KEY_UNKNOWN)
        {
            continue;
        }

        result |= input_get_key_tapped(context, hotkey.key);

        int key_index = hotkey.key;
        InputModifier modifier = keyboard->modifiers[key_index];
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

void input_composed_text_entered(InputContext* context, char* text)
{
    copy_string(context->composed_text.buffer, COMPOSED_TEXT_BUFFER_SIZE, text);
}

char* input_get_composed_text(InputContext* context)
{
    return context->composed_text.buffer;
}

InputContext* input_create_context(Stack* stack)
{
    InputContext* context = STACK_ALLOCATE(stack, InputContext, 1);

    Hotkey copy;
    copy.key = INPUT_KEY_C;
    copy.modifier = MODIFIER_COMBO_CONTROL;
    input_set_primary_hotkey(context, INPUT_FUNCTION_COPY, copy);

    Hotkey cut;
    cut.key = INPUT_KEY_X;
    cut.modifier = MODIFIER_COMBO_CONTROL;
    input_set_primary_hotkey(context, INPUT_FUNCTION_CUT, cut);

    Hotkey delete_hotkey;
    delete_hotkey.key = INPUT_KEY_DELETE;
    delete_hotkey.modifier = MODIFIER_COMBO_NONE;
    input_set_primary_hotkey(context, INPUT_FUNCTION_DELETE, delete_hotkey);

    Hotkey paste;
    paste.key = INPUT_KEY_V;
    paste.modifier = MODIFIER_COMBO_CONTROL;
    input_set_primary_hotkey(context, INPUT_FUNCTION_PASTE, paste);

    Hotkey redo;
    redo.key = INPUT_KEY_Z;
    redo.modifier = MODIFIER_COMBO_CONTROL_SHIFT;
    input_set_primary_hotkey(context, INPUT_FUNCTION_REDO, redo);

    Hotkey select_all;
    select_all.key = INPUT_KEY_A;
    select_all.modifier = MODIFIER_COMBO_CONTROL;
    input_set_primary_hotkey(context, INPUT_FUNCTION_SELECT_ALL, select_all);

    Hotkey undo;
    undo.key = INPUT_KEY_Z;
    undo.modifier = MODIFIER_COMBO_CONTROL;
    input_set_primary_hotkey(context, INPUT_FUNCTION_UNDO, undo);

    return context;
}

static void update_button_change_counts(InputContext* context)
{
    Mouse* mouse = &context->mouse;
    for(int i = 0; i < 3; i += 1)
    {
        mouse->edge_counts[i] += 1;
    }
    mouse->velocity = int2_zero;
    mouse->scroll_velocity = int2_zero;
}

static void update_key_change_counts(InputContext* context)
{
    Keyboard* keyboard = &context->keyboard;
    for(int i = 0; i < KEYS_COUNT; i += 1)
    {
        keyboard->edge_counts[i] += 1;
    }
}

static void update_composed_text(InputContext* context)
{
    context->composed_text.buffer[0] = '\0';
}

void input_update_context(InputContext* context)
{
    update_button_change_counts(context);
    update_key_change_counts(context);
    update_composed_text(context);
}
