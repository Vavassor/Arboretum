#ifndef PLATFORM_VIDEO_H_
#define PLATFORM_VIDEO_H_

#include <stdbool.h>

typedef enum VideoBackendType
{
    VIDEO_BACKEND_TYPE_D3D12,
    VIDEO_BACKEND_TYPE_GL,
} VideoBackendType;

typedef struct PlatformVideo PlatformVideo;

typedef bool (*PlatformVideoCreateCall)(PlatformVideo* platform);
typedef void (*PlatformVideoDestroyCall)(PlatformVideo* platform);
typedef void (*PlatformVideoSwapBuffersCall)(PlatformVideo* platform);

struct PlatformVideo
{
    VideoBackendType backend_type;
    PlatformVideoCreateCall create;
    PlatformVideoDestroyCall destroy;
    PlatformVideoSwapBuffersCall swap_buffers;
};

bool platform_video_create(PlatformVideo* platform);
void platform_video_destroy(PlatformVideo* platform);
void platform_video_swap_buffers(PlatformVideo* platform);
void set_up_platform_video_d3d12(PlatformVideo* platform);
void set_up_platform_video_glx(PlatformVideo* platform);
void set_up_platform_video_wgl(PlatformVideo* platform);

#endif // PLATFORM_VIDEO_H_
