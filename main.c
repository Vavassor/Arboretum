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
#include "log.h"
#include "platform.h"
#include "string_utilities.h"
#include "unicode_load_tables.h"
#include "video.h"

#include <limits.h>
#include <locale.h>
#include <stdlib.h>
#include <time.h>

typedef GLXContext (*glXCreateContextAttribsARBProc)(Display*, GLXFBConfig, GLXContext, Bool, const int*);

typedef struct PlatformX11
{
    Platform base;

    InputKey key_table[256];
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
} PlatformX11;

static double get_dots_per_millimeter(PlatformX11* platform)
{
    char* resource = XResourceManagerString(platform->display);

    XrmInitialize();
    XrmDatabase database = XrmGetStringDatabase(resource);

    double dots_per_millimeter = 0.0;
    if(resource)
    {
        XrmValue value;
        char* type = NULL;
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
        case CURSOR_TYPE_ARROW:            return "left_ptr";
        case CURSOR_TYPE_HAND_POINTING:    return "hand1";
        case CURSOR_TYPE_I_BEAM:           return "xterm";
        case CURSOR_TYPE_PROHIBITION_SIGN: return "crossed_circle";
    }
}

void change_cursor(Platform* base, CursorType type)
{
    PlatformX11* platform = (PlatformX11*) base;
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
    PlatformX11* platform = (PlatformX11*) base;
    XSetICFocus(platform->input_context);
    platform->input_context_focused = true;
}

void end_composed_text(Platform* base)
{
    PlatformX11* platform = (PlatformX11*) base;
    if(platform->input_context)
    {
        XUnsetICFocus(platform->input_context);
    }
    platform->input_context_focused = false;
}

void set_composed_text_position(Platform* base, int x, int y)
{
    PlatformX11* platform = (PlatformX11*) base;
    ASSERT(platform->input_context_focused);

    XPoint location;
    location.x = x;
    location.y = y;
    XVaNestedList list = XVaCreateNestedList(0, XNSpotLocation, &location, NULL);
    XSetICValues(platform->input_context, XNPreeditAttributes, list, NULL);
    XFree(list);
}

bool copy_to_clipboard(Platform* base, char* clipboard)
{
    PlatformX11* platform = (PlatformX11*) base;

    XSetSelectionOwner(platform->display, platform->selection_clipboard, platform->window, CurrentTime);

    bool is_owner = XGetSelectionOwner(platform->display, platform->selection_clipboard) == platform->window;
    if(is_owner)
    {
        platform->clipboard = clipboard;
        platform->clipboard_size = string_size(clipboard);

        Window owner = XGetSelectionOwner(platform->display, platform->clipboard_manager);
        if(owner == X11_NONE)
        {
            log_error(&base->logger, "There's no clipboard manager.");
            return true;
        }

        Atom target_types[1] = {platform->utf8_string};
        unsigned char* property = (unsigned char*) target_types;
        XChangeProperty(platform->display, platform->window, platform->save_code, XA_ATOM, 32, PropModeReplace, property, 1);

        XConvertSelection(platform->display, platform->clipboard_manager, platform->save_targets, platform->save_code, platform->window, CurrentTime);
    }

    return is_owner;
}

void request_paste_from_clipboard(Platform* base)
{
    PlatformX11* platform = (PlatformX11*) base;

    XConvertSelection(platform->display, platform->selection_clipboard, platform->utf8_string, platform->paste_code, platform->window, CurrentTime);
}

static const int window_width = 800;
static const int window_height = 600;

static PlatformX11 platform;
static GLXContext rendering_context;
static bool functions_loaded;

