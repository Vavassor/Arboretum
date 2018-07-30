#ifndef INPUT_H_
#define INPUT_H_

#include "int2.h"
#include "memory.h"

#include <stdbool.h>

typedef struct InputModifier
{
    bool shift : 1;
    bool control : 1;
    bool alt : 1;
    bool super : 1;
    bool caps_lock : 1;
    bool num_lock : 1;
} InputModifier;

typedef enum InputKey
{
    INPUT_KEY_UNKNOWN,
    INPUT_KEY_A,
    INPUT_KEY_APOSTROPHE,
    INPUT_KEY_B,
    INPUT_KEY_BACKSLASH,
    INPUT_KEY_BACKSPACE,
    INPUT_KEY_C,
    INPUT_KEY_COMMA,
    INPUT_KEY_D,
    INPUT_KEY_DELETE,
    INPUT_KEY_DOWN_ARROW,
    INPUT_KEY_E,
    INPUT_KEY_EIGHT,
    INPUT_KEY_END,
    INPUT_KEY_ENTER,
    INPUT_KEY_EQUALS_SIGN,
    INPUT_KEY_ESCAPE,
    INPUT_KEY_F,
    INPUT_KEY_F1,
    INPUT_KEY_F2,
    INPUT_KEY_F3,
    INPUT_KEY_F4,
    INPUT_KEY_F5,
    INPUT_KEY_F6,
    INPUT_KEY_F7,
    INPUT_KEY_F8,
    INPUT_KEY_F9,
    INPUT_KEY_F10,
    INPUT_KEY_F11,
    INPUT_KEY_F12,
    INPUT_KEY_FIVE,
    INPUT_KEY_FOUR,
    INPUT_KEY_G,
    INPUT_KEY_GRAVE_ACCENT,
    INPUT_KEY_H,
    INPUT_KEY_HOME,
    INPUT_KEY_I,
    INPUT_KEY_INSERT,
    INPUT_KEY_J,
    INPUT_KEY_K,
    INPUT_KEY_L,
    INPUT_KEY_LEFT_ARROW,
    INPUT_KEY_LEFT_BRACKET,
    INPUT_KEY_M,
    INPUT_KEY_MINUS,
    INPUT_KEY_N,
    INPUT_KEY_NINE,
    INPUT_KEY_NUMPAD_0,
    INPUT_KEY_NUMPAD_1,
    INPUT_KEY_NUMPAD_2,
    INPUT_KEY_NUMPAD_3,
    INPUT_KEY_NUMPAD_4,
    INPUT_KEY_NUMPAD_5,
    INPUT_KEY_NUMPAD_6,
    INPUT_KEY_NUMPAD_7,
    INPUT_KEY_NUMPAD_8,
    INPUT_KEY_NUMPAD_9,
    INPUT_KEY_NUMPAD_DECIMAL,
    INPUT_KEY_NUMPAD_DIVIDE,
    INPUT_KEY_NUMPAD_ENTER,
    INPUT_KEY_NUMPAD_SUBTRACT,
    INPUT_KEY_NUMPAD_MULTIPLY,
    INPUT_KEY_NUMPAD_ADD,
    INPUT_KEY_O,
    INPUT_KEY_ONE,
    INPUT_KEY_P,
    INPUT_KEY_PAGE_DOWN,
    INPUT_KEY_PAGE_UP,
    INPUT_KEY_PAUSE,
    INPUT_KEY_PERIOD,
    INPUT_KEY_Q,
    INPUT_KEY_R,
    INPUT_KEY_RIGHT_ARROW,
    INPUT_KEY_RIGHT_BRACKET,
    INPUT_KEY_S,
    INPUT_KEY_SEMICOLON,
    INPUT_KEY_SEVEN,
    INPUT_KEY_SIX,
    INPUT_KEY_SLASH,
    INPUT_KEY_SPACE,
    INPUT_KEY_T,
    INPUT_KEY_TAB,
    INPUT_KEY_THREE,
    INPUT_KEY_TWO,
    INPUT_KEY_U,
    INPUT_KEY_UP_ARROW,
    INPUT_KEY_V,
    INPUT_KEY_W,
    INPUT_KEY_X,
    INPUT_KEY_Y,
    INPUT_KEY_Z,
    INPUT_KEY_ZERO,
} InputKey;

typedef enum MouseButton
{
    MOUSE_BUTTON_LEFT,
    MOUSE_BUTTON_MIDDLE,
    MOUSE_BUTTON_RIGHT,
} MouseButton;

typedef enum InputFunction
{
    INPUT_FUNCTION_COPY,
    INPUT_FUNCTION_CUT,
    INPUT_FUNCTION_DELETE,
    INPUT_FUNCTION_PASTE,
    INPUT_FUNCTION_REDO,
    INPUT_FUNCTION_SELECT_ALL,
    INPUT_FUNCTION_UNDO,
} InputFunction;

typedef enum ModifierCombo
{
    MODIFIER_COMBO_NONE,
    MODIFIER_COMBO_ALT,
    MODIFIER_COMBO_ALT_SHIFT,
    MODIFIER_COMBO_CONTROL,
    MODIFIER_COMBO_CONTROL_SHIFT,
    MODIFIER_COMBO_SHIFT,
} ModifierCombo;

typedef struct Hotkey
{
    ModifierCombo modifier;
    InputKey key;
} Hotkey;

typedef struct InputContext InputContext;

void input_key_press(InputContext* context, InputKey key, bool pressed, InputModifier modifier);
bool input_get_key_pressed(InputContext* context, InputKey key);
bool input_get_key_tapped(InputContext* context, InputKey key);
bool input_get_key_modified_by_control(InputContext* context, InputKey key);
bool input_get_key_modified_by_shift(InputContext* context, InputKey key);
bool input_get_key_modified_by_alt(InputContext* context, InputKey key);
bool input_get_key_auto_repeated(InputContext* context, InputKey key);

void input_mouse_click(InputContext* context, MouseButton button, bool pressed, InputModifier modifier);
void input_mouse_scroll(InputContext* context, Int2 velocity);
void input_mouse_move(InputContext* context, Int2 position);
Int2 input_get_mouse_position(InputContext* context);
Int2 input_get_mouse_velocity(InputContext* context);
Int2 input_get_mouse_scroll_velocity(InputContext* context);
bool input_get_mouse_pressed(InputContext* context, MouseButton button);
bool input_get_mouse_clicked(InputContext* context, MouseButton button);

void input_set_primary_hotkey(InputContext* context, InputFunction function, Hotkey hotkey);
void input_set_secondary_hotkey(InputContext* context, InputFunction function, Hotkey hotkey);
bool input_get_hotkey_pressed(InputContext* context, InputFunction function);
bool input_get_hotkey_tapped(InputContext* context, InputFunction function);

void input_composed_text_entered(InputContext* context, char* text);
char* input_get_composed_text(InputContext* context);

InputContext* input_create_context(Stack* stack);
void input_update_context(InputContext* context);

#endif // INPUT_H_
