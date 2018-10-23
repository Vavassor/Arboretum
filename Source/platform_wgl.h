#ifndef PLATFORM_WGL_H_
#define PLATFORM_WGL_H_

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include "platform_video.h"
#include "wgl_extensions.h"

typedef struct PlatformWgl
{
    PlatformVideo base;
    HDC device_context;
    HGLRC rendering_context;
    HWND window;
    bool functions_loaded;
} PlatformWgl;

#endif // PLATFORM_WGL_H_