static InputKey translate_key_sym(KeySym key_sym)
{
    switch(key_sym)
    {
        case XK_a:
        case XK_A:            return INPUT_KEY_A;
        case XK_apostrophe:   return INPUT_KEY_APOSTROPHE;
        case XK_b:
        case XK_B:            return INPUT_KEY_B;
        case XK_backslash:    return INPUT_KEY_BACKSLASH;
        case XK_BackSpace:    return INPUT_KEY_BACKSPACE;
        case XK_c:
        case XK_C:            return INPUT_KEY_C;
        case XK_comma:        return INPUT_KEY_COMMA;
        case XK_d:
        case XK_D:            return INPUT_KEY_D;
        case XK_Delete:       return INPUT_KEY_DELETE;
        case XK_Down:         return INPUT_KEY_DOWN_ARROW;
        case XK_e:
        case XK_E:            return INPUT_KEY_E;
        case XK_8:            return INPUT_KEY_EIGHT;
        case XK_End:          return INPUT_KEY_END;
        case XK_Return:       return INPUT_KEY_ENTER;
        case XK_equal:        return INPUT_KEY_EQUALS_SIGN;
        case XK_Escape:       return INPUT_KEY_ESCAPE;
        case XK_f:
        case XK_F:            return INPUT_KEY_F;
        case XK_F1:           return INPUT_KEY_F1;
        case XK_F2:           return INPUT_KEY_F2;
        case XK_F3:           return INPUT_KEY_F3;
        case XK_F4:           return INPUT_KEY_F4;
        case XK_F5:           return INPUT_KEY_F5;
        case XK_F6:           return INPUT_KEY_F6;
        case XK_F7:           return INPUT_KEY_F7;
        case XK_F8:           return INPUT_KEY_F8;
        case XK_F9:           return INPUT_KEY_F9;
        case XK_F10:          return INPUT_KEY_F10;
        case XK_F11:          return INPUT_KEY_F11;
        case XK_F12:          return INPUT_KEY_F12;
        case XK_5:            return INPUT_KEY_FIVE;
        case XK_4:            return INPUT_KEY_FOUR;
        case XK_g:
        case XK_G:            return INPUT_KEY_G;
        case XK_grave:        return INPUT_KEY_GRAVE_ACCENT;
        case XK_h:
        case XK_H:            return INPUT_KEY_H;
        case XK_Home:         return INPUT_KEY_HOME;
        case XK_i:
        case XK_I:            return INPUT_KEY_I;
        case XK_Insert:       return INPUT_KEY_INSERT;
        case XK_j:
        case XK_J:            return INPUT_KEY_J;
        case XK_k:
        case XK_K:            return INPUT_KEY_K;
        case XK_l:
        case XK_L:            return INPUT_KEY_L;
        case XK_Left:         return INPUT_KEY_LEFT_ARROW;
        case XK_bracketleft:  return INPUT_KEY_LEFT_BRACKET;
        case XK_m:
        case XK_M:            return INPUT_KEY_M;
        case XK_minus:        return INPUT_KEY_MINUS;
        case XK_n:
        case XK_N:            return INPUT_KEY_N;
        case XK_9:            return INPUT_KEY_NINE;
        case XK_KP_0:         return INPUT_KEY_NUMPAD_0;
        case XK_KP_1:         return INPUT_KEY_NUMPAD_1;
        case XK_KP_2:         return INPUT_KEY_NUMPAD_2;
        case XK_KP_3:         return INPUT_KEY_NUMPAD_3;
        case XK_KP_4:         return INPUT_KEY_NUMPAD_4;
        case XK_KP_5:         return INPUT_KEY_NUMPAD_5;
        case XK_KP_6:         return INPUT_KEY_NUMPAD_6;
        case XK_KP_7:         return INPUT_KEY_NUMPAD_7;
        case XK_KP_8:         return INPUT_KEY_NUMPAD_8;
        case XK_KP_9:         return INPUT_KEY_NUMPAD_9;
        case XK_KP_Decimal:   return INPUT_KEY_NUMPAD_DECIMAL;
        case XK_KP_Divide:    return INPUT_KEY_NUMPAD_DIVIDE;
        case XK_KP_Enter:     return INPUT_KEY_NUMPAD_ENTER;
        case XK_KP_Subtract:  return INPUT_KEY_NUMPAD_SUBTRACT;
        case XK_KP_Multiply:  return INPUT_KEY_NUMPAD_MULTIPLY;
        case XK_KP_Add:       return INPUT_KEY_NUMPAD_ADD;
        case XK_o:
        case XK_O:            return INPUT_KEY_O;
        case XK_1:            return INPUT_KEY_ONE;
        case XK_p:
        case XK_P:            return INPUT_KEY_P;
        case XK_Next:         return INPUT_KEY_PAGE_DOWN;
        case XK_Prior:        return INPUT_KEY_PAGE_UP;
        case XK_Pause:        return INPUT_KEY_PAUSE;
        case XK_period:       return INPUT_KEY_PERIOD;
        case XK_q:
        case XK_Q:            return INPUT_KEY_Q;
        case XK_r:
        case XK_R:            return INPUT_KEY_R;
        case XK_Right:        return INPUT_KEY_RIGHT_ARROW;
        case XK_bracketright: return INPUT_KEY_RIGHT_BRACKET;
        case XK_s:
        case XK_S:            return INPUT_KEY_S;
        case XK_semicolon:    return INPUT_KEY_SEMICOLON;
        case XK_7:            return INPUT_KEY_SEVEN;
        case XK_6:            return INPUT_KEY_SIX;
        case XK_slash:        return INPUT_KEY_SLASH;
        case XK_space:        return INPUT_KEY_SPACE;
        case XK_t:
        case XK_T:            return INPUT_KEY_T;
        case XK_Tab:          return INPUT_KEY_TAB;
        case XK_3:            return INPUT_KEY_THREE;
        case XK_2:            return INPUT_KEY_TWO;
        case XK_u:
        case XK_U:            return INPUT_KEY_U;
        case XK_Up:           return INPUT_KEY_UP_ARROW;
        case XK_v:
        case XK_V:            return INPUT_KEY_V;
        case XK_w:
        case XK_W:            return INPUT_KEY_W;
        case XK_x:
        case XK_X:            return INPUT_KEY_X;
        case XK_y:
        case XK_Y:            return INPUT_KEY_Y;
        case XK_z:
        case XK_Z:            return INPUT_KEY_Z;
        case XK_0:            return INPUT_KEY_ZERO;
        default:              return INPUT_KEY_UNKNOWN;
    }
}

