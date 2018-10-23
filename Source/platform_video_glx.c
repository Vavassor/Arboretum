#include "platform_video_glx.h"

#include "invalid_index.h"

#ifdef None
#undef None
#define X11_NONE 0L /* universal null resource or null atom */
#endif

typedef GLXContext (*glXCreateContextAttribsARBProc)(Display*, GLXFBConfig,
        GLXContext, Bool, const int*);

static GLXFBConfig choose_best_framebuffer_configuration(
        PlatformVideoGlx* platform)
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
    GLXFBConfig* framebuffer_configs = glXChooseFBConfig(platform->display,
            platform->screen, visual_attributes, &config_count);
    if(!framebuffer_configs)
    {
        return NULL;
    }

    int best_config = invalid_index;
    int worst_config = invalid_index;
    int best_samples = -1;
    int worst_samples = 999;
    for(int config_index = 0;
            config_index < config_count;
            config_index += 1)
    {
        XVisualInfo* visual_info = glXGetVisualFromFBConfig(platform->display,
                framebuffer_configs[config_index]);
        if(visual_info)
        {
            int sample_buffers;
            int samples;
            glXGetFBConfigAttrib(platform->display,
                    framebuffer_configs[config_index], GLX_SAMPLE_BUFFERS,
                    &sample_buffers);
            glXGetFBConfigAttrib(platform->display,
                    framebuffer_configs[config_index], GLX_SAMPLES, &samples);

            if(!is_valid_index(best_config)
                    || (sample_buffers && samples > best_samples))
            {
                best_config = config_index;
                best_samples = samples;
            }

            if(!is_valid_index(worst_config)
                    || !sample_buffers || samples < worst_samples)
            {
                worst_config = config_index;
                worst_samples = samples;
            }
        }
        XFree(visual_info);
    }
    GLXFBConfig chosen_framebuffer_config = framebuffer_configs[best_config];
    XFree(framebuffer_configs);

    return chosen_framebuffer_config;
}

static bool create(PlatformVideo* platform_base)
{
    PlatformVideoGlx* platform = (PlatformVideoGlx*) platform_base;

    GLXFBConfig chosen_framebuffer_config =
            choose_best_framebuffer_configuration(platform);
    if(!chosen_framebuffer_config)
    {
        return false;
    }
    platform->chosen_framebuffer_config = chosen_framebuffer_config;

    // Choose the abstract "Visual" type that will be used to describe both the
    // window and the OpenGL rendering context.
    platform->visual_info = glXGetVisualFromFBConfig(platform->display,
            chosen_framebuffer_config);
    if(!platform->visual_info)
    {
        return false;
    }

    glXCreateContextAttribsARBProc glXCreateContextAttribsARB = NULL;
    glXCreateContextAttribsARB =
            (glXCreateContextAttribsARBProc) glXGetProcAddressARB(
                    (const GLubyte*) "glXCreateContextAttribsARB");
    if(!glXCreateContextAttribsARB)
    {
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
    GLXContext rendering_context = glXCreateContextAttribsARB(platform->display,
            platform->chosen_framebuffer_config, NULL, True,
            context_attributes);
    if(!rendering_context)
    {
        return false;
    }
    platform->rendering_context = rendering_context;

    return true;
}

static void destroy(PlatformVideo* platform_base)
{
    PlatformVideoGlx* platform = (PlatformVideoGlx*) platform_base;

    if(platform->rendering_context)
    {
        glXDestroyContext(platform->display, platform->rendering_context);
    }
}

static void swap_buffers(PlatformVideo* platform_base)
{
    PlatformVideoGlx* platform = (PlatformVideoGlx*) platform_base;

    glXSwapBuffers(platform->display, platform->window);
}

bool platform_video_glx_create_post_window(PlatformVideo* platform_base)
{
    PlatformVideoGlx* platform = (PlatformVideoGlx*) platform_base;

    XMapWindow(platform->display, platform->window);

    Bool made_current = glXMakeCurrent(platform->display, platform->window,
            platform->rendering_context);
    if(!made_current)
    {
        return false;
    }

    platform->functions_loaded = ogl_LoadFunctions();
    if(!platform->functions_loaded)
    {
        return false;
    }

    return true;
}

void set_up_platform_video_glx(PlatformVideo* platform)
{
    platform->create = create;
    platform->destroy = destroy;
    platform->swap_buffers = swap_buffers;
}
