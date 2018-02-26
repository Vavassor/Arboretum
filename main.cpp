#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xresource.h>
#include <X11/Xcursor/Xcursor.h>

#ifdef None
#undef None
#define X11_NONE 0L /* universal null resource or null atom */
#endif

#include "editor.h"
#include "gl_core_3_3.h"
#include "glx_extensions.h"
#include "video.h"
#include "input.h"
#include "logging.h"
#include "assert.h"
#include "sized_types.h"
#include "string_utilities.h"
#include "platform.h"

#include <ctime>

struct PlatformX11
{
    Platform base;
    input::Key key_table[256];
    Display* display;
    XVisualInfo* visual_info;
    Colormap colormap;
    Window window;
    Atom wm_delete_window;
    int screen;
    bool close_window_requested;
};

static double get_dots_per_millimeter(PlatformX11* platform)
{
    char* resource = XResourceManagerString(platform->display);

    XrmInitialize();
    XrmDatabase database = XrmGetStringDatabase(resource);

    double dots_per_millimeter = 0.0;
    if(resource)
    {
        XrmValue value;
        char* type = nullptr;
        if(XrmGetResource(database, "Xft.dpi", "String", &type, &value) == True)
        {
            if(value.addr)
            {
                double dots_per_inch;
                bool success = string_to_double(value.addr, &dots_per_inch);
                if(success)
                {
                    dots_per_millimeter = dots_per_inch / 25.4;
                }
            }
        }
    }

    if(dots_per_millimeter == 0.0)
    {
        double height = DisplayHeight(platform->display, platform->screen);
        double millimeters = DisplayHeightMM(platform->display, platform->screen);
        dots_per_millimeter = height / millimeters;
    }

    return dots_per_millimeter;
}

static const char* translate_cursor_type(CursorType type)
{
    switch(type)
    {
        case CursorType::Arrow:         return "left_ptr";
        case CursorType::Hand_Pointing: return "hand1";
    }
}

void change_cursor(Platform* base, CursorType type)
{
    PlatformX11* platform = reinterpret_cast<PlatformX11*>(base);
    const char* name = translate_cursor_type(type);
    Cursor cursor = XcursorLibraryLoadCursor(platform->display, name);
    XDefineCursor(platform->display, platform->window, cursor);
}

namespace
{
    const char* app_name = "Arboretum";
    const int window_width = 800;
    const int window_height = 600;

    PlatformX11 platform;
    GLXContext rendering_context;
    bool functions_loaded;
}

