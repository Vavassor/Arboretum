#include "platform_video.h"

bool platform_video_create(PlatformVideo* platform)
{
    return platform->create(platform);
}

void platform_video_destroy(PlatformVideo* platform)
{
    platform->destroy(platform);
}

void platform_video_swap_buffers(PlatformVideo* platform)
{
    platform->swap_buffers(platform);
}
