#ifndef PLATFORM_D3D12_H_
#define PLATFORM_D3D12_H_

#include "platform_video.h"

#define COBJMACROS
#include <initguid.h>
#include <d3d12.h>
#include <dxgi1_6.h>

struct PlatformVideoD3d12
{
    PlatformVideo base;
    ID3D12Debug* debug_controller;
    ID3D12Device* device;
};

#endif // PLATFORM_D3D12_H_
