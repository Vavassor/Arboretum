#include "platform_definitions.h"

#if defined(OS_LINUX)

#include <X11/X.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xresource.h>
#include <X11/Xcursor/Xcursor.h>

#ifdef None
#undef None
#define X11_NONE 0L /* universal null resource or null atom */
#endif

#include "assert.h"
#include "editor.h"
#include "gl_core_3_3.h"
#include "glx_extensions.h"
#include "input.h"
#include "int_utilities.h"
#include "logging.h"
#include "platform.h"
#include "sized_types.h"
#include "string_utilities.h"
#include "video.h"

#include <climits>
#include <clocale>
#include <cstdlib>
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

    CursorType cursor_type;

    XFontSet font_set;
    XIM input_method;
    XIC input_context;
    bool input_method_connected;
    bool input_context_focused;

    Atom selection_clipboard;
    Atom selection_primary;
    Atom paste_code;
    Atom save_code;
    Atom utf8_string;
    Atom atom_pair;
    Atom targets;
    Atom multiple;
    Atom clipboard_manager;
    Atom save_targets;
    char* clipboard;
    int clipboard_size;

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
        default:
        case CursorType::Arrow:            return "left_ptr";
        case CursorType::Hand_Pointing:    return "hand1";
        case CursorType::I_Beam:           return "xterm";
        case CursorType::Prohibition_Sign: return "crossed_circle";
    }
}

void change_cursor(Platform* base, CursorType type)
{
    PlatformX11* platform = reinterpret_cast<PlatformX11*>(base);
    if(platform->cursor_type != type)
    {
        const char* name = translate_cursor_type(type);
        Cursor cursor = XcursorLibraryLoadCursor(platform->display, name);
        XDefineCursor(platform->display, platform->window, cursor);
        platform->cursor_type = type;
    }
}

void begin_composed_text(Platform* base)
{
    PlatformX11* platform = reinterpret_cast<PlatformX11*>(base);
    XSetICFocus(platform->input_context);
    platform->input_context_focused = true;
}

void end_composed_text(Platform* base)
{
    PlatformX11* platform = reinterpret_cast<PlatformX11*>(base);
    if(platform->input_context)
    {
        XUnsetICFocus(platform->input_context);
    }
    platform->input_context_focused = false;
}

void set_composed_text_position(Platform* base, int x, int y)
{
    PlatformX11* platform = reinterpret_cast<PlatformX11*>(base);
    ASSERT(platform->input_context_focused);

    XPoint location;
    location.x = x;
    location.y = y;
    XVaNestedList list = XVaCreateNestedList(0, XNSpotLocation, &location, nullptr);
    XSetICValues(platform->input_context, XNPreeditAttributes, list, nullptr);
    XFree(list);
}

bool copy_to_clipboard(Platform* base, char* clipboard)
{
    PlatformX11* platform = reinterpret_cast<PlatformX11*>(base);

    XSetSelectionOwner(platform->display, platform->selection_clipboard, platform->window, CurrentTime);

    bool is_owner = XGetSelectionOwner(platform->display, platform->selection_clipboard) == platform->window;
    if(is_owner)
    {
        platform->clipboard = clipboard;
        platform->clipboard_size = string_size(clipboard);

        Window owner = XGetSelectionOwner(platform->display, platform->clipboard_manager);
        if(owner == X11_NONE)
        {
            LOG_ERROR("There's no clipboard manager.");
            return true;
        }

        const int count = 1;
        Atom target_types[count] = {platform->utf8_string};
        unsigned char* property = reinterpret_cast<unsigned char*>(target_types);
        XChangeProperty(platform->display, platform->window, platform->save_code, XA_ATOM, 32, PropModeReplace, property, count);

        XConvertSelection(platform->display, platform->clipboard_manager, platform->save_targets, platform->save_code, platform->window, CurrentTime);
    }

    return is_owner;
}

void request_paste_from_clipboard(Platform* base)
{
    PlatformX11* platform = reinterpret_cast<PlatformX11*>(base);

    XConvertSelection(platform->display, platform->selection_clipboard, platform->utf8_string, platform->paste_code, platform->window, CurrentTime);
}

