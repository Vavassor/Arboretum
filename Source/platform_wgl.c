#include "platform_wgl.h"

static bool create(PlatformVideo* platform_base)
{
    PlatformWgl* platform = (PlatformWgl*) platform_base;

    HDC device_context = GetDC(platform->window);
    if(!device_context)
    {
        return false;
    }
    platform->device_context = device_context;

    PIXELFORMATDESCRIPTOR descriptor =
    {
        .nSize = sizeof descriptor,
        .nVersion = 1,
        .dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
        .iPixelType = PFD_TYPE_RGBA,
        .cColorBits = 32,
        .cDepthBits = 24,
        .cStencilBits = 8,
        .iLayerType = PFD_MAIN_PLANE,
    };
    int format_index = ChoosePixelFormat(platform->device_context, &descriptor);
    if(format_index == 0)
    {
        return false;
    }
    BOOL format_set = SetPixelFormat(platform->device_context, format_index,
            &descriptor);
    if(format_set == FALSE)
    {
        return false;
    }

    HGLRC rendering_context = wglCreateContext(platform->device_context);
    if(!rendering_context)
    {
        return false;
    }
    platform->rendering_context = rendering_context;

    if(wglMakeCurrent(platform->device_context, platform->rendering_context) == FALSE)
    {
        return false;
    }

    bool functions_loaded = ogl_LoadFunctions();
    if(!functions_loaded)
    {
        return false;
    }
    platform->functions_loaded = functions_loaded;

    return true;
}

static void destroy(PlatformVideo* platform_base)
{
    PlatformWgl* platform = (PlatformWgl*) platform_base;

    if(platform->rendering_context)
    {
        wglMakeCurrent(NULL, NULL);
        ReleaseDC(platform->window, platform->device_context);
        wglDeleteContext(platform->rendering_context);
    }
    else if(platform->device_context)
    {
        ReleaseDC(platform->window, platform->device_context);
    }
}

static void swap_buffers(PlatformVideo* platform_base)
{
    PlatformWgl* platform = (PlatformWgl*) platform_base;
    SwapBuffers(platform->device_context);
}

void set_up_platform_video_wgl(PlatformVideo* platform)
{
    platform->create = create;
    platform->destroy = destroy;
    platform->swap_buffers = swap_buffers;
}
