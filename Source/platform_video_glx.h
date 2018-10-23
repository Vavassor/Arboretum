#ifndef PLATFORM_VIDEO_GLX_H_
#define PLATFORM_VIDEO_GLX_H_

#include "platform_video.h"
#include "gl_core_3_3.h"
#include "glx_extensions.h"

typedef struct PlatformVideoGlx
{
    PlatformVideo base;
    Display* display;
    XVisualInfo* visual_info;
    GLXFBConfig chosen_framebuffer_config;
    GLXContext rendering_context;
    Window window;
    int screen;
    bool functions_loaded;
} PlatformVideoGlx;

bool platform_video_glx_create_post_window(PlatformVideo* platform);

#endif // PLATFORM_VIDEO_GLX_H_