namespace
{
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

static XIMStyle choose_better_style(XIMStyle style1, XIMStyle style2)
{
    XIMStyle preedit = XIMPreeditArea | XIMPreeditCallbacks | XIMPreeditPosition | XIMPreeditNothing | XIMPreeditNone;
    XIMStyle status = XIMStatusArea | XIMStatusCallbacks | XIMStatusNothing | XIMStatusNone;
    if(style1 == 0)
    {
        return style2;
    }
    if(style2 == 0)
    {
        return style1;
    }
    if((style1 & (preedit | status)) == (style2 & (preedit | status)))
    {
        return style1;
    }
    XIMStyle s = style1 & preedit;
    XIMStyle t = style2 & preedit;
    if(s != t)
    {
        if(s | t | XIMPreeditCallbacks)
        {
            return (s == XIMPreeditCallbacks) ? style1 : style2;
        }
        else if(s | t | XIMPreeditPosition)
        {
            return (s == XIMPreeditPosition) ? style1 : style2;
        }
        else if(s | t | XIMPreeditArea)
        {
            return (s == XIMPreeditArea) ? style1 : style2;
        }
        else if(s | t | XIMPreeditNothing)
        {
            return (s == XIMPreeditNothing) ? style1 : style2;
        }
    }
    else
    {
        // if preedit flags are the same, compare status flags
        s = style1 & status;
        t = style2 & status;
        if(s | t | XIMStatusCallbacks)
        {
            return (s == XIMStatusCallbacks) ? style1 : style2;
        }
        else if(s | t | XIMStatusArea)
        {
           return (s == XIMStatusArea) ? style1 : style2;
        }
        else if(s | t | XIMStatusNothing)
        {
            return (s == XIMStatusNothing) ? style1 : style2;
        }
    }
    return style2;
}

static void destroy_input_method(XIM input_method, XPointer client_data, XPointer call_data)
{
    static_cast<void>(call_data); // always null

    PlatformX11* platform = reinterpret_cast<PlatformX11*>(client_data);
    platform->input_context = nullptr;
    platform->input_method = nullptr;
    platform->input_method_connected = false;

    LOG_ERROR("Input method closed unexpectedly.");
}

static void instantiate_input_method(Display* display, XPointer client_data, XPointer call_data)
{
    static_cast<void>(call_data); // always null

    PlatformX11* platform = reinterpret_cast<PlatformX11*>(client_data);

    // Close the previous input method and context, if there was one.
    if(platform->input_method_connected)
    {
        if(platform->input_context)
        {
            XDestroyIC(platform->input_context);
            platform->input_context = nullptr;
        }
        if(platform->input_method)
        {
            XCloseIM(platform->input_method);
            platform->input_method = nullptr;
        }
        platform->input_method_connected = false;
    }

    // input method
    platform->input_method = XOpenIM(display, nullptr, nullptr, nullptr);
    if(!platform->input_method)
    {
        LOG_ERROR("X Input Method failed to open.");
        return;
    }

    // font set
    int num_missing_charsets;
    char** missing_charsets;
    char* default_string;
    const char* font_names = "-adobe-helvetica-*-r-*-*-*-120-*-*-*-*-*-*,-misc-fixed-*-r-*-*-*-130-*-*-*-*-*-*";
    platform->font_set = XCreateFontSet(display, font_names, &missing_charsets, &num_missing_charsets, &default_string);
    if(!platform->font_set)
    {
        LOG_ERROR("Failed to make a font set.");
        return;
    }

    // Negotiate input method styles.
    XIMStyles* input_method_styles;
    XGetIMValues(platform->input_method, XNQueryInputStyle, &input_method_styles, NULL);
    XIMStyle supported_styles = XIMPreeditPosition | XIMPreeditNothing | XIMStatusNothing;
    XIMStyle best_style = 0;
    for(int i = 0; i < input_method_styles->count_styles; i += 1)
    {
        XIMStyle style = input_method_styles->supported_styles[i];
        if((style & supported_styles) == style)
        {
            best_style = choose_better_style(style, best_style);
        }
    }
    XFree(input_method_styles);
    if(!best_style)
    {
        LOG_ERROR("None of the input styles were supported.");
        return;
    }

    // input context
    XPoint spot_location = {0, 0};
    XVaNestedList list = XVaCreateNestedList(0, XNFontSet, platform->font_set, XNSpotLocation, &spot_location, nullptr);
    XIMCallback destroy_callback;
    destroy_callback.client_data = reinterpret_cast<XPointer>(platform);
    destroy_callback.callback = destroy_input_method;
    platform->input_context = XCreateIC(platform->input_method, XNInputStyle, best_style, XNClientWindow, platform->window, XNPreeditAttributes, list, XNStatusAttributes, list, XNDestroyCallback, &destroy_callback, nullptr);
    XFree(list);
    if(!platform->input_context)
    {
        LOG_ERROR("X Input Context failed to open.");
        return;
    }

    // Add input events to the event mask for the current window.
    XWindowAttributes attributes = {};
    XGetWindowAttributes(display, platform->window, &attributes);
    long event_mask = attributes.your_event_mask;
    long input_method_event_mask;
    XGetICValues(platform->input_context, XNFilterEvents, &input_method_event_mask, nullptr);
    XSelectInput(display, platform->window, event_mask | input_method_event_mask);

    // By default, it seems to be in focus and will pop up the input method
    // editor when the app first opens, so purposefully "unset" it.
    XUnsetICFocus(platform->input_context);

    platform->input_method_connected = true;
}

// The locale parameter expects a X/Open locale identifier. These are strings
// with the form: language[_territory][.codeset][@modifier] where brackets []
// indicate a part of the string that is optional.
//
// 1. The language part is an ISO 639 language code, which can be two or three
// letters.
// 2. The territory is an ISO 3166 code, which is two or three letters, or
// a three digit number.
// 3. The codeset is a character encoding identifier, but has an unspecified and
// non-standardized format. It's hopefully either blank or the string "UTF-8".
// 4. The modifier can indicate script, dialect, and collation order changes,
// and is language-specific and non-standardized. Ignored, here.
LocaleId match_closest_locale_id(const char* locale)
{
    char language[4] = {};
    char region[4] = {};
    char encoding[16] = {};
    char modifier[16] = {};

    int end = string_size(locale);

    int at = find_char(locale, '@');
    if(at != invalid_index)
    {
        copy_string(modifier, 16, &locale[at + 1]);
        end = at;
    }

    int period = find_char(locale, '.');
    if(period != invalid_index)
    {
        int size = MIN(16, end - period);
        copy_string(encoding, size, &locale[period + 1]);
        end = period;
    }

    int underscore = find_char(locale, '_');
    if(underscore != invalid_index)
    {
        int size = MIN(4, end - underscore);
        copy_string(region, size, &locale[underscore + 1]);
        end = underscore;
    }

    int size = MIN(4, end + 1);
    copy_string(language, size, locale);

    // LOG_DEBUG("%s, %s, %s, %s", language, region, encoding, modifier);

    return LocaleId::Default;
}

bool main_start_up()
{
    // Set the locale.
    char* locale = setlocale(LC_ALL, "");
    if(!locale)
    {
        LOG_ERROR("Failed to set the locale.");
        return false;
    }

    // Connect to the X server, which is used for display and input services.
    platform.display = XOpenDisplay(nullptr);
    if(!platform.display)
    {
        LOG_ERROR("X Display failed to open.");
        return false;
    }
    platform.screen = DefaultScreen(platform.display);

    // Check if the X server is okay with the locale.
    Bool locale_supported = XSupportsLocale();
    if(!locale_supported)
    {
        LOG_ERROR("X does not support locale %s.", locale);
        return false;
    }
    char* locale_modifiers = XSetLocaleModifiers("");
    if(!locale_modifiers)
    {
        LOG_ERROR("Failed to set locale modifiers.");
        return false;
    }

    // Retrieve the locale that will be used to localize general text.
    const char* text_locale = getenv("LC_MESSAGES");
    if(!text_locale)
    {
        text_locale = getenv("LC_ALL");
        if(!text_locale)
        {
            text_locale = getenv("LANG");
            if(!text_locale)
            {
                LOG_ERROR("Failed to determine the text locale.");
                return false;
            }
        }
    }
    platform.base.locale_id = match_closest_locale_id(text_locale);

    create_stack(&platform.base);
    bool loaded = load_localized_text(&platform.base);
    if(!loaded)
    {
        LOG_ERROR("Failed to load the localized text.");
        return false;
    }

    // Choose the abstract "Visual" type that will be used to describe both the
    // window and the OpenGL rendering context.
    GLint visual_attributes[] = {GLX_RGBA, GLX_DEPTH_SIZE, 24, GLX_STENCIL_SIZE, 8, GLX_DOUBLEBUFFER, X11_NONE};
    platform.visual_info = glXChooseVisual(platform.display, platform.screen, visual_attributes);
    if(!platform.visual_info)
    {
        LOG_ERROR("Wasn't able to choose an appropriate Visual type given the requested attributes. [The Visual type contains information on color mappings for the display hardware]");
        return false;
    }

    // Create the window.
    long event_mask = StructureNotifyMask | KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask | FocusChangeMask | PropertyChangeMask;
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
        attributes.event_mask = event_mask;
        unsigned long mask = CWColormap | CWEventMask;
        platform.window = XCreateWindow(platform.display, root, 0, 0, width, height, 0, depth, InputOutput, visual, mask, &attributes);
    }