static void build_key_table(PlatformX11* platform)
{
    for(int i = 0; i < 8; ++i)
    {
        platform->key_table[i] = INPUT_KEY_UNKNOWN;
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
    (void) call_data; // always null

    PlatformX11* platform = (PlatformX11*) client_data;
    platform->input_context = NULL;
    platform->input_method = NULL;
    platform->input_method_connected = false;

    log_error(&platform->base.logger, "Input method closed unexpectedly.");
}

static void close_prior_input_method_and_context(PlatformX11* platform)
{
    if(platform->input_method_connected)
    {
        if(platform->input_context)
        {
            XDestroyIC(platform->input_context);
            platform->input_context = NULL;
        }
        if(platform->input_method)
        {
            XCloseIM(platform->input_method);
            platform->input_method = NULL;
        }
        platform->input_method_connected = false;
    }
}

static void create_font_set(PlatformX11* platform, Display* display)
{
    int num_missing_charsets;
    char** missing_charsets;
    char* default_string;
    const char* font_names = "-adobe-helvetica-*-r-*-*-*-120-*-*-*-*-*-*,-misc-fixed-*-r-*-*-*-130-*-*-*-*-*-*";
    platform->font_set = XCreateFontSet(display, font_names, &missing_charsets, &num_missing_charsets, &default_string);
}

static XIMStyle negotiate_input_method_styles(PlatformX11* platform)
{
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
    return best_style;
}

static void create_input_context(PlatformX11* platform, XIMStyle best_style)
{
    XPoint spot_location = {0, 0};
    XVaNestedList list = XVaCreateNestedList(0, XNFontSet, platform->font_set, XNSpotLocation, &spot_location, NULL);
    XIMCallback destroy_callback;
    destroy_callback.client_data = (XPointer) platform;
    destroy_callback.callback = destroy_input_method;
    platform->input_context = XCreateIC(platform->input_method, XNInputStyle, best_style, XNClientWindow, platform->window, XNPreeditAttributes, list, XNStatusAttributes, list, XNDestroyCallback, &destroy_callback, NULL);
    XFree(list);
}

static void add_input_events_to_window(PlatformX11* platform, Display* display)
{
    XWindowAttributes attributes = {0};
    XGetWindowAttributes(display, platform->window, &attributes);
    long event_mask = attributes.your_event_mask;
    long input_method_event_mask;
    XGetICValues(platform->input_context, XNFilterEvents, &input_method_event_mask, NULL);
    XSelectInput(display, platform->window, event_mask | input_method_event_mask);
}

static void instantiate_input_method(Display* display, XPointer client_data, XPointer call_data)
{
    (void) call_data; // always null

    PlatformX11* platform = (PlatformX11*) client_data;

    close_prior_input_method_and_context(platform);

    platform->input_method = XOpenIM(display, NULL, NULL, NULL);
    if(!platform->input_method)
    {
        log_error(&platform->base.logger, "X Input Method failed to open.");
        return;
    }

    create_font_set(platform, display);
    if(!platform->font_set)
    {
        log_error(&platform->base.logger, "Failed to make a font set.");
        return;
    }

    XIMStyle best_style = negotiate_input_method_styles(platform);
    if(!best_style)
    {
        log_error(&platform->base.logger, "None of the input styles were supported.");
        return;
    }

    create_input_context(platform, best_style);
    if(!platform->input_context)
    {
        log_error(&platform->base.logger, "X Input Context failed to open.");
        return;
    }

    add_input_events_to_window(platform, display);

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
        int size = imin(16, end - period);
        copy_string(encoding, size, &locale[period + 1]);
        end = period;
    }

    int underscore = find_char(locale, '_');
    if(underscore != invalid_index)
    {
        int size = imin(4, end - underscore);
        copy_string(region, size, &locale[underscore + 1]);
        end = underscore;
    }

    int size = imin(4, end + 1);
    copy_string(language, size, locale);

    // LOG_DEBUG("%s, %s, %s, %s", language, region, encoding, modifier);

    return LOCALE_ID_DEFAULT;
}

static GLXFBConfig choose_best_framebuffer_configuration()
{
    const GLint visual_attributes[] =
    {
        GLX_X_RENDERABLE, True,
        GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
        GLX_RENDER_TYPE, GLX_RGBA_BIT,
        GLX_X_VISUAL_TYPE, GLX_TRUE_COLOR,
        GLX_RED_SIZE, 8,
        GLX_GREEN_SIZE, 8,
        GLX_BLUE_SIZE, 8,
        GLX_ALPHA_SIZE, 8,
        GLX_DEPTH_SIZE, 24,
        GLX_STENCIL_SIZE, 8,
        GLX_DOUBLEBUFFER, True,
        X11_NONE,
    };

    int config_count;
    GLXFBConfig* framebuffer_configs = glXChooseFBConfig(platform.display, platform.screen, visual_attributes, &config_count);
    if(!framebuffer_configs)
    {
        return NULL;
    }

    int best_config = invalid_index;
    int worst_config = invalid_index;
    int best_samples = -1;
    int worst_samples = 999;
    for(int i = 0; i < config_count; i += 1)
    {
        XVisualInfo* visual_info = glXGetVisualFromFBConfig(platform.display, framebuffer_configs[i]);
        if(visual_info)
        {
            int sample_buffers;
            int samples;
            glXGetFBConfigAttrib(platform.display, framebuffer_configs[i], GLX_SAMPLE_BUFFERS, &sample_buffers);
            glXGetFBConfigAttrib(platform.display, framebuffer_configs[i], GLX_SAMPLES, &samples);
            if(!is_valid_index(best_config) || (sample_buffers && samples > best_samples))
            {
                best_config = i;
                best_samples = samples;
            }
            if(!is_valid_index(worst_config) || !sample_buffers || samples < worst_samples)
            {
                worst_config = i;
                worst_samples = samples;
            }
        }
        XFree(visual_info);
    }
    GLXFBConfig chosen_framebuffer_config = framebuffer_configs[best_config];
    XFree(framebuffer_configs);

    return chosen_framebuffer_config;
}

