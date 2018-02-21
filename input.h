#ifndef INPUT_H_
#define INPUT_H_

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
	Space,
	F,
	G,
	Z,
};

enum class MouseButton
{
	Left,
	Middle,
	Right,
};

void key_press(Key key, bool pressed, Modifier modifier);
bool get_key_pressed(Key key);
bool get_key_tapped(Key key);

void mouse_click(MouseButton button, bool pressed, Modifier modifier);
void mouse_scroll(int x, int y);
void mouse_move(int x, int y);
void get_mouse_position(int* x, int* y);
void get_mouse_velocity(int* x, int* y);
void get_mouse_scroll_velocity(int* x, int* y);
bool get_mouse_pressed(MouseButton button);
bool get_mouse_clicked(MouseButton button);

void system_update();

} // namespace input

#endif // INPUT_H_