static input::Key translate_key_sym(KeySym key_sym)
{
    switch(key_sym)
    {
        case XK_a:
        case XK_A:            return input::Key::A;
        case XK_apostrophe:   return input::Key::Apostrophe;
        case XK_b:
        case XK_B:            return input::Key::B;
        case XK_backslash:    return input::Key::Backslash;
        case XK_BackSpace:    return input::Key::Backspace;
        case XK_c:
        case XK_C:            return input::Key::C;
        case XK_comma:        return input::Key::Comma;
        case XK_d:
        case XK_D:            return input::Key::D;
        case XK_Delete:       return input::Key::Delete;
        case XK_Down:         return input::Key::Down_Arrow;
        case XK_e:
        case XK_E:            return input::Key::E;
        case XK_8:            return input::Key::Eight;
        case XK_End:          return input::Key::End;
        case XK_Return:       return input::Key::Enter;
        case XK_equal:        return input::Key::Equals_Sign;
        case XK_Escape:       return input::Key::Escape;
        case XK_f:
        case XK_F:            return input::Key::F;
        case XK_F1:           return input::Key::F1;
        case XK_F2:           return input::Key::F2;
        case XK_F3:           return input::Key::F3;
        case XK_F4:           return input::Key::F4;
        case XK_F5:           return input::Key::F5;
        case XK_F6:           return input::Key::F6;
        case XK_F7:           return input::Key::F7;
        case XK_F8:           return input::Key::F8;
        case XK_F9:           return input::Key::F9;
        case XK_F10:          return input::Key::F10;
        case XK_F11:          return input::Key::F11;
        case XK_F12:          return input::Key::F12;
        case XK_5:            return input::Key::Five;
        case XK_4:            return input::Key::Four;
        case XK_g:
        case XK_G:            return input::Key::G;
        case XK_grave:        return input::Key::Grave_Accent;
        case XK_h:
        case XK_H:            return input::Key::H;
        case XK_Home:         return input::Key::Home;
        case XK_i:
        case XK_I:            return input::Key::I;
        case XK_Insert:       return input::Key::Insert;
        case XK_j:
        case XK_J:            return input::Key::J;
        case XK_k:
        case XK_K:            return input::Key::K;
        case XK_l:
        case XK_L:            return input::Key::L;
        case XK_Left:         return input::Key::Left_Arrow;
        case XK_bracketleft:  return input::Key::Left_Bracket;
        case XK_m:
        case XK_M:            return input::Key::M;
        case XK_minus:        return input::Key::Minus;
        case XK_n:
        case XK_N:            return input::Key::N;
        case XK_9:            return input::Key::Nine;
        case XK_KP_0:         return input::Key::Numpad_0;
        case XK_KP_1:         return input::Key::Numpad_1;
        case XK_KP_2:         return input::Key::Numpad_2;
        case XK_KP_3:         return input::Key::Numpad_3;
        case XK_KP_4:         return input::Key::Numpad_4;
        case XK_KP_5:         return input::Key::Numpad_5;
        case XK_KP_6:         return input::Key::Numpad_6;
        case XK_KP_7:         return input::Key::Numpad_7;
        case XK_KP_8:         return input::Key::Numpad_8;
        case XK_KP_9:         return input::Key::Numpad_9;
        case XK_KP_Decimal:   return input::Key::Numpad_Decimal;
        case XK_KP_Divide:    return input::Key::Numpad_Divide;
        case XK_KP_Enter:     return input::Key::Numpad_Enter;
        case XK_KP_Subtract:  return input::Key::Numpad_Subtract;
        case XK_KP_Multiply:  return input::Key::Numpad_Multiply;
        case XK_KP_Add:       return input::Key::Numpad_Add;
        case XK_o:
        case XK_O:            return input::Key::O;
        case XK_1:            return input::Key::One;
        case XK_p:
        case XK_P:            return input::Key::P;
        case XK_Next:         return input::Key::Page_Down;
        case XK_Prior:        return input::Key::Page_Up;
        case XK_Pause:        return input::Key::Pause;
        case XK_period:       return input::Key::Period;
        case XK_q:
        case XK_Q:            return input::Key::Q;
        case XK_r:
        case XK_R:            return input::Key::R;
        case XK_Right:        return input::Key::Right_Arrow;
        case XK_bracketright: return input::Key::Right_Bracket;
        case XK_s:
        case XK_S:            return input::Key::S;
        case XK_semicolon:    return input::Key::Semicolon;
        case XK_7:            return input::Key::Seven;
        case XK_6:            return input::Key::Six;
        case XK_slash:        return input::Key::Slash;
        case XK_space:        return input::Key::Space;
        case XK_t:
        case XK_T:            return input::Key::T;
        case XK_Tab:          return input::Key::Tab;
        case XK_3:            return input::Key::Three;
        case XK_2:            return input::Key::Two;
        case XK_u:
        case XK_U:            return input::Key::U;
        case XK_Up:           return input::Key::Up_Arrow;
        case XK_v:
        case XK_V:            return input::Key::V;
        case XK_w:
        case XK_W:            return input::Key::W;
        case XK_x:
        case XK_X:            return input::Key::X;
        case XK_y:
        case XK_Y:            return input::Key::Y;
        case XK_z:
        case XK_Z:            return input::Key::Z;
        case XK_0:            return input::Key::Zero;
        default:              return input::Key::Unknown;
    }
}

static void build_key_table(PlatformX11* platform)
{
    for(int i = 0; i < 8; ++i)
    {
        platform->key_table[i] = input::Key::Unknown;
    }
    for(int i = 8; i < 256; ++i)
    {
        KeyCode scancode = i;
        int keysyms_per_keycode_return;
        KeySym* key_syms = XGetKeyboardMapping(platform->display, scancode, 1, &keysyms_per_keycode_return);
        platform->key_table[i] = translate_key_sym(key_syms[0]);
        XFree(key_syms);
    }
}