static void create_window()
{
    long event_mask = StructureNotifyMask | KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask | FocusChangeMask | PropertyChangeMask;

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

static void set_up_clipboard()
{
    platform.selection_primary = XInternAtom(platform.display, "PRIMARY", False);
    platform.selection_clipboard = XInternAtom(platform.display, "CLIPBOARD", False);
    platform.utf8_string = XInternAtom(platform.display, "UTF8_STRING", False);
    platform.atom_pair = XInternAtom(platform.display, "ATOM_PAIR", False);
    platform.targets = XInternAtom(platform.display, "TARGETS", False);
    platform.multiple = XInternAtom(platform.display, "MULTIPLE", False);

    platform.clipboard_manager = XInternAtom(platform.display, "CLIPBOARD_MANAGER", False);
    platform.save_targets = XInternAtom(platform.display, "SAVE_TARGETS", False);

    // These are arbitrarily-named atoms chosen which will be used to
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

bool main_start_up()
{
    // Set the locale.
    char* locale = setlocale(LC_ALL, "");
    if(!locale)
    {
        log_error(&platform.base.logger, "Failed to set the locale.");
        return false;
    }

    // Connect to the X server, which is used for display and input services.
    platform.display = XOpenDisplay(NULL);
    if(!platform.display)
    {
        log_error(&platform.base.logger, "X Display failed to open.");
        return false;
    }
    platform.screen = DefaultScreen(platform.display);

    // Check if the X server is okay with the locale.
    Bool locale_supported = XSupportsLocale();
    if(!locale_supported)
    {
        log_error(&platform.base.logger, "X does not support locale %s.", locale);
        return false;
    }
    char* locale_modifiers = XSetLocaleModifiers("");
    if(!locale_modifiers)
    {
        log_error(&platform.base.logger, "Failed to set locale modifiers.");
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
                log_error(&platform.base.logger, "Failed to determine the text locale.");
                return false;
            }
        }
    }
    platform.base.locale_id = match_closest_locale_id(text_locale);

    create_stack(&platform.base);
    bool loaded = load_localized_text(&platform.base);
    if(!loaded)
    {
        log_error(&platform.base.logger, "Failed to load the localized text.");
        return false;
    }

    GLXFBConfig chosen_framebuffer_config = choose_best_framebuffer_configuration();
    if(!chosen_framebuffer_config)
    {
        log_error(&platform.base.logger, "Failed to retrieve a framebuffer configuration.");
        return false;
    }

    // Choose the abstract "Visual" type that will be used to describe both the
    // window and the OpenGL rendering context.
    platform.visual_info = glXGetVisualFromFBConfig(platform.display, chosen_framebuffer_config);
    if(!platform.visual_info)
    {
        log_error(&platform.base.logger, "Wasn't able to choose an appropriate Visual type given the requested attributes. [The Visual type contains information on color mappings for the display hardware]");
        return false;
    }

    create_window();

    // Register to receive window close messages.
    platform.wm_delete_window = XInternAtom(platform.display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(platform.display, platform.window, &platform.wm_delete_window, 1);

    XStoreName(platform.display, platform.window, platform.base.nonlocalized_text.app_name);
    XSetIconName(platform.display, platform.window, platform.base.nonlocalized_text.app_name);

    // Register for the input method context to be created.
    XPointer client_data = (XPointer) &platform;
    XRegisterIMInstantiateCallback(platform.display, NULL, NULL, NULL, instantiate_input_method, client_data);

    set_up_clipboard();

    glXCreateContextAttribsARBProc glXCreateContextAttribsARB = NULL;
	glXCreateContextAttribsARB = (glXCreateContextAttribsARBProc) glXGetProcAddressARB((const GLubyte*) "glXCreateContextAttribsARB");
	if(!glXCreateContextAttribsARB)
	{
	    log_error(&platform.base.logger, "Couldn't load glXCreateContextAttribsARB.");
		return false;
	}

    // Create the rendering context for OpenGL. The rendering context can only
    // be "made current" after the window is mapped (with XMapWindow).
	const int context_attributes[] =
	{
		GLX_CONTEXT_MAJOR_VERSION_ARB, 3,
		GLX_CONTEXT_MINOR_VERSION_ARB, 3,
		X11_NONE,
	};
    rendering_context = glXCreateContextAttribsARB(platform.display, chosen_framebuffer_config, NULL, True, context_attributes);
    if(!rendering_context)
    {
        log_error(&platform.base.logger, "Couldn't create a GLX rendering context.");
        return false;
    }

    XMapWindow(platform.display, platform.window);

    Bool made_current = glXMakeCurrent(platform.display, platform.window, rendering_context);
    if(!made_current)
    {
        log_error(&platform.base.logger, "Failed to attach the GLX context to the platform.");
        return false;
    }

    functions_loaded = ogl_LoadFunctions();
    if(!functions_loaded)
    {
        log_error(&platform.base.logger, "OpenGL functions could not be loaded!");
        return false;
    }

    platform.base.input_context = input_create_context(&platform.base.stack);

    bool started = editor_start_up(&platform.base);
    if(!started)
    {
        log_error(&platform.base.logger, "Editor failed startup.");
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
    editor_shut_down(functions_loaded);
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

static InputModifier translate_modifiers(unsigned int state)
{
    InputModifier modifier = {};
    modifier.shift = state & ShiftMask;
    modifier.control = state & ControlMask;
    modifier.alt = state & Mod1Mask;
    modifier.super = state & Mod4Mask;
    modifier.caps_lock = state & LockMask;
    modifier.num_lock = state & Mod2Mask;
    return modifier;
}

static InputKey translate_key(unsigned int scancode)
{
    if(scancode < 0 || scancode > 255)
    {
        return INPUT_KEY_UNKNOWN;
    }
    else
    {
        return platform.key_table[scancode];
    }
}

static void handle_button_press(XButtonEvent event)
{
    InputContext* input_context = platform.base.input_context;
    InputModifier modifier = translate_modifiers(event.state);
    if(event.button == Button1)
    {
        input_mouse_click(input_context, MOUSE_BUTTON_LEFT, true, modifier);
    }
    else if(event.button == Button2)
    {
        input_mouse_click(input_context, MOUSE_BUTTON_MIDDLE, true, modifier);
    }
    else if(event.button == Button3)
    {
        input_mouse_click(input_context, MOUSE_BUTTON_RIGHT, true, modifier);
    }
    else if(event.button == Button4)
    {
        Int2 velocity = {0, +1};
        input_mouse_scroll(input_context, velocity);
    }
    else if(event.button == Button5)
    {
        Int2 velocity = {0, -1};
        input_mouse_scroll(input_context, velocity);
    }
}

static void handle_button_release(XButtonEvent event)
{
    InputContext* input_context = platform.base.input_context;
    InputModifier modifier = translate_modifiers(event.state);
    if(event.button == Button1)
    {
        input_mouse_click(input_context, MOUSE_BUTTON_LEFT, false, modifier);
    }
    else if(event.button == Button2)
    {
        input_mouse_click(input_context, MOUSE_BUTTON_MIDDLE, false, modifier);
    }
    else if(event.button == Button3)
    {
        input_mouse_click(input_context, MOUSE_BUTTON_RIGHT, false, modifier);
    }
}

static void handle_client_message(XClientMessageEvent client_message)
{
    Atom message = (Atom) client_message.data.l[0];
    if(message == platform.wm_delete_window)
    {
        platform.close_window_requested = true;
    }
}

static void handle_configure_notify(XConfigureRequestEvent configure)
{
    double dpmm = get_dots_per_millimeter(&platform);
    Int2 dimensions = {configure.width, configure.height};
    resize_viewport(dimensions, dpmm);
}

static void handle_focus_in(XFocusInEvent focus_in)
{
    if(focus_in.window == platform.window && platform.input_context_focused)
    {
        XSetICFocus(platform.input_context);
    }
}

static void handle_focus_out(XFocusInEvent focus_out)
{
    if(focus_out.window == platform.window && platform.input_context_focused)
    {
        XUnsetICFocus(platform.input_context);
    }
}

static void handle_key_press(XKeyEvent press)
{
    InputContext* input_context = platform.base.input_context;

    // Process key presses that are used for controls and hotkeys.
    InputKey key = translate_key(press.keycode);
    InputModifier modifier = translate_modifiers(press.state);
    input_key_press(input_context, key, true, modifier);

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
            case XLookupBoth:
            {
                if(key_sym == XK_BackSpace || key_sym == XK_Delete || key_sym == XK_Return)
                {
                    break;
                }
                if(!only_control_characters(buffer))
                {
                    input_composed_text_entered(input_context, buffer);
                }
                break;
            }
            case XLookupChars:
            {
                if(!only_control_characters(buffer))
                {
                    input_composed_text_entered(input_context, buffer);
                }
                break;
            }
        }
    }
    else
    {
        // @Incomplete: throw away keystrokes?
    }
}

static void handle_key_release(XKeyEvent release)
{
    InputContext* input_context = platform.base.input_context;

    InputKey key = translate_key(release.keycode);
    InputModifier modifier = translate_modifiers(release.state);
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
        input_key_press(input_context, key, false, modifier);
    }
}