    // Register to receive window close messages.
    platform.wm_delete_window = XInternAtom(platform.display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(platform.display, platform.window, &platform.wm_delete_window, 1);

    XStoreName(platform.display, platform.window, platform.base.nonlocalized_text.app_name);
    XSetIconName(platform.display, platform.window, platform.base.nonlocalized_text.app_name);

    // Register for the input method context to be created.
    XPointer client_data = reinterpret_cast<XPointer>(&platform);
    XRegisterIMInstantiateCallback(platform.display, nullptr, nullptr, nullptr, instantiate_input_method, client_data);

    // Set up the clipboard.
    {
        platform.selection_primary = XInternAtom(platform.display, "PRIMARY", False);
        platform.selection_clipboard = XInternAtom(platform.display, "CLIPBOARD", False);
        platform.utf8_string = XInternAtom(platform.display, "UTF8_STRING", False);
        platform.atom_pair = XInternAtom(platform.display, "ATOM_PAIR", False);
        platform.targets = XInternAtom(platform.display, "TARGETS", False);
        platform.multiple = XInternAtom(platform.display, "MULTIPLE", False);

        platform.clipboard_manager = XInternAtom(platform.display, "CLIPBOARD_MANAGER", False);
        platform.save_targets = XInternAtom(platform.display, "SAVE_TARGETS", False);

        // There are arbitrarily-named atom chosen which will be used to
        // identify properties in selection requests we make.
        const char* app_name = platform.base.nonlocalized_text.app_name;

        char code[24];
        format_string(code, sizeof(code), "%s_PASTE", app_name);
        to_upper_case_ascii(code);
        platform.paste_code = XInternAtom(platform.display, code, False);

        format_string(code, sizeof(code), "%s_SAVE_TARGETS", app_name);
        to_upper_case_ascii(code);
        platform.save_code = XInternAtom(platform.display, code, False);
    }

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

    started = editor_start_up(&platform.base);
    if(!started)
    {
        LOG_ERROR("Editor failed startup.");
        return false;
    }
    double dpmm = get_dots_per_millimeter(&platform);
    Int2 dimensions = {window_width, window_height};
    resize_viewport(dimensions, dpmm);

    build_key_table(&platform);

    return true;
}

void main_shut_down()
{
    editor_shut_down();
    video::system_shut_down(functions_loaded);
    destroy_stack(&platform.base);

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
        if(platform.input_context)
        {
            XDestroyIC(platform.input_context);
        }
        if(platform.input_method)
        {
            XCloseIM(platform.input_method);
        }
        if(platform.font_set)
        {
            XFreeFontSet(platform.display, platform.font_set);
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

static void handle_selection_clear()
{
    // Another application overwrote our clipboard contents, so they can
    // be deallocated now.
    editor_destroy_clipboard_copy(platform.clipboard);
    platform.clipboard = nullptr;
    platform.clipboard_size = 0;
}

static void handle_selection_notify(XEvent* event)
{
    XSelectionEvent notification = event->xselection;
    if(notification.target != platform.save_targets && notification.property != X11_NONE)
    {
        Atom type;
        int format;
        unsigned long count;
        unsigned long size;
        unsigned char* property = nullptr;
        XGetWindowProperty(platform.display, platform.window, notification.property, 0, 0, False, AnyPropertyType, &type, &format, &count, &size, &property);
        XFree(property);

        Atom incr = XInternAtom(platform.display, "INCR", False);
        if(type == incr)
        {
            LOG_ERROR("Clipboard does not support incremental transfers (INCR).");
        }
        else
        {
            XGetWindowProperty(platform.display, platform.window, notification.property, 0, size, False, AnyPropertyType, &type, &format, &count, &size, &property);
            char* paste = reinterpret_cast<char*>(property);
            editor_paste_from_clipboard(&platform.base, paste);

            XFree(property);
        }

        XDeleteProperty(notification.display, notification.requestor, notification.property);
    }
}

static void handle_selection_request(XEvent* event)
{
    XSelectionRequestEvent request = event->xselectionrequest;
    if(request.target == platform.utf8_string && request.property != X11_NONE)
    {
        unsigned char* contents = reinterpret_cast<unsigned char*>(platform.clipboard);
        XChangeProperty(request.display, request.requestor, request.property, platform.utf8_string, 8, PropModeReplace, contents, platform.clipboard_size);

        XEvent response = {};
        response.xselection.type = SelectionNotify;
        response.xselection.requestor = request.requestor;
        response.xselection.selection = request.selection;
        response.xselection.target = request.target;
        response.xselection.property = request.property;
        response.xselection.time = request.time;
        XSendEvent(request.display, request.requestor, False, NoEventMask, &response);
    }
    else if(request.target == platform.multiple && request.property != X11_NONE)
    {
        Atom type;
        int format;
        unsigned long count;
        unsigned long bytes_after;
        unsigned char* property;
        XGetWindowProperty(request.display, request.requestor, request.property, 0, LONG_MAX, False, platform.atom_pair, &type, &format, &count, &bytes_after, &property);

        Atom* targets = reinterpret_cast<Atom*>(property);
        for(unsigned long i = 0; i < count; i += 2)
        {
            char* name = XGetAtomName(platform.display, targets[i]);
            XFree(name);
            if(targets[i] == platform.utf8_string)
            {
                unsigned char* contents = reinterpret_cast<unsigned char*>(platform.clipboard);
                XChangeProperty(request.display, request.requestor, targets[i + 1], targets[i], 8, PropModeReplace, contents, platform.clipboard_size);
            }
            else
            {
                targets[i + 1] = X11_NONE;
            }
        }

        XChangeProperty(request.display, request.requestor, request.property, platform.atom_pair, 32, PropModeReplace, property, count);
        XFree(property);

        XEvent response = {};
        response.xselection.type = SelectionNotify;
        response.xselection.requestor = request.requestor;
        response.xselection.selection = request.selection;
        response.xselection.target = request.target;
        response.xselection.property = request.property;
        response.xselection.time = request.time;
        XSendEvent(request.display, request.requestor, False, NoEventMask, &response);
    }
    else if(request.target == platform.targets)
    {
        const int count = 4;
        Atom target_types[count] =
        {
            platform.targets,
            platform.multiple,
            platform.save_targets,
            platform.utf8_string,
        };
        unsigned char* targets = reinterpret_cast<unsigned char*>(target_types);
        XChangeProperty(request.display, request.requestor, request.property, XA_ATOM, 32, PropModeReplace, targets, count);

        XEvent response = {};
        response.xselection.type = SelectionNotify;
        response.xselection.requestor = request.requestor;
        response.xselection.selection = request.selection;
        response.xselection.target = request.target;
        response.xselection.property = request.property;
        response.xselection.time = request.time;
        XSendEvent(request.display, request.requestor, False, NoEventMask, &response);
    }
    else
    {
        XEvent denial;
        denial.xselection.type = SelectionNotify;
        denial.xselection.requestor = request.requestor;
        denial.xselection.selection = request.selection;
        denial.xselection.target = request.target;
        denial.xselection.property = X11_NONE;
        denial.xselection.time = request.time;
        XSendEvent(request.display, request.requestor, False, NoEventMask, &denial);
    }
}

static void handle_event(XEvent event)
{
    switch(event.type)
    {
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
                Int2 velocity = {0, +1};
                input::mouse_scroll(velocity);
            }
            else if(button.button == Button5)
            {
                Int2 velocity = {0, -1};
                input::mouse_scroll(velocity);
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
            Int2 dimensions = {configure.width, configure.height};
            resize_viewport(dimensions, dpmm);
            break;
        }
        case FocusIn:
        {
            XFocusInEvent focus_in = event.xfocus;
            if(focus_in.window == platform.window && platform.input_context_focused)
            {
                XSetICFocus(platform.input_context);
            }
            break;
        }
        case FocusOut:
        {
            XFocusInEvent focus_out = event.xfocus;
            if(focus_out.window == platform.window && platform.input_context_focused)
            {
                XUnsetICFocus(platform.input_context);
            }
            break;
        }
        case KeyPress:
        {
            XKeyEvent press = event.xkey;

            // Process key presses that are used for controls and hotkeys.
            input::Key key = translate_key(press.keycode);
            input::Modifier modifier = translate_modifiers(press.state);
            input::key_press(key, true, modifier);

            // Process key presses that are for typing text.
            if(platform.input_context)
            {
                Status status;
                KeySym key_sym;
                const int buffer_size = 16;
                char buffer[buffer_size];
                int length = Xutf8LookupString(platform.input_context, &press, buffer, buffer_size, &key_sym, &status);
                ASSERT(status != XBufferOverflow || length >= buffer_size);
                switch(status)
                {
                    case XLookupNone:
                    case XLookupKeySym:
                    {
                        break;
                    }
                    [[fallthrough]]
                    case XLookupBoth:
                    {
                        if(key_sym == XK_BackSpace || key_sym == XK_Delete || key_sym == XK_Return)
                        {
                            break;
                        }
                    }
                    case XLookupChars:
                    {
                        if(!only_control_characters(buffer))
                        {
                            input::composed_text_entered(buffer);
                        }
                        break;
                    }
                }
            }
            else
            {
                // @Incomplete: throw away keystrokes?
            }
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
        case MotionNotify:
        {
            XMotionEvent motion = event.xmotion;
            Int2 position = {motion.x, motion.y};
            input::mouse_move(position);
            break;
        }
        case SelectionClear:
        {
            handle_selection_clear();
            break;
        }
        case SelectionNotify:
        {
            handle_selection_notify(&event);
            break;
        }
        case SelectionRequest:
        {
            handle_selection_request(&event);
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
            if(XFilterEvent(&event, X11_NONE))
            {
                continue;
            }
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

    if(!main_start_up())
    {
        main_shut_down();
        return 1;
    }
    main_loop();
    main_shut_down();

    return 0;
}

#elif defined(OS_WINDOWS)

#include "assert.h"
#include "editor.h"
#include "gl_core_3_3.h"
#include "input.h"
#include "int2.h"
#include "logging.h"
#include "platform.h"
#include "string_build.h"
#include "string_utilities.h"
#include "video.h"
#include "wgl_extensions.h"
#include "wide_char.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <windowsx.h>

struct PlatformWindows
{
    Platform base;

    HWND window;
    HDC device_context;
    Int2 viewport;

    CursorType cursor_type;
    HCURSOR cursor_arrow;
    HCURSOR cursor_hand_pointing;
    HCURSOR cursor_i_beam;
    HCURSOR cursor_prohibition_sign;

    bool input_context_focused;
    Int2 composed_text_position;
    bool composing;
};

static void load_cursors(PlatformWindows* platform)
{
    UINT flags = LR_DEFAULTSIZE | LR_SHARED;
    platform->cursor_arrow = static_cast<HCURSOR>(LoadImage(nullptr, IDC_ARROW, IMAGE_CURSOR, 0, 0, flags));
    platform->cursor_hand_pointing = static_cast<HCURSOR>(LoadImage(nullptr, IDC_HAND, IMAGE_CURSOR, 0, 0, flags));
    platform->cursor_i_beam = static_cast<HCURSOR>(LoadImage(nullptr, IDC_IBEAM, IMAGE_CURSOR, 0, 0, flags));
    platform->cursor_prohibition_sign = static_cast<HCURSOR>(LoadImage(nullptr, IDC_NO, IMAGE_CURSOR, 0, 0, flags));
}

static HCURSOR get_cursor_by_type(PlatformWindows* platform, CursorType type)
{
    switch(type)
    {
        default:
        case CursorType::Arrow:            return platform->cursor_arrow;
        case CursorType::Hand_Pointing:    return platform->cursor_hand_pointing;
        case CursorType::I_Beam:           return platform->cursor_i_beam;
        case CursorType::Prohibition_Sign: return platform->cursor_prohibition_sign;
    }
}

void change_cursor(Platform* base, CursorType type)
{
    PlatformWindows* platform = reinterpret_cast<PlatformWindows*>(base);
    if(platform->cursor_type != type)
    {
        HCURSOR cursor = get_cursor_by_type(platform, type);
        SetCursor(cursor);
        platform->cursor_type = type;
    }
}

static void reset_composing(PlatformWindows* platform, HIMC context)
{
    if(platform->composing)
    {
        ImmNotifyIME(context, NI_COMPOSITIONSTR, CPS_COMPLETE, 0);
        platform->composing = false;
    }
}

static void move_input_method(HIMC context, Int2 position)
{
    CANDIDATEFORM candidate_position;
    candidate_position.dwIndex = 0;
    candidate_position.dwStyle = CFS_CANDIDATEPOS;
    candidate_position.ptCurrentPos = {position.x, position.y};
    candidate_position.rcArea = {0, 0, 0, 0};
    ImmSetCandidateWindow(context, &candidate_position);
}

void set_composed_text_position(Platform* base, int x, int y)
{
    PlatformWindows* platform = reinterpret_cast<PlatformWindows*>(base);

    Int2 position = {x, y};
    if(position != platform->composed_text_position)
    {
        HIMC context = ImmGetContext(platform->window);
        if(context)
        {
            move_input_method(context, position);
            ImmReleaseContext(platform->window, context);
        }

        platform->composed_text_position = position;
    }
}

void begin_composed_text(Platform* base)
{
    PlatformWindows* platform = reinterpret_cast<PlatformWindows*>(base);

    ImmAssociateContextEx(platform->window, nullptr, IACE_DEFAULT);

    platform->input_context_focused = true;
}

void end_composed_text(Platform* base)
{
    PlatformWindows* platform = reinterpret_cast<PlatformWindows*>(base);

    HIMC context = ImmGetContext(platform->window);
    if(context)
    {
        reset_composing(platform, context);
        ImmReleaseContext(platform->window, context);
    }
    
    ImmAssociateContextEx(platform->window, nullptr, 0);

    platform->input_context_focused = false;
}

bool copy_to_clipboard(Platform* base, char* clipboard)
{
    PlatformWindows* platform = reinterpret_cast<PlatformWindows*>(base);

    // Convert the contents to UTF-16.
    wchar_t* wide = utf8_to_wide_char(clipboard, &base->stack);
    if(!wide)
    {
        return false;
    }

    // Make a copy of the wide string that can be moved within the default windows
    // heap (GMEM_MOVEABLE).
    int count = string_size(clipboard);
    HGLOBAL handle = GlobalAlloc(GMEM_MOVEABLE, (count + 1) * sizeof(wchar_t));
    if(!handle)
    {
        STACK_DEALLOCATE(&base->stack, wide);
        return false;
    }

    LPWSTR wide_clone = static_cast<LPWSTR>(GlobalLock(handle));
    copy_memory(wide_clone, wide, count * sizeof(wchar_t));
    wide_clone[count] = L'\0';
    GlobalUnlock(handle);

    STACK_DEALLOCATE(&base->stack, wide);

    // Actually copy to the clipboard.
    BOOL opened = OpenClipboard(platform->window);
    if(!opened)
    {
        GlobalFree(handle);
        return false;
    }

    EmptyClipboard();
    SetClipboardData(CF_UNICODETEXT, handle);
    CloseClipboard();

    GlobalFree(handle);

    return true;
}

void request_paste_from_clipboard(Platform* base)
{
    PlatformWindows* platform = reinterpret_cast<PlatformWindows*>(base);

    BOOL has_utf16 = IsClipboardFormatAvailable(CF_UNICODETEXT);
    if(!has_utf16)
    {
        LOG_ERROR("Paste format UTF-16 was not available.");
        return;
    }

    // Get the actual paste and convert to UTF-8 text.
    char* paste = nullptr;
    BOOL opened = OpenClipboard(platform->window);
    if(opened)
    {
        HGLOBAL data = GetClipboardData(CF_UNICODETEXT);
        if(data)
        {
            LPWSTR wide = static_cast<LPWSTR>(GlobalLock(data));
            if(wide)
            {
                paste = wide_char_to_utf8(wide, &base->stack);
                GlobalUnlock(data);
            }
        }

        CloseClipboard();
    }
    
    if(paste)
    {
        // Standardize on Unix line endings by replacing "Windows style"
        // carriage return + line feed pairs before handing it to the editor.

        char* corrected = replace_substrings(paste, "\r\n", "\n", &base->stack);
        editor_paste_from_clipboard(base, corrected);
        STACK_DEALLOCATE(&base->stack, corrected);
        STACK_DEALLOCATE(&base->stack, paste);
    }
    else
    {
        LOG_ERROR("Paste failed.");
    }
}

static Int2 get_window_dimensions(PlatformWindows* platform)
{
    RECT rect;
    BOOL got = GetClientRect(platform->window, &rect);
    ASSERT(got);
    return {rect.right, rect.bottom};
}

static double get_dots_per_millimeter(PlatformWindows* platform)
{
    const double millimeters_per_inch = 25.4;
    UINT dpi = 96; // GetDpiForWindow(platform->window);
    return dpi / millimeters_per_inch;
}

static input::Key translate_virtual_key(WPARAM w_param)
{
    switch(w_param)
    {
        case '0':           return input::Key::Zero;
        case '1':           return input::Key::One;
        case '2':           return input::Key::Two;
        case '3':           return input::Key::Three;
        case '4':           return input::Key::Four;
        case '5':           return input::Key::Five;
        case '6':           return input::Key::Six;
        case '7':           return input::Key::Seven;
        case '8':           return input::Key::Eight;
        case '9':           return input::Key::Nine;
        case 'A':           return input::Key::A;
        case VK_ADD:        return input::Key::Numpad_Add;
        case 'B':           return input::Key::B;
        case VK_BACK:       return input::Key::Backspace;
        case 'C':           return input::Key::C;
        case 'D':           return input::Key::D;
        case VK_DECIMAL:    return input::Key::Numpad_Decimal;
        case VK_DELETE:     return input::Key::Delete;
        case VK_DIVIDE:     return input::Key::Numpad_Divide;
        case VK_DOWN:       return input::Key::Down_Arrow;
        case 'E':           return input::Key::E;
        case VK_END:        return input::Key::End;
        case VK_ESCAPE:     return input::Key::Escape;
        case 'F':           return input::Key::F;
        case VK_F1:         return input::Key::F1;
        case VK_F2:         return input::Key::F2;
        case VK_F3:         return input::Key::F3;
        case VK_F4:         return input::Key::F4;
        case VK_F5:         return input::Key::F5;
        case VK_F6:         return input::Key::F6;
        case VK_F7:         return input::Key::F7;
        case VK_F8:         return input::Key::F8;
        case VK_F9:         return input::Key::F9;
        case VK_F10:        return input::Key::F10;
        case VK_F11:        return input::Key::F11;
        case VK_F12:        return input::Key::F12;
        case 'G':           return input::Key::G;
        case 'H':           return input::Key::H;
        case VK_HOME:       return input::Key::Home;
        case 'I':           return input::Key::I;
        case VK_INSERT:     return input::Key::Insert;
        case 'J':           return input::Key::J;
        case 'K':           return input::Key::K;
        case 'L':           return input::Key::L;
        case VK_LEFT:       return input::Key::Left_Arrow;
        case 'M':           return input::Key::M;
        case VK_MULTIPLY:   return input::Key::Numpad_Multiply;
        case 'N':           return input::Key::N;
        case VK_NUMPAD0:    return input::Key::Numpad_0;
        case VK_NUMPAD1:    return input::Key::Numpad_1;
        case VK_NUMPAD2:    return input::Key::Numpad_2;
        case VK_NUMPAD3:    return input::Key::Numpad_3;
        case VK_NUMPAD4:    return input::Key::Numpad_4;
        case VK_NUMPAD5:    return input::Key::Numpad_5;
        case VK_NUMPAD6:    return input::Key::Numpad_6;
        case VK_NUMPAD7:    return input::Key::Numpad_7;
        case VK_NUMPAD8:    return input::Key::Numpad_8;
        case VK_NUMPAD9:    return input::Key::Numpad_9;
        case VK_NEXT:       return input::Key::Page_Down;
        case 'O':           return input::Key::O;
        case VK_OEM_1:      return input::Key::Semicolon;
        case VK_OEM_2:      return input::Key::Slash;
        case VK_OEM_3:      return input::Key::Grave_Accent;
        case VK_OEM_4:      return input::Key::Left_Bracket;
        case VK_OEM_5:      return input::Key::Backslash;
        case VK_OEM_6:      return input::Key::Right_Bracket;
        case VK_OEM_7:      return input::Key::Apostrophe;
        case VK_OEM_COMMA:  return input::Key::Comma;
        case VK_OEM_MINUS:  return input::Key::Minus;
        case VK_OEM_PERIOD: return input::Key::Period;
        case VK_OEM_PLUS:   return input::Key::Equals_Sign;
        case 'P':           return input::Key::P;
        case VK_PAUSE:      return input::Key::Pause;
        case VK_PRIOR:      return input::Key::Page_Up;
        case 'Q':           return input::Key::Q;
        case 'R':           return input::Key::R;
        case VK_RETURN:     return input::Key::Enter;
        case VK_RIGHT:      return input::Key::Right_Arrow;
        case 'S':           return input::Key::S;
        case VK_SPACE:      return input::Key::Space;
        case VK_SUBTRACT:   return input::Key::Numpad_Subtract;
        case 'T':           return input::Key::T;
        case VK_TAB:        return input::Key::Tab;
        case 'U':           return input::Key::U;
        case VK_UP:         return input::Key::Up_Arrow;
        case 'V':           return input::Key::V;
        case 'W':           return input::Key::W;
        case 'X':           return input::Key::X;
        case 'Y':           return input::Key::Y;
        case 'Z':           return input::Key::Z;
        default:            return input::Key::Unknown;
    }
}

static input::Modifier translate_modifiers(WPARAM w_param)
{
    input::Modifier modifier = {};
    modifier.control = w_param & MK_CONTROL;
    modifier.shift = w_param & MK_SHIFT;
    return modifier;
}

static input::Modifier fetch_modifiers()
{
    input::Modifier modifier = {};
    modifier.alt = GetKeyState(VK_MENU) & 0x8000;
    modifier.control = GetKeyState(VK_CONTROL) & 0x8000;
    modifier.shift = GetKeyState(VK_SHIFT) & 0x8000;
    return modifier;
}

namespace
{
    const int window_width = 800;
    const int window_height = 600;

    PlatformWindows platform;
    HGLRC rendering_context;
    bool functions_loaded;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param)
{
    switch(message)
    {
        case WM_CLOSE:
        {
            PostQuitMessage(0);
            return 0;
        }
        case WM_DESTROY:
        {
            HGLRC rc = wglGetCurrentContext();
            if(rc)
            {
                HDC dc = wglGetCurrentDC();
                wglMakeCurrent(nullptr, nullptr);
                ReleaseDC(hwnd, dc);
                wglDeleteContext(rc);
            }
            DestroyWindow(hwnd);
            if(hwnd == platform.window)
            {
                platform.window = nullptr;
            }
            return 0;
        }
        case WM_DPICHANGED:
        {
            WORD dpi = HIWORD(w_param);
            double dots_per_millimeter = dpi / 25.4;
            resize_viewport(platform.viewport, dots_per_millimeter);

            RECT* suggested_rect = reinterpret_cast<RECT*>(l_param);
            int left = suggested_rect->left;
            int top = suggested_rect->top;
            int width = suggested_rect->right - suggested_rect->left;
            int height = suggested_rect->bottom - suggested_rect->top;
            UINT flags = SWP_NOZORDER | SWP_NOACTIVATE;
            SetWindowPos(hwnd, nullptr, left, top, width, height, flags);

            return 0;
        }
        case WM_CHAR:
        {
            if(platform.input_context_focused)
            {
                wchar_t wide[2] = {w_param, L'\0'};
                char* text = wide_char_to_utf8(wide, &platform.base.stack);
                if(!only_control_characters(text))
                {
                    input::composed_text_entered(text);
                }
                STACK_DEALLOCATE(&platform.base.stack, text);
                
                return 0;
            }
            break;
        }
        case WM_IME_COMPOSITION:
        {
            if(l_param & GCS_RESULTSTR)
            {
                HIMC context = ImmGetContext(hwnd);
                if(!context)
                {
                    LOG_ERROR("Failed to get the input method context.");
                    break;
                }

                move_input_method(context, platform.composed_text_position);

                int bytes = ImmGetCompositionStringW(context, GCS_RESULTSTR, nullptr, 0);
                bytes += sizeof(wchar_t);
                wchar_t* string = static_cast<wchar_t*>(stack_allocate(&platform.base.stack, bytes));

                ImmGetCompositionStringW(context, GCS_RESULTSTR, string, bytes);
                ImmReleaseContext(hwnd, context);

                char* text = wide_char_to_utf8(string, &platform.base.stack);
                input::composed_text_entered(text);

                STACK_DEALLOCATE(&platform.base.stack, text);
                STACK_DEALLOCATE(&platform.base.stack, string);

                platform.composing = false;

                return 0;
            }
            else if(l_param & GCS_COMPSTR)
            {
                platform.composing = true;

                return 0;
            }
        }
        case WM_IME_SETCONTEXT:
        {
            HIMC context = ImmGetContext(hwnd);
            if(context)
            {
                move_input_method(context, platform.composed_text_position);
                reset_composing(&platform, context);

                ImmReleaseContext(hwnd, context);
            }

            l_param &= ~ISC_SHOWUICOMPOSITIONWINDOW;

            break;
        }
        case WM_IME_STARTCOMPOSITION:
        {
            HIMC context = ImmGetContext(hwnd);
            if(context)
            {
                move_input_method(context, platform.composed_text_position);

                ImmReleaseContext(hwnd, context);
            }

            platform.composing = false;

            return 0;
        }
        case WM_KEYDOWN:
        {
            // Bit 30 of the LPARAM is set when the key was previously down, so
            // it can be used to determine whether a press is auto-repeated.

            bool auto_repeated = l_param & 0x40000000;
            if(!auto_repeated)
            {
                input::Modifier modifier = fetch_modifiers();
                input::Key key = translate_virtual_key(w_param);
                input::key_press(key, true, modifier);

                return 0;
            }
        }
        case WM_KEYUP:
        {
            input::Modifier modifier = fetch_modifiers();
            input::Key key = translate_virtual_key(w_param);
            input::key_press(key, false, modifier);

            return 0;
        }
        case WM_LBUTTONDOWN:
        {
            input::Modifier modifier = translate_modifiers(w_param);
            input::mouse_click(input::MouseButton::Left, true, modifier);

            return 0;
        }
        case WM_LBUTTONUP:
        {
            input::Modifier modifier = translate_modifiers(w_param);
            input::mouse_click(input::MouseButton::Left, false, modifier);

            return 0;
        }
        case WM_MBUTTONDOWN:
        {
            input::Modifier modifier = translate_modifiers(w_param);
            input::mouse_click(input::MouseButton::Middle, true, modifier);

            return 0;
        }
        case WM_MBUTTONUP:
        {
            input::Modifier modifier = translate_modifiers(w_param);
            input::mouse_click(input::MouseButton::Middle, false, modifier);

            return 0;
        }
        case WM_MOUSEMOVE:
        {
            Int2 position;
            position.x = GET_X_LPARAM(l_param);
            position.y = GET_Y_LPARAM(l_param);
            input::mouse_move(position);

            return 0;
        }
        case WM_MOUSEWHEEL:
        {
            int scroll = GET_WHEEL_DELTA_WPARAM(w_param) / WHEEL_DELTA;
            Int2 velocity = {0, scroll};
            input::mouse_scroll(velocity);

            return 0;
        }
        case WM_RBUTTONDOWN:
        {
            input::Modifier modifier = translate_modifiers(w_param);
            input::mouse_click(input::MouseButton::Right, true, modifier);

            return 0;
        }
        case WM_RBUTTONUP:
        {
            input::Modifier modifier = translate_modifiers(w_param);
            input::mouse_click(input::MouseButton::Right, false, modifier);

            return 0;
        }
        case WM_SETCURSOR:
        {
            if(LOWORD(l_param) == HTCLIENT)
            {
                HCURSOR cursor = get_cursor_by_type(&platform, platform.cursor_type);
                SetCursor(cursor);
                return TRUE;
            }

            return FALSE;
        }
        case WM_SIZE:
        {
            int width = LOWORD(l_param);
            int height = HIWORD(l_param);
            platform.viewport = {width, height};
            double dpmm = get_dots_per_millimeter(&platform);
            if(functions_loaded)
            {
                resize_viewport(platform.viewport, dpmm);
            }

            return 0;
        }
    }
    return DefWindowProcW(hwnd, message, w_param, l_param);
}

static LocaleId match_closest_locale_id()
{
    LANGID id = GetUserDefaultUILanguage();
    WORD primary = PRIMARYLANGID(id);
    switch(primary)
    {
        default:
        case LANG_ENGLISH:
        {
            return LocaleId::Default;
        }
    }
}

#define MAKEINTATOMW(atom) \
    ((LPWSTR)((ULONG_PTR)((WORD)(atom))))

static bool main_start_up(HINSTANCE instance, int show_command)
{
    platform.base.locale_id = match_closest_locale_id();
    create_stack(&platform.base);
    bool loaded = load_localized_text(&platform.base);
    if(!loaded)
    {
        LOG_ERROR("Failed to load the localized text.");
        return false;
    }
    load_cursors(&platform);

    WNDCLASSEXW window_class = {};
    window_class.cbSize = sizeof window_class;
    window_class.style = CS_HREDRAW | CS_VREDRAW;
    window_class.lpfnWndProc = WindowProc;
    window_class.hInstance = instance;
    window_class.hIcon = LoadIcon(instance, IDI_APPLICATION);
    window_class.hIconSm = static_cast<HICON>(LoadIcon(instance, IDI_APPLICATION));
    window_class.hCursor = LoadCursor(nullptr, IDC_ARROW);
    window_class.lpszClassName = L"ArboretumWindowClass";
    ATOM registered_class = RegisterClassExW(&window_class);
    if(registered_class == 0)
    {
        LOG_ERROR("Failed to register the window class.");
        return false;
    }

    DWORD window_style = WS_OVERLAPPEDWINDOW;
    const char* app_name = platform.base.nonlocalized_text.app_name;
    wchar_t* title = utf8_to_wide_char(app_name, &platform.base.stack);
    platform.window = CreateWindowExW(WS_EX_APPWINDOW, MAKEINTATOMW(registered_class), title, window_style, CW_USEDEFAULT, CW_USEDEFAULT, window_width, window_height, nullptr, nullptr, instance, nullptr);
    STACK_DEALLOCATE(&platform.base.stack, title);
    if(!platform.window)
    {
        LOG_ERROR("Failed to create the window.");
        return false;
    }

    platform.device_context = GetDC(platform.window);
    if(!platform.device_context)
    {
        LOG_ERROR("Couldn't obtain the device context.");
        return false;
    }

    PIXELFORMATDESCRIPTOR descriptor = {};
    descriptor.nSize = sizeof descriptor;
    descriptor.nVersion = 1;
    descriptor.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    descriptor.iPixelType = PFD_TYPE_RGBA;
    descriptor.cColorBits = 32;
    descriptor.cDepthBits = 24;
    descriptor.cStencilBits = 8;
    descriptor.iLayerType = PFD_MAIN_PLANE;
    int format_index = ChoosePixelFormat(platform.device_context, &descriptor);
    if(format_index == 0)
    {
        LOG_ERROR("Failed to set up the pixel format.");
        return false;
    }
    if(SetPixelFormat(platform.device_context, format_index, &descriptor) == FALSE)
    {
        LOG_ERROR("Failed to set up the pixel format.");
        return false;
    }

    rendering_context = wglCreateContext(platform.device_context);
    if(!rendering_context)
    {
        LOG_ERROR("Couldn't create the rendering context.");
        return false;
    }

    ShowWindow(platform.window, show_command);

    // Set it to be this thread's rendering context.
    if(wglMakeCurrent(platform.device_context, rendering_context) == FALSE)
    {
        LOG_ERROR("Couldn't set this thread's rendering context (wglMakeCurrent failed).");
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

    started = editor_start_up(&platform.base);
    if(!started)
    {
        LOG_ERROR("Editor failed startup.");
        return false;
    }

    double dpmm = get_dots_per_millimeter(&platform);
    Int2 dimensions = get_window_dimensions(&platform);
    resize_viewport(dimensions, dpmm);

    return true;
}

static void main_shut_down()
{
    editor_shut_down();
    video::system_shut_down(functions_loaded);
    destroy_stack(&platform.base);

    if(rendering_context)
    {
        wglMakeCurrent(nullptr, nullptr);
        ReleaseDC(platform.window, platform.device_context);
        wglDeleteContext(rendering_context);
    }
    else if(platform.device_context)
    {
        ReleaseDC(platform.window, platform.device_context);
    }
    if(platform.window)
    {
        DestroyWindow(platform.window);
    }
}

static s64 get_clock_frequency()
{
    LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency);
    return frequency.QuadPart;
}

static s64 get_timestamp()
{
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return now.QuadPart;
}

static double get_second_duration(s64 start, s64 end, s64 frequency)
{
    return (end - start) / static_cast<double>(frequency);
}

static void go_to_sleep(double amount_to_sleep)
{
    DWORD milliseconds = 1000 * amount_to_sleep;
    Sleep(milliseconds);
}

static int main_loop()
{
    const double frame_frequency = 1.0 / 60.0;
    s64 clock_frequency = get_clock_frequency();

    MSG msg = {};
    for(;;)
    {
        s64 frame_start_time = get_timestamp();

        editor_update(&platform.base);
        input::system_update();

        SwapBuffers(platform.device_context);

        while(PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            if(msg.message == WM_QUIT)
            {
                return msg.wParam;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        // Sleep off any remaining time until the next frame.
        s64 frame_end_time = get_timestamp();
        double frame_thusfar = get_second_duration(frame_start_time, frame_end_time, clock_frequency);
        if(frame_thusfar < frame_frequency)
        {
            go_to_sleep(frame_frequency - frame_thusfar);
        }
    }
}

int CALLBACK WinMain(HINSTANCE instance, HINSTANCE previous_instance, LPSTR command_line, int show_command)
{
    // This is always null.
    static_cast<void>(previous_instance);
    // Call GetCommandLineW instead for the Unicode version of this string.
    static_cast<void>(command_line);

    if(!main_start_up(instance, show_command))
    {
        main_shut_down();
        return 0;
    }
    int result = main_loop();
    main_shut_down();

    return result;
}

#endif // defined(OS_WINDOWS)
