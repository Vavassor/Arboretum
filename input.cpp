#include "input.h"

#include "logging.h"

namespace input {

namespace
{
    struct
    {
        bool buttons_pressed[3];
        int edge_counts[3];
        int x;
        int y;
        int velocity_x;
        int velocity_y;
        int scroll_velocity_x;
        int scroll_velocity_y;
    } mouse;

    const int keys_count = 256;
    struct
    {
        bool keys_pressed[keys_count];
        int edge_counts[keys_count];
    } keyboard;
}

void key_press(Key key, bool pressed, Modifier modifier)
{
    int index = static_cast<int>(key);
    keyboard.keys_pressed[index] = pressed;
    keyboard.edge_counts[index] = 0;
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

void mouse_click(MouseButton button, bool pressed, Modifier modifier)
{
    int index = static_cast<int>(button);
    mouse.buttons_pressed[index] = pressed;
    mouse.edge_counts[index] = 0;
}

void mouse_scroll(int x, int y)
{
    mouse.scroll_velocity_x = x;
    mouse.scroll_velocity_y = y;
}

void mouse_move(int x, int y)
{
    mouse.velocity_x = x - mouse.x;
    mouse.velocity_y = y - mouse.y;
    mouse.x = x;
    mouse.y = y;
}

void get_mouse_position(int* x, int* y)
{
    *x = mouse.x;
    *y = mouse.y;
}

void get_mouse_velocity(int* x, int* y)
{
    *x = mouse.velocity_x;
    *y = mouse.velocity_y;
}

void get_mouse_scroll_velocity(int* x, int* y)
{
    *x = mouse.scroll_velocity_x;
    *y = mouse.scroll_velocity_y;
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

static void update_button_change_counts()
{
    for(int i = 0; i < 3; ++i)
    {
        mouse.edge_counts[i] += 1;
    }
    mouse.velocity_x = 0;
    mouse.velocity_y = 0;
    mouse.scroll_velocity_x = 0;
    mouse.scroll_velocity_y = 0;
}

static void update_key_change_counts()
{
    for(int i = 0; i < keys_count; ++i)
    {
        keyboard.edge_counts[i] += 1;
    }
}

void system_update()
{
    update_button_change_counts();
    update_key_change_counts();
}

} // namespace input