static void handle_motion_notify(XMotionEvent event)
{
    InputContext* input_context = platform.base.input_context;
    Int2 position = {event.x, event.y};
    input_mouse_move(input_context, position);
}

static void handle_selection_clear()
{
    // Another application overwrote our clipboard contents, so they can
    // be deallocated now.
    editor_destroy_clipboard_copy(platform.clipboard);
    platform.clipboard = NULL;
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
        unsigned char* property = NULL;
        XGetWindowProperty(platform.display, platform.window, notification.property, 0, 0, False, AnyPropertyType, &type, &format, &count, &size, &property);
        XFree(property);

        Atom incr = XInternAtom(platform.display, "INCR", False);
        if(type == incr)
        {
            log_error(&platform.base.logger, "Clipboard does not support incremental transfers (INCR).");
        }
        else
        {
            XGetWindowProperty(platform.display, platform.window, notification.property, 0, size, False, AnyPropertyType, &type, &format, &count, &size, &property);
            char* paste = (char*) property;
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
        unsigned char* contents = (unsigned char*) platform.clipboard;
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

        Atom* targets = (Atom*) property;
        for(unsigned long i = 0; i < count; i += 2)
        {
            char* name = XGetAtomName(platform.display, targets[i]);
            XFree(name);
            if(targets[i] == platform.utf8_string)
            {
                unsigned char* contents = (unsigned char*) platform.clipboard;
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
        Atom target_types[4] =
        {
            platform.targets,
            platform.multiple,
            platform.save_targets,
            platform.utf8_string,
        };
        unsigned char* targets = (unsigned char*) target_types;
        XChangeProperty(request.display, request.requestor, request.property, XA_ATOM, 32, PropModeReplace, targets, 4);

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
            handle_button_press(event.xbutton);
            break;
        }
        case ButtonRelease:
        {
            handle_button_release(event.xbutton);
            break;
        }
        case ClientMessage:
        {
            handle_client_message(event.xclient);
            break;
        }
        case ConfigureNotify:
        {
            handle_configure_notify(event.xconfigurerequest);
            break;
        }
        case FocusIn:
        {
            handle_focus_in(event.xfocus);
            break;
        }
        case FocusOut:
        {
            handle_focus_out(event.xfocus);
            break;
        }
        case KeyPress:
        {
            handle_key_press(event.xkey);
            break;
        }
        case KeyRelease:
        {
            handle_key_release(event.xkey);
            break;
        }
        case MotionNotify:
        {
            handle_motion_notify(event.xmotion);
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
    int64_t nanoseconds = resolution.tv_nsec + resolution.tv_sec * 1000000000;
    return ((double) nanoseconds) / 1.0e9;
}

double get_time(double clock_frequency)
{
    struct timespec timestamp;
    clock_gettime(CLOCK_MONOTONIC, &timestamp);
    int64_t nanoseconds = timestamp.tv_nsec + timestamp.tv_sec * 1000000000;
    return ((double) nanoseconds) * clock_frequency;
}

void go_to_sleep(double amount_to_sleep)
{
    struct timespec requested_time;
    requested_time.tv_sec = 0;
    requested_time.tv_nsec = (int64_t) (1.0e9 * amount_to_sleep);
    clock_nanosleep(CLOCK_MONOTONIC, 0, &requested_time, NULL);
}

void main_loop()
{
    const double frame_frequency = 1.0 / 60.0;
    double clock_frequency = get_clock_frequency();

    for(;;)
    {
        double frame_start_time = get_time(clock_frequency);

        editor_update(&platform.base);
        input_update_context(platform.base.input_context);

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
    (void) argc;
    (void) argv;

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

typedef struct PlatformWindows
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
} PlatformWindows;

static void load_cursors(PlatformWindows* platform)
{
    UINT flags = LR_DEFAULTSIZE | LR_SHARED;
    platform->cursor_arrow = (HCURSOR) LoadImage(NULL, IDC_ARROW, IMAGE_CURSOR, 0, 0, flags);
    platform->cursor_hand_pointing = (HCURSOR) LoadImage(NULL, IDC_HAND, IMAGE_CURSOR, 0, 0, flags);
    platform->cursor_i_beam = (HCURSOR) LoadImage(NULL, IDC_IBEAM, IMAGE_CURSOR, 0, 0, flags);
    platform->cursor_prohibition_sign = (HCURSOR) LoadImage(NULL, IDC_NO, IMAGE_CURSOR, 0, 0, flags);
}

static HCURSOR get_cursor_by_type(PlatformWindows* platform, CursorType type)
{
    switch(type)
    {
        default:
        case CURSOR_TYPE_ARROW:            return platform->cursor_arrow;
        case CURSOR_TYPE_HAND_POINTING:    return platform->cursor_hand_pointing;
        case CURSOR_TYPE_I_BEAM:           return platform->cursor_i_beam;
        case CURSOR_TYPE_PROHIBITION_SIGN: return platform->cursor_prohibition_sign;
    }
}

void change_cursor(Platform* base, CursorType type)
{
    PlatformWindows* platform = (PlatformWindows*) base;
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
    candidate_position.ptCurrentPos = (POINT){position.x, position.y};
    candidate_position.rcArea = (RECT){0, 0, 0, 0};
    ImmSetCandidateWindow(context, &candidate_position);
}

void set_composed_text_position(Platform* base, int x, int y)
{
    PlatformWindows* platform = (PlatformWindows*) base;

    Int2 position = {x, y};
    if(int2_not_equals(position, platform->composed_text_position))
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
    PlatformWindows* platform = (PlatformWindows*) base;

    ImmAssociateContextEx(platform->window, NULL, IACE_DEFAULT);

    platform->input_context_focused = true;
}

void end_composed_text(Platform* base)
{
    PlatformWindows* platform = (PlatformWindows*) base;

    HIMC context = ImmGetContext(platform->window);
    if(context)
    {
        reset_composing(platform, context);
        ImmReleaseContext(platform->window, context);
    }
    
    ImmAssociateContextEx(platform->window, NULL, 0);

    platform->input_context_focused = false;
}

bool copy_to_clipboard(Platform* base, char* clipboard)
{
    PlatformWindows* platform = (PlatformWindows*) base;

    // Convert the contents to UTF-16.
    wchar_t* wide = utf8_to_wide_char_stack(clipboard, &base->stack);
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

    LPWSTR wide_clone = (LPWSTR) GlobalLock(handle);
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
    PlatformWindows* platform = (PlatformWindows*) base;

    BOOL has_utf16 = IsClipboardFormatAvailable(CF_UNICODETEXT);
    if(!has_utf16)
    {
        log_error(&base->logger, "Paste format UTF-16 was not available.");
        return;
    }

    // Get the actual paste and convert to UTF-8 text.
    char* paste = NULL;
    BOOL opened = OpenClipboard(platform->window);
    if(opened)
    {
        HGLOBAL data = GetClipboardData(CF_UNICODETEXT);
        if(data)
        {
            LPWSTR wide = (LPWSTR) GlobalLock(data);
            if(wide)
            {
                paste = wide_char_to_utf8_stack(wide, &base->stack);
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
        log_error(&base->logger, "Paste failed.");
    }
}

static Int2 get_window_dimensions(PlatformWindows* platform)
{
    RECT rect;
    BOOL got = GetClientRect(platform->window, &rect);
    ASSERT(got);
    return (Int2){rect.right, rect.bottom};
}

static double get_dots_per_millimeter(PlatformWindows* platform)
{
    const double millimeters_per_inch = 25.4;
    UINT dpi = 96; // GetDpiForWindow(platform->window);
    return dpi / millimeters_per_inch;
}

static InputKey translate_virtual_key(WPARAM w_param)
{
    switch(w_param)
    {
        case '0':           return INPUT_KEY_ZERO;
        case '1':           return INPUT_KEY_ONE;
        case '2':           return INPUT_KEY_TWO;
        case '3':           return INPUT_KEY_THREE;
        case '4':           return INPUT_KEY_FOUR;
        case '5':           return INPUT_KEY_FIVE;
        case '6':           return INPUT_KEY_SIX;
        case '7':           return INPUT_KEY_SEVEN;
        case '8':           return INPUT_KEY_EIGHT;
        case '9':           return INPUT_KEY_NINE;
        case 'A':           return INPUT_KEY_A;
        case VK_ADD:        return INPUT_KEY_NUMPAD_ADD;
        case 'B':           return INPUT_KEY_B;
        case VK_BACK:       return INPUT_KEY_BACKSPACE;
        case 'C':           return INPUT_KEY_C;
        case 'D':           return INPUT_KEY_D;
        case VK_DECIMAL:    return INPUT_KEY_NUMPAD_DECIMAL;
        case VK_DELETE:     return INPUT_KEY_DELETE;
        case VK_DIVIDE:     return INPUT_KEY_NUMPAD_DIVIDE;
        case VK_DOWN:       return INPUT_KEY_DOWN_ARROW;
        case 'E':           return INPUT_KEY_E;
        case VK_END:        return INPUT_KEY_END;
        case VK_ESCAPE:     return INPUT_KEY_ESCAPE;
        case 'F':           return INPUT_KEY_F;
        case VK_F1:         return INPUT_KEY_F1;
        case VK_F2:         return INPUT_KEY_F2;
        case VK_F3:         return INPUT_KEY_F3;
        case VK_F4:         return INPUT_KEY_F4;
        case VK_F5:         return INPUT_KEY_F5;
        case VK_F6:         return INPUT_KEY_F6;
        case VK_F7:         return INPUT_KEY_F7;
        case VK_F8:         return INPUT_KEY_F8;
        case VK_F9:         return INPUT_KEY_F9;
        case VK_F10:        return INPUT_KEY_F10;
        case VK_F11:        return INPUT_KEY_F11;
        case VK_F12:        return INPUT_KEY_F12;
        case 'G':           return INPUT_KEY_G;
        case 'H':           return INPUT_KEY_H;
        case VK_HOME:       return INPUT_KEY_HOME;
        case 'I':           return INPUT_KEY_I;
        case VK_INSERT:     return INPUT_KEY_INSERT;
        case 'J':           return INPUT_KEY_J;
        case 'K':           return INPUT_KEY_K;
        case 'L':           return INPUT_KEY_L;
        case VK_LEFT:       return INPUT_KEY_LEFT_ARROW;
        case 'M':           return INPUT_KEY_M;
        case VK_MULTIPLY:   return INPUT_KEY_NUMPAD_MULTIPLY;
        case 'N':           return INPUT_KEY_N;
        case VK_NUMPAD0:    return INPUT_KEY_NUMPAD_0;
        case VK_NUMPAD1:    return INPUT_KEY_NUMPAD_1;
        case VK_NUMPAD2:    return INPUT_KEY_NUMPAD_2;
        case VK_NUMPAD3:    return INPUT_KEY_NUMPAD_3;
        case VK_NUMPAD4:    return INPUT_KEY_NUMPAD_4;
        case VK_NUMPAD5:    return INPUT_KEY_NUMPAD_5;
        case VK_NUMPAD6:    return INPUT_KEY_NUMPAD_6;
        case VK_NUMPAD7:    return INPUT_KEY_NUMPAD_7;
        case VK_NUMPAD8:    return INPUT_KEY_NUMPAD_8;
        case VK_NUMPAD9:    return INPUT_KEY_NUMPAD_9;
        case VK_NEXT:       return INPUT_KEY_PAGE_DOWN;
        case 'O':           return INPUT_KEY_O;
        case VK_OEM_1:      return INPUT_KEY_SEMICOLON;
        case VK_OEM_2:      return INPUT_KEY_SLASH;
        case VK_OEM_3:      return INPUT_KEY_GRAVE_ACCENT;
        case VK_OEM_4:      return INPUT_KEY_LEFT_BRACKET;
        case VK_OEM_5:      return INPUT_KEY_BACKSLASH;
        case VK_OEM_6:      return INPUT_KEY_RIGHT_BRACKET;
        case VK_OEM_7:      return INPUT_KEY_APOSTROPHE;
        case VK_OEM_COMMA:  return INPUT_KEY_COMMA;
        case VK_OEM_MINUS:  return INPUT_KEY_MINUS;
        case VK_OEM_PERIOD: return INPUT_KEY_PERIOD;
        case VK_OEM_PLUS:   return INPUT_KEY_EQUALS_SIGN;
        case 'P':           return INPUT_KEY_P;
        case VK_PAUSE:      return INPUT_KEY_PAUSE;
        case VK_PRIOR:      return INPUT_KEY_PAGE_UP;
        case 'Q':           return INPUT_KEY_Q;
        case 'R':           return INPUT_KEY_R;
        case VK_RETURN:     return INPUT_KEY_ENTER;
        case VK_RIGHT:      return INPUT_KEY_RIGHT_ARROW;
        case 'S':           return INPUT_KEY_S;
        case VK_SPACE:      return INPUT_KEY_SPACE;
        case VK_SUBTRACT:   return INPUT_KEY_NUMPAD_SUBTRACT;
        case 'T':           return INPUT_KEY_T;
        case VK_TAB:        return INPUT_KEY_TAB;
        case 'U':           return INPUT_KEY_U;
        case VK_UP:         return INPUT_KEY_UP_ARROW;
        case 'V':           return INPUT_KEY_V;
        case 'W':           return INPUT_KEY_W;
        case 'X':           return INPUT_KEY_X;
        case 'Y':           return INPUT_KEY_Y;
        case 'Z':           return INPUT_KEY_Z;
        default:            return INPUT_KEY_UNKNOWN;
    }
}

static InputModifier translate_modifiers(WPARAM w_param)
{
    InputModifier modifier = {0};
    modifier.control = w_param & MK_CONTROL;
    modifier.shift = w_param & MK_SHIFT;
    return modifier;
}

static InputModifier fetch_modifiers()
{
    InputModifier modifier = {0};
    modifier.alt = GetKeyState(VK_MENU) & 0x8000;
    modifier.control = GetKeyState(VK_CONTROL) & 0x8000;
    modifier.shift = GetKeyState(VK_SHIFT) & 0x8000;
    return modifier;
}

static const int window_width = 800;
static const int window_height = 600;

static PlatformWindows platform;
static HGLRC rendering_context;
static bool functions_loaded;

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
                wglMakeCurrent(NULL, NULL);
                ReleaseDC(hwnd, dc);
                wglDeleteContext(rc);
            }
            DestroyWindow(hwnd);
            if(hwnd == platform.window)
            {
                platform.window = NULL;
            }
            return 0;
        }
        case WM_DPICHANGED:
        {
            WORD dpi = HIWORD(w_param);
            double dots_per_millimeter = dpi / 25.4;
            resize_viewport(platform.viewport, dots_per_millimeter);

            RECT* suggested_rect = (RECT*) l_param;
            int left = suggested_rect->left;
            int top = suggested_rect->top;
            int width = suggested_rect->right - suggested_rect->left;
            int height = suggested_rect->bottom - suggested_rect->top;
            UINT flags = SWP_NOZORDER | SWP_NOACTIVATE;
            SetWindowPos(hwnd, NULL, left, top, width, height, flags);

            return 0;
        }
        case WM_CHAR:
        {
            if(platform.input_context_focused)
            {
                wchar_t wide[2] = {(wchar_t) w_param, L'\0'};
                char* text = wide_char_to_utf8_stack(wide, &platform.base.stack);
                if(!only_control_characters(text))
                {
                    input_composed_text_entered(text);
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
                    log_error(&platform.base.logger, "Failed to get the input method context.");
                    break;
                }

                move_input_method(context, platform.composed_text_position);

                int bytes = ImmGetCompositionStringW(context, GCS_RESULTSTR, NULL, 0);
                bytes += sizeof(wchar_t);
                wchar_t* string = (wchar_t*) stack_allocate(&platform.base.stack, bytes);

                ImmGetCompositionStringW(context, GCS_RESULTSTR, string, bytes);
                ImmReleaseContext(hwnd, context);

                char* text = wide_char_to_utf8_stack(string, &platform.base.stack);
                input_composed_text_entered(text);

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
                InputModifier modifier = fetch_modifiers();
                InputKey key = translate_virtual_key(w_param);
                input_key_press(key, true, modifier);

                return 0;
            }
        }
        case WM_KEYUP:
        {
            InputModifier modifier = fetch_modifiers();
            InputKey key = translate_virtual_key(w_param);
            input_key_press(key, false, modifier);

            return 0;
        }
        case WM_LBUTTONDOWN:
        {
            InputModifier modifier = translate_modifiers(w_param);
            input_mouse_click(MOUSE_BUTTON_LEFT, true, modifier);

            return 0;
        }
        case WM_LBUTTONUP:
        {
            InputModifier modifier = translate_modifiers(w_param);
            input_mouse_click(MOUSE_BUTTON_LEFT, false, modifier);

            return 0;
        }
        case WM_MBUTTONDOWN:
        {
            InputModifier modifier = translate_modifiers(w_param);
            input_mouse_click(MOUSE_BUTTON_MIDDLE, true, modifier);

            return 0;
        }
        case WM_MBUTTONUP:
        {
            InputModifier modifier = translate_modifiers(w_param);
            input_mouse_click(MOUSE_BUTTON_MIDDLE, false, modifier);

            return 0;
        }
        case WM_MOUSEMOVE:
        {
            Int2 position;
            position.x = GET_X_LPARAM(l_param);
            position.y = GET_Y_LPARAM(l_param);
            input_mouse_move(position);

            return 0;
        }
        case WM_MOUSEWHEEL:
        {
            int scroll = GET_WHEEL_DELTA_WPARAM(w_param) / WHEEL_DELTA;
            Int2 velocity = {0, scroll};
            input_mouse_scroll(velocity);

            return 0;
        }
        case WM_RBUTTONDOWN:
        {
            InputModifier modifier = translate_modifiers(w_param);
            input_mouse_click(MOUSE_BUTTON_RIGHT, true, modifier);

            return 0;
        }
        case WM_RBUTTONUP:
        {
            InputModifier modifier = translate_modifiers(w_param);
            input_mouse_click(MOUSE_BUTTON_RIGHT, false, modifier);

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
            platform.viewport = (Int2){width, height};
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
            return LOCALE_ID_DEFAULT;
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
        log_error(&platform.base.logger, "Failed to load the localized text.");
        return false;
    }
    load_cursors(&platform);

    WNDCLASSEXW window_class = {0};
    window_class.cbSize = sizeof window_class;
    window_class.style = CS_HREDRAW | CS_VREDRAW;
    window_class.lpfnWndProc = WindowProc;
    window_class.hInstance = instance;
    window_class.hIcon = LoadIcon(instance, IDI_APPLICATION);
    window_class.hIconSm = (HICON) LoadIcon(instance, IDI_APPLICATION);
    window_class.hCursor = LoadCursor(NULL, IDC_ARROW);
    window_class.lpszClassName = L"ArboretumWindowClass";
    ATOM registered_class = RegisterClassExW(&window_class);
    if(registered_class == 0)
    {
        log_error(&platform.base.logger, "Failed to register the window class.");
        return false;
    }

    DWORD window_style = WS_OVERLAPPEDWINDOW;
    const char* app_name = platform.base.nonlocalized_text.app_name;
    wchar_t* title = utf8_to_wide_char_stack(app_name, &platform.base.stack);
    platform.window = CreateWindowExW(WS_EX_APPWINDOW, MAKEINTATOMW(registered_class), title, window_style, CW_USEDEFAULT, CW_USEDEFAULT, window_width, window_height, NULL, NULL, instance, NULL);
    STACK_DEALLOCATE(&platform.base.stack, title);
    if(!platform.window)
    {
        log_error(&platform.base.logger, "Failed to create the window.");
        return false;
    }

    platform.device_context = GetDC(platform.window);
    if(!platform.device_context)
    {
        log_error(&platform.base.logger, "Couldn't obtain the device context.");
        return false;
    }

    PIXELFORMATDESCRIPTOR descriptor = {0};
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
        log_error(&platform.base.logger, "Failed to set up the pixel format.");
        return false;
    }
    if(SetPixelFormat(platform.device_context, format_index, &descriptor) == FALSE)
    {
        log_error(&platform.base.logger, "Failed to set up the pixel format.");
        return false;
    }

    rendering_context = wglCreateContext(platform.device_context);
    if(!rendering_context)
    {
        log_error(&platform.base.logger, "Couldn't create the rendering context.");
        return false;
    }

    ShowWindow(platform.window, show_command);

    // Set it to be this thread's rendering context.
    if(wglMakeCurrent(platform.device_context, rendering_context) == FALSE)
    {
        log_error(&platform.base.logger, "Couldn't set this thread's rendering context (wglMakeCurrent failed).");
        return false;
    }

    functions_loaded = ogl_LoadFunctions();
    if(!functions_loaded)
    {
        log_error(&platform.base.logger, "OpenGL functions could not be loaded!");
        return false;
    }

    input_system_start_up();

    bool started = video_system_start_up();
    if(!started)
    {
        log_error(&platform.base.logger, "Video system failed startup.");
        return false;
    }

    started = editor_start_up(&platform.base);
    if(!started)
    {
        log_error(&platform.base.logger, "Editor failed startup.");
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
    video_system_shut_down(functions_loaded);
    destroy_stack(&platform.base);

    if(rendering_context)
    {
        wglMakeCurrent(NULL, NULL);
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

static int64_t get_clock_frequency()
{
    LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency);
    return frequency.QuadPart;
}

static int64_t get_timestamp()
{
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return now.QuadPart;
}

static double get_second_duration(int64_t start, int64_t end, int64_t frequency)
{
    return (end - start) / ((double) frequency);
}

static void go_to_sleep(double amount_to_sleep)
{
    DWORD milliseconds = (DWORD) (1000.0 * amount_to_sleep);
    Sleep(milliseconds);
}

static int main_loop()
{
    const double frame_frequency = 1.0 / 60.0;
    int64_t clock_frequency = get_clock_frequency();

    MSG msg = {0};
    for(;;)
    {
        int64_t frame_start_time = get_timestamp();

        editor_update(&platform.base);
        input_system_update();

        SwapBuffers(platform.device_context);

        while(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            if(msg.message == WM_QUIT)
            {
                return (int) msg.wParam;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        // Sleep off any remaining time until the next frame.
        int64_t frame_end_time = get_timestamp();
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
    (void) previous_instance;
    // Call GetCommandLineW instead for the Unicode version of this string.
    (void) command_line;

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
