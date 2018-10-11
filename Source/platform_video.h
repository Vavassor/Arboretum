#ifndef PLATFORM_VIDEO_H_
#define PLATFORM_VIDEO_H_

typedef enum VideoBackendType
{
    VIDEO_BACKEND_TYPE_D3D12,
    VIDEO_BACKEND_TYPE_GL,
} VideoBackendType;

typedef struct PlatformVideo
{
    VideoBackendType backend_type;
} PlatformVideo;

typedef struct PlatformVideoD3d12 PlatformVideoD3d12;
typedef struct PlatformVideoGl PlatformVideoGl;

#endif // PLATFORM_VIDEO_H_
