#ifndef INPUT_H_
#define INPUT_H_

#include "int2.h"

namespace input {

struct Modifier
{
    bool shift : 1;
    bool control : 1;
    bool alt : 1;
    bool super : 1;
    bool caps_lock : 1;
    bool num_lock : 1;
};

enum class Key
{
    Unknown,
    A,
    Apostrophe,
    B,
    Backslash,
    Backspace,
    C,
    Comma,
    D,
    Delete,
    Down_Arrow,
    E,
    Eight,
    End,
    Enter,
    Equals_Sign,
    Escape,
    F,
    F1,
    F2,
    F3,
    F4,
    F5,
    F6,
    F7,
    F8,
    F9,
    F10,
    F11,
    F12,
    Five,
    Four,
    G,
    Grave_Accent,
    H,
    Home,
    I,
    Insert,
    J,
    K,
    L,
    Left_Arrow,
    Left_Bracket,
    M,
    Minus,
    N,
    Nine,
    Numpad_0,
    Numpad_1,
    Numpad_2,
    Numpad_3,
    Numpad_4,
    Numpad_5,
    Numpad_6,
    Numpad_7,
    Numpad_8,
    Numpad_9,
    Numpad_Decimal,
    Numpad_Divide,
    Numpad_Enter,
    Numpad_Subtract,
    Numpad_Multiply,
    Numpad_Add,
    O,
    One,
    P,
    Page_Down,
    Page_Up,
    Pause,
    Period,
    Q,
    R,
    Right_Arrow,
    Right_Bracket,
    S,
    Semicolon,
    Seven,
    Six,
    Slash,
    Space,
    T,
    Tab,
    Three,
    Two,
    U,
    Up_Arrow,
    V,
    W,
    X,
    Y,
    Z,
    Zero,
};

enum class MouseButton
{
    Left,
    Middle,
    Right,
};

enum class Function
{
    Copy,
    Cut,
    Delete,
    Paste,
    Redo,
    Select_All,
    Undo,
};

enum class ModifierCombo
{
    None,
    Alt,
    Alt_Shift,
    Control,
    Control_Shift,
    Shift,
};

struct Hotkey
{
    ModifierCombo modifier;
    Key key;
};

void key_press(Key key, bool pressed, Modifier modifier);
bool get_key_pressed(Key key);
bool get_key_tapped(Key key);
bool get_key_modified_by_control(Key key);
bool get_key_modified_by_shift(Key key);
bool get_key_modified_by_alt(Key key);
bool get_key_auto_repeated(Key key);

void mouse_click(MouseButton button, bool pressed, Modifier modifier);
void mouse_scroll(Int2 velocity);
void mouse_move(Int2 position);
Int2 get_mouse_position();
Int2 get_mouse_velocity();
Int2 get_mouse_scroll_velocity();
bool get_mouse_pressed(MouseButton button);
bool get_mouse_clicked(MouseButton button);

void set_primary_hotkey(Function function, Hotkey hotkey);
void set_secondary_hotkey(Function function, Hotkey hotkey);
bool get_hotkey_pressed(Function function);
bool get_hotkey_tapped(Function function);

void composed_text_entered(char* text);
char* get_composed_text();

void system_start_up();
void system_update();

} // namespace input

#endif // INPUT_H_