bool main_startup()
{
    // Connect to the X server, which is used for display and input services.
    platform.display = XOpenDisplay(nullptr);
    if(!platform.display)
    {
        LOG_ERROR("X Display failed to open.");
        return false;
    }
    platform.screen = DefaultScreen(platform.display);

    // Choose the abstract "Visual" type that will be used to describe both the
    // window and the OpenGL rendering context.
    GLint visual_attributes[] = {GLX_RGBA, GLX_DEPTH_SIZE, 24, GLX_STENCIL_SIZE, 8, GLX_DOUBLEBUFFER, X11_NONE};
    platform.visual_info = glXChooseVisual(platform.display, platform.screen, visual_attributes);
    if(!platform.visual_info)
    {
        LOG_ERROR("Wasn't able to choose an appropriate Visual type given the requested attributes. [The Visual type contains information on color mappings for the display hardware]");
        return false;
    }

    // Create the platform.
    {
        int screen = platform.screen;
        Window root = RootWindow(platform.display, screen);
        Visual* visual = platform.visual_info->visual;
        platform.colormap = XCreateColormap(platform.display, root, visual, AllocNone);

        int width = window_width;
        int height = window_height;
        int depth = platform.visual_info->depth;

        XSetWindowAttributes attributes = {};
        attributes.colormap = platform.colormap;
        attributes.event_mask = StructureNotifyMask | KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask;
        unsigned long mask = CWColormap | CWEventMask;
        platform.window = XCreateWindow(platform.display, root, 0, 0, width, height, 0, depth, InputOutput, visual, mask, &attributes);
    }

    // Register to receive window close messages.
    platform.wm_delete_window = XInternAtom(platform.display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(platform.display, platform.window, &platform.wm_delete_window, 1);

    XStoreName(platform.display, platform.window, app_name);
    XSetIconName(platform.display, platform.window, app_name);

    // Create the rendering context for OpenGL. The rendering context can only
    // be "made current" after the window is mapped (with XMapWindow).
    rendering_context = glXCreateContext(platform.display, platform.visual_info, nullptr, True);
    if(!rendering_context)
    {
        LOG_ERROR("Couldn't create a GLX rendering context.");
        return false;
    }

    XMapWindow(platform.display, platform.window);

    Bool made_current = glXMakeCurrent(platform.display, platform.window, rendering_context);
    if(!made_current)
    {
        LOG_ERROR("Failed to attach the GLX context to the platform.");
        return false;
    }

    functions_loaded = ogl_LoadFunctions();
    if(!functions_loaded)
    {
        LOG_ERROR("OpenGL functions could not be loaded!");
        return false;
    }

    input::system_start_up();

    bool started = video::system_start_up();
    if(!started)
    {
        LOG_ERROR("Video system failed startup.");
        return false;
    }

    started = editor_start_up();
    if(!started)
    {
        LOG_ERROR("Editor failed startup.");
        return false;
    }
    double dpmm = get_dots_per_millimeter(&platform);
    resize_viewport(window_width, window_height, dpmm);

    build_key_table(&platform);

    return true;
}

void main_shutdown()
{
    editor_shut_down();
    video::system_shut_down(functions_loaded);

    if(platform.visual_info)
    {
        XFree(platform.visual_info);
    }
    if(platform.display)
    {
        if(rendering_context)
        {
            glXDestroyContext(platform.display, rendering_context);
        }
        if(platform.colormap != X11_NONE)
        {
            XFreeColormap(platform.display, platform.colormap);
        }
        XCloseDisplay(platform.display);
    }
}

static input::Modifier translate_modifiers(unsigned int state)
{
    input::Modifier modifier = {};
    modifier.shift = state & ShiftMask;
    modifier.control = state & ControlMask;
    modifier.alt = state & Mod1Mask;
    modifier.super = state & Mod4Mask;
    modifier.caps_lock = state & LockMask;
    modifier.num_lock = state & Mod2Mask;
    return modifier;
}

static input::Key translate_key(unsigned int scancode)
{
    if(scancode < 0 || scancode > 255)
    {
        return input::Key::Unknown;
    }
    else
    {
        return platform.key_table[scancode];
    }
}

static void handle_event(XEvent event)
{
    switch(event.type)
    {
        case ClientMessage:
        {
            XClientMessageEvent client_message = event.xclient;
            Atom message = static_cast<Atom>(client_message.data.l[0]);
            if(message == platform.wm_delete_window)
            {
                platform.close_window_requested = true;
            }
            break;
        }
        case ConfigureNotify:
        {
            XConfigureRequestEvent configure = event.xconfigurerequest;
            double dpmm = get_dots_per_millimeter(&platform);
            resize_viewport(configure.width, configure.height, dpmm);
            break;
        }
        case KeyPress:
        {
            XKeyEvent press = event.xkey;
            input::Key key = translate_key(press.keycode);
            input::Modifier modifier = translate_modifiers(press.state);
            input::key_press(key, true, modifier);
            break;
        }
        case KeyRelease:
        {
            XKeyEvent release = event.xkey;
            input::Key key = translate_key(release.keycode);
            input::Modifier modifier = translate_modifiers(release.state);
            // Examine the next event in the queue and if it's a
            // key-press generated by auto-repeating, discard it and
            // ignore this key release.
            bool auto_repeated = false;
            XEvent lookahead = {};
            if(XPending(platform.display) > 0 && XPeekEvent(platform.display, &lookahead))
            {
                XKeyEvent next_press = lookahead.xkey;
                if(
                    next_press.type == KeyPress &&
                    next_press.window == release.window &&
                    next_press.time == release.time &&
                    next_press.keycode == release.keycode)
                {
                    // Remove the lookahead event.
                    XNextEvent(platform.display, &lookahead);
                    auto_repeated = true;
                }
            }
            if(!auto_repeated)
            {
                input::key_press(key, false, modifier);
            }
            break;
        }
        case ButtonPress:
        {
            XButtonEvent button = event.xbutton;
            input::Modifier modifier = translate_modifiers(button.state);
            if(button.button == Button1)
            {
                input::mouse_click(input::MouseButton::Left, true, modifier);
            }
            else if(button.button == Button2)
            {
                input::mouse_click(input::MouseButton::Middle, true, modifier);
            }
            else if(button.button == Button3)
            {
                input::mouse_click(input::MouseButton::Right, true, modifier);
            }
            else if(button.button == Button4)
            {
                input::mouse_scroll(0, +1);
            }
            else if(button.button == Button5)
            {
                input::mouse_scroll(0, -1);
            }
            break;
        }
        case ButtonRelease:
        {
            XButtonEvent button = event.xbutton;
            input::Modifier modifier = translate_modifiers(button.state);
            if(button.button == Button1)
            {
                input::mouse_click(input::MouseButton::Left, false, modifier);
            }
            else if(button.button == Button2)
            {
                input::mouse_click(input::MouseButton::Middle, false, modifier);
            }
            else if(button.button == Button3)
            {
                input::mouse_click(input::MouseButton::Right, false, modifier);
            }
            break;
        }
        case MotionNotify:
        {
            XMotionEvent motion = event.xmotion;
            input::mouse_move(motion.x, motion.y);
            break;
        }
    }
}

static double get_clock_frequency()
{
    struct timespec resolution;
    clock_getres(CLOCK_MONOTONIC, &resolution);
    s64 nanoseconds = resolution.tv_nsec + resolution.tv_sec * 1000000000;
    return static_cast<double>(nanoseconds) / 1.0e9;
}

double get_time(double clock_frequency)
{
    struct timespec timestamp;
    clock_gettime(CLOCK_MONOTONIC, &timestamp);
    s64 nanoseconds = timestamp.tv_nsec + timestamp.tv_sec * 1000000000;
    return static_cast<double>(nanoseconds) * clock_frequency;
}

void go_to_sleep(double amount_to_sleep)
{
    struct timespec requested_time;
    requested_time.tv_sec = 0;
    requested_time.tv_nsec = static_cast<s64>(1.0e9 * amount_to_sleep);
    clock_nanosleep(CLOCK_MONOTONIC, 0, &requested_time, nullptr);
}

void main_loop()
{
    const double frame_frequency = 1.0 / 60.0;
    double clock_frequency = get_clock_frequency();

    for(;;)
    {
        double frame_start_time = get_time(clock_frequency);

        editor_update(&platform.base);
        input::system_update();

        glXSwapBuffers(platform.display, platform.window);

        // Handle window events.
        while(XPending(platform.display) > 0)
        {
            XEvent event = {};
            XNextEvent(platform.display, &event);
            handle_event(event);
            if(platform.close_window_requested)
            {
                XDestroyWindow(platform.display, platform.window);
                return;
            }
        }

        // Sleep off any remaining time until the next frame.
        double frame_thusfar = get_time(clock_frequency) - frame_start_time;
        if(frame_thusfar < frame_frequency)
        {
            go_to_sleep(frame_frequency - frame_thusfar);
        }
    }
}

int main(int argc, char** argv)
{
    static_cast<void>(argc);
    static_cast<void>(argv);

    if(!main_startup())
    {
        main_shutdown();
        return 1;
    }
    main_loop();
    main_shutdown();

    return 0;
}
