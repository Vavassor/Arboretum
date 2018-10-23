#include "platform_d3d12.h"

static bool create(PlatformVideo* platform_base)
{
    (void) platform_base;
    return true;
}

static void destroy(PlatformVideo* platform_base)
{
    (void) platform_base;
}

static void swap_buffers(PlatformVideo* platform_base)
{
    (void) platform_base;
}

void set_up_platform_video_d3d12(PlatformVideo* platform)
{
    platform->create = create;
    platform->destroy = destroy;
    platform->swap_buffers = swap_buffers;
}
