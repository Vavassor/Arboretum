#ifndef PLATFORM_D3D12_H_
#define PLATFORM_D3D12_H_

#include "platform_video.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

typedef struct PlatformVideoD3d12
{
    PlatformVideo base;
    HWND window;
} PlatformVideoD3d12;

#endif // PLATFORM_D3D12_H_
