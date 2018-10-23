#include "video_d3d12.h"

#define COBJMACROS
#include <initguid.h>
#include <d3d12.h>
#include <dxgi1_6.h>

#include "assert.h"
#include "id_pool.h"
#include "platform_d3d12.h"


#define FRAME_COUNT 2


typedef enum ResourceStatus
{
	RESOURCE_STATUS_INVALID,
	RESOURCE_STATUS_FAILURE,
	RESOURCE_STATUS_VALID,
} ResourceStatus;


typedef struct ResourceBase
{
	ResourceStatus status;
} ResourceBase;

typedef struct Buffer
{
	ResourceBase resource;
    ID3D12Resource* content;
    D3D12_GPU_VIRTUAL_ADDRESS address;
    BufferFormat format;
} Buffer;

typedef struct BackendD3d12
{
    Backend base;
    IdPool buffer_id_pool;
    ID3D12Resource* render_targets[FRAME_COUNT];
    Buffer* buffers;
    ID3D12CommandQueue* command_queue;
    ID3D12Debug* debug_controller;
    ID3D12Device* device;
    ID3D12Fence* fence;
    IDXGISwapChain3* swap_chain;
    ID3D12DescriptorHeap* rtv_heap;
    HANDLE fence_event;
    UINT64 fence_value;
    UINT frame_index;
    UINT rtv_descriptor_size;
} BackendD3d12;


static D3D12_BLEND get_blend_factor(BlendFactor blend_factor)
{
    switch(blend_factor)
    {
        case BLEND_FACTOR_CONSTANT_ALPHA:
        case BLEND_FACTOR_CONSTANT_COLOUR:           return D3D12_BLEND_BLEND_FACTOR;
        case BLEND_FACTOR_DST_ALPHA:                 return D3D12_BLEND_DEST_ALPHA;
        case BLEND_FACTOR_DST_COLOUR:                return D3D12_BLEND_DEST_COLOR;
        case BLEND_FACTOR_ONE:                       return D3D12_BLEND_ONE;
        case BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA:
        case BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOUR: return D3D12_BLEND_INV_BLEND_FACTOR;
        case BLEND_FACTOR_ONE_MINUS_DST_ALPHA:       return D3D12_BLEND_INV_DEST_ALPHA;
        case BLEND_FACTOR_ONE_MINUS_DST_COLOUR:      return D3D12_BLEND_INV_DEST_COLOR;
        case BLEND_FACTOR_ONE_MINUS_SRC_ALPHA:       return D3D12_BLEND_INV_SRC_ALPHA;
        case BLEND_FACTOR_ONE_MINUS_SRC_COLOUR:      return D3D12_BLEND_INV_SRC_COLOR;
        case BLEND_FACTOR_SRC_ALPHA:                 return D3D12_BLEND_INV_SRC_ALPHA;
        case BLEND_FACTOR_SRC_ALPHA_SATURATED:       return D3D12_BLEND_SRC_ALPHA_SAT;
        case BLEND_FACTOR_SRC_COLOUR:                return D3D12_BLEND_SRC_COLOR;
        case BLEND_FACTOR_ZERO:                      return D3D12_BLEND_ZERO;
        default:                                     return 0;
    }
}

static D3D12_BLEND_OP get_blend_op(BlendOp blend_op)
{
    switch(blend_op)
    {
        case BLEND_OP_ADD:              return D3D12_BLEND_OP_ADD;
        case BLEND_OP_REVERSE_SUBTRACT: return D3D12_BLEND_OP_REV_SUBTRACT;
        case BLEND_OP_SUBTRACT:         return D3D12_BLEND_OP_SUBTRACT;
        default:                        return 0;
    }
}

static D3D12_COMPARISON_FUNC get_compare_op(CompareOp compare_op)
{
    switch(compare_op)
    {
        case COMPARE_OP_ALWAYS:           return D3D12_COMPARISON_FUNC_ALWAYS;
        case COMPARE_OP_EQUAL:            return D3D12_COMPARISON_FUNC_EQUAL;
        case COMPARE_OP_GREATER:          return D3D12_COMPARISON_FUNC_GREATER;
        case COMPARE_OP_GREATER_OR_EQUAL: return D3D12_COMPARISON_FUNC_GREATER_EQUAL;
        case COMPARE_OP_LESS:             return D3D12_COMPARISON_FUNC_LESS;
        case COMPARE_OP_LESS_OR_EQUAL:    return D3D12_COMPARISON_FUNC_LESS_EQUAL;
        case COMPARE_OP_NEVER:            return D3D12_COMPARISON_FUNC_NEVER;
        case COMPARE_OP_NOT_EQUAL:        return D3D12_COMPARISON_FUNC_NOT_EQUAL;
        default:                          return 0;
    }
}

static D3D12_CULL_MODE get_cull_mode(CullMode cull_mode)
{
    switch(cull_mode)
    {
        case CULL_MODE_BACK:  return D3D12_CULL_MODE_BACK;
        case CULL_MODE_FRONT: return D3D12_CULL_MODE_FRONT;
        case CULL_MODE_NONE:  return D3D12_CULL_MODE_NONE;
        default:              return 0;
    }
}

static DXGI_FORMAT translate_index_type(IndexType index_type)
{
    switch(index_type)
    {
        case INDEX_TYPE_NONE:   return DXGI_FORMAT_UNKNOWN;
        case INDEX_TYPE_UINT16: return DXGI_FORMAT_R16_UINT;
        case INDEX_TYPE_UINT32: return DXGI_FORMAT_R32_UINT;
        default:                return 0;
    }
}

static D3D12_PRIMITIVE_TOPOLOGY_TYPE translate_primitive_topology(PrimitiveTopology primitive_topology)
{
    switch(primitive_topology)
    {
        case PRIMITIVE_TOPOLOGY_TRIANGLE_LIST: return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        default:                               return 0;
    }
}

static D3D12_STENCIL_OP get_stencil_op(StencilOp stencil_op)
{
    switch(stencil_op)
    {
        case STENCIL_OP_DECREMENT_AND_CLAMP: return D3D12_STENCIL_OP_DECR_SAT;
        case STENCIL_OP_DECREMENT_AND_WRAP:  return D3D12_STENCIL_OP_DECR;
        case STENCIL_OP_INCREMENT_AND_CLAMP: return D3D12_STENCIL_OP_INCR_SAT;
        case STENCIL_OP_INCREMENT_AND_WRAP:  return D3D12_STENCIL_OP_INCR;
        case STENCIL_OP_INVERT:              return D3D12_STENCIL_OP_INVERT;
        case STENCIL_OP_KEEP:                return D3D12_STENCIL_OP_KEEP;
        case STENCIL_OP_REPLACE:             return D3D12_STENCIL_OP_REPLACE;
        case STENCIL_OP_ZERO:                return D3D12_STENCIL_OP_ZERO;
        default:                             return 0;
    }
}

static D3D12_TEXTURE_ADDRESS_MODE get_wrap_parameter(SamplerAddressMode mode)
{
    switch(mode)
    {
        default:
        case SAMPLER_ADDRESS_MODE_REPEAT:          return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        case SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT: return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
        case SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE:   return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    }
}


static void wait_for_prior_frame(BackendD3d12* backend)
{
    UINT64 fence = backend->fence_value;
    HRESULT result = ID3D12CommandQueue_Signal(backend->command_queue,
            backend->fence, fence);
    backend->fence_value += 1;

    if(ID3D12Fence_GetCompletedValue(backend->fence) < fence)
    {
        result = ID3D12Fence_SetEventOnCompletion(backend->fence, fence,
                backend->fence_event);
        WaitForSingleObject(backend->fence_event, INFINITE);
    }

    backend->frame_index = IDXGISwapChain3_GetCurrentBackBufferIndex(
            backend->swap_chain);
}


static Buffer* fetch_buffer(BackendD3d12* backend, BufferId id)
{
    if(id.value != invalid_id)
    {
        uint32_t slot = get_id_slot(id.value);
        ASSERT(slot >= 0 && slot < (uint32_t) backend->buffer_id_pool.cap);
        return &backend->buffers[slot];
    }
    return NULL;
}

static void load_buffer(Buffer* buffer, BufferSpec* spec,
        BackendD3d12* backend)
{
    ASSERT(spec->format != BUFFER_FORMAT_INVALID);
    ASSERT(spec->usage != BUFFER_USAGE_INVALID);
    ASSERT(spec->size > 0);

    D3D12_HEAP_PROPERTIES heap_properties =
    {
        .Type = D3D12_HEAP_TYPE_UPLOAD,
        .CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
        .MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
        .CreationNodeMask = 1,
        .VisibleNodeMask = 1,
    };

    D3D12_RESOURCE_DESC resource_desc =
    {
        .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
        .Alignment = 0,
        .Width = spec->size,
        .Height = 1,
        .DepthOrArraySize = 1,
        .MipLevels = 1,
        .Format = DXGI_FORMAT_UNKNOWN,
        .SampleDesc =
        {
            .Count = 1,
            .Quality = 0,
        },
        .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
        .Flags = D3D12_RESOURCE_FLAG_NONE,
    };

    D3D12_RESOURCE_STATES resource_state = D3D12_RESOURCE_STATE_GENERIC_READ;
    switch(spec->format)
    {
        case BUFFER_FORMAT_INDEX:
        {
            resource_state |= D3D12_RESOURCE_STATE_INDEX_BUFFER;
            break;
        }
        case BUFFER_FORMAT_UNIFORM:
        case BUFFER_FORMAT_VERTEX:
        {
            resource_state |= D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
            break;
        }
    }

    ID3D12Resource* resource;
    HRESULT result = ID3D12Device_CreateCommittedResource(backend->device,
            &heap_properties, D3D12_HEAP_FLAG_NONE, &resource_desc,
            resource_state, NULL, &IID_ID3D12Resource,
            &resource);
    if(FAILED(result))
    {
        buffer->resource.status = RESOURCE_STATUS_FAILURE;
        return;
    }
    buffer->content = resource;

    if(spec->content)
    {
        UINT8* data_begin;
        D3D12_RANGE read_range = {0, 0};
        result = ID3D12Resource_Map(buffer->content, 0, &read_range,
                &data_begin);
        if(FAILED(result))
        {
            buffer->resource.status = RESOURCE_STATUS_FAILURE;
            return;
        }
        copy_memory(data_begin, spec->content, spec->size);
        ID3D12Resource_Unmap(buffer->content, 0, NULL);
    }
    
    switch(spec->format)
    {
        case BUFFER_FORMAT_INDEX:
        case BUFFER_FORMAT_VERTEX:
        {
            buffer->address = ID3D12Resource_GetGPUVirtualAddress(
                    buffer->content);
            break;
        }
        case BUFFER_FORMAT_UNIFORM:
        {
            break;
        }
    }
    
    buffer->resource.status = RESOURCE_STATUS_VALID;
}

static void unload_buffer(Buffer* buffer)
{
    ID3D12Resource_Release(buffer->content);

    buffer->resource.status = RESOURCE_STATUS_INVALID;
}

static bool create_fence(BackendD3d12* backend)
{
    ID3D12Fence* fence;
    HRESULT result = ID3D12Device_CreateFence(backend->device, 0,
            D3D12_FENCE_FLAG_NONE, &IID_ID3D12Fence, &fence);
    if(FAILED(result))
    {
        return false;
    }
    backend->fence = fence;

    HANDLE fence_event = CreateEvent(NULL, FALSE, FALSE, NULL);
    if(!fence_event)
    {
        return false;
    }
    backend->fence_event = fence_event;

    return true;
}

static IDXGIAdapter1* create_adapter(IDXGIFactory4* factory)
{
    IDXGIAdapter1* adapter = NULL;

    for(UINT adapter_index = 0; ; adapter_index += 1)
    {
        HRESULT result = IDXGIFactory4_EnumAdapters1(factory, adapter_index,
                &adapter);
        if(FAILED(result))
        {
            break;
        }

        DXGI_ADAPTER_DESC1 desc = {0};
        IDXGIAdapter1_GetDesc1(adapter, &desc);
        if(desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
        {
            continue;
        }

        result = D3D12CreateDevice((IUnknown*) adapter, D3D_FEATURE_LEVEL_11_0,
                &IID_ID3D12Device, NULL);
        if(SUCCEEDED(result))
        {
            break;
        }
    }

    return adapter;
}

static ID3D12Device* create_device(IDXGIFactory4* factory)
{
    IDXGIAdapter1* adapter = create_adapter(factory);

    ID3D12Device* device = NULL;
    D3D_FEATURE_LEVEL feature_level = D3D_FEATURE_LEVEL_11_0;
    HRESULT result = D3D12CreateDevice((IUnknown*) adapter, feature_level,
            &IID_ID3D12Device, (LPVOID*) &device);
    if(adapter)
    {
        IDXGIAdapter1_Release(adapter);
    }
    if(FAILED(result))
    {
        return NULL;
    }

    return device;
}

static D3D12_CPU_DESCRIPTOR_HANDLE get_cpu_descriptor_handle_for_heap_start(
        ID3D12DescriptorHeap* heap)
{
    D3D12_CPU_DESCRIPTOR_HANDLE handle;
    ((void(__stdcall*)(ID3D12DescriptorHeap*, D3D12_CPU_DESCRIPTOR_HANDLE*))
            heap->lpVtbl->GetCPUDescriptorHandleForHeapStart)(heap, &handle);
    return handle;
}


static void create_backend_d3d12(Backend* backend_base, PlatformVideo* video,
        Heap* heap)
{
    BackendD3d12* backend = (BackendD3d12*) backend_base;

    create_id_pool(&backend->buffer_id_pool, heap, 32);

    backend->buffers = HEAP_ALLOCATE(heap, Buffer, backend->buffer_id_pool.cap);

    PlatformVideoD3d12* platform = (PlatformVideoD3d12*) video;

    UINT dxgi_factory_flags = 0;
    HRESULT result;

#if !defined(NDEBUG)
    ID3D12Debug* debug_controller;
    result = D3D12GetDebugInterface(&IID_ID3D12Debug, &debug_controller);
    if(FAILED(result))
    {
        return;
    }
    backend->debug_controller = debug_controller;
    ID3D12Debug_EnableDebugLayer(debug_controller);
    dxgi_factory_flags |= DXGI_CREATE_FACTORY_DEBUG;
#endif

    IDXGIFactory4* factory;
    result = CreateDXGIFactory2(dxgi_factory_flags, &IID_IDXGIFactory4,
            &factory);
    if(FAILED(result))
    {
        return;
    }

    backend->device = create_device(factory);
    if(!backend->device)
    {
        IDXGIFactory4_Release(factory);
        return;
    }

    D3D12_COMMAND_QUEUE_DESC queue_desc =
    {
        .Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
        .Type = D3D12_COMMAND_LIST_TYPE_DIRECT,
    };
    result = ID3D12Device_CreateCommandQueue(backend->device, &queue_desc,
            &IID_ID3D12CommandQueue, &backend->command_queue);
    if(FAILED(result))
    {
        IDXGIFactory4_Release(factory);
        return;
    }

    DXGI_SWAP_CHAIN_DESC1 swap_chain_desc =
    {
        .BufferCount = FRAME_COUNT,
        .Width = 800,
        .Height = 600,
        .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
        .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
        .SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
        .SampleDesc =
        {
            .Count = 1,
        },
    };
    IDXGISwapChain1* swap_chain;
    result = IDXGIFactory4_CreateSwapChainForHwnd(factory,
            (IUnknown*) backend->command_queue, platform->window,
            &swap_chain_desc, NULL, NULL, &swap_chain);
    if(FAILED(result))
    {
        IDXGIFactory4_Release(factory);
        return;
    }
    backend->swap_chain = (IDXGISwapChain3*) swap_chain;

    result = IDXGIFactory4_MakeWindowAssociation(factory, platform->window,
            DXGI_MWA_NO_ALT_ENTER);
    IDXGIFactory4_Release(factory);
    if(FAILED(result))
    {
        return;
    }

    backend->frame_index = IDXGISwapChain3_GetCurrentBackBufferIndex(
            backend->swap_chain);

    bool fence_created = create_fence(backend);
    if(!fence_created)
    {
        return;
    }

    D3D12_DESCRIPTOR_HEAP_DESC rtv_heap_desc =
    {
        .NumDescriptors = FRAME_COUNT,
        .Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
        .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
    };
    result = ID3D12Device_CreateDescriptorHeap(backend->device, &rtv_heap_desc,
            &IID_ID3D12DescriptorHeap, &backend->rtv_heap);
    if(FAILED(result))
    {
        return;
    }

    backend->rtv_descriptor_size =
            ID3D12Device_GetDescriptorHandleIncrementSize(
                    backend->device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle =
            get_cpu_descriptor_handle_for_heap_start(backend->rtv_heap);

    for(UINT frame = 0; frame < FRAME_COUNT; frame += 1)
    {
        ID3D12Resource* target = backend->render_targets[frame];
        result = IDXGISwapChain1_GetBuffer(backend->swap_chain, frame,
                &IID_ID3D12Resource, &target);
        if(FAILED(result))
        {
            return;
        }
        backend->render_targets[frame] = target;
        ID3D12Device_CreateRenderTargetView(backend->device, target, NULL,
                rtv_handle);
        rtv_handle.ptr += backend->rtv_descriptor_size;
    }
}

static void destroy_backend_d3d12(Backend* backend_base, Heap* heap)
{
    BackendD3d12* backend = (BackendD3d12*) backend_base;

    for(int buffer_index = 0;
            buffer_index < backend->buffer_id_pool.cap;
            buffer_index += 1)
    {
        Buffer* buffer = &backend->buffers[buffer_index];
        if(buffer->resource.status != RESOURCE_STATUS_INVALID)
        {
            unload_buffer(buffer);
        }
    }

    destroy_id_pool(&backend->buffer_id_pool, heap);

    HEAP_DEALLOCATE(heap, backend->buffers);

    if(backend->command_queue
            && backend->fence
            && backend->fence_event)
    {
        wait_for_prior_frame(backend);
    }

    if(backend->command_queue)
    {
        ID3D12CommandQueue_Release(backend->command_queue);
    }
    if(backend->debug_controller)
    {
        ID3D12Debug_Release(backend->debug_controller);
    }
    if(backend->device)
    {
        ID3D12Device_Release(backend->device);
    }
    if(backend->fence)
    {
        ID3D12Fence_Release(backend->fence);
    }
    if(backend->fence_event)
    {
        CloseHandle(backend->fence_event);
    }
    for(int target_index = 0; target_index < FRAME_COUNT; target_index += 1)
    {
        ID3D12Resource* target = backend->render_targets[target_index];
        if(target)
        {
            ID3D12Resource_Release(target);
        }
    }
    if(backend->rtv_heap)
    {
        ID3D12DescriptorHeap_Release(backend->rtv_heap);
    }
    if(backend->swap_chain)
    {
        IDXGISwapChain1_Release(backend->swap_chain);
    }
}

static BufferId create_buffer_d3d12(Backend* backend_base, BufferSpec* spec,
        Log* log)
{
    BackendD3d12* backend = (BackendD3d12*) backend_base;

    IdPool* pool = &backend->buffer_id_pool;
    BufferId id = (BufferId) {allocate_id(pool)};
    if(id.value != invalid_id)
    {
        Buffer* buffer = fetch_buffer(backend, id);
        load_buffer(buffer, spec, backend);
        ASSERT(buffer->resource.status == RESOURCE_STATUS_VALID);
    }
    else
    {
        log_error(log, "The buffer pool is out of memory.");
    }
    return id;
}

static void destroy_buffer_d3d12(Backend* backend_base, BufferId id)
{
    BackendD3d12* backend = (BackendD3d12*) backend_base;
    Buffer* buffer = fetch_buffer(backend, id);
    if(buffer)
    {
        unload_buffer(buffer);
        deallocate_id(&backend->buffer_id_pool, id.value);
        ASSERT(buffer->resource.status == RESOURCE_STATUS_INVALID);
    }
}

static void swap_buffers_d3d12(Backend* backend_base, PlatformVideo* video)
{
    BackendD3d12* backend = (BackendD3d12*) backend_base;
    HRESULT result = IDXGISwapChain_Present(backend->swap_chain, 1, 0);
    ASSERT(SUCCEEDED(result));
    wait_for_prior_frame(backend);
    platform_video_swap_buffers(video);
}


Backend* set_up_backend_d3d12(Heap* heap)
{
    BackendD3d12* backend_d3d12 = HEAP_ALLOCATE(heap, BackendD3d12, 1);
    Backend* backend = &backend_d3d12->base;

    backend->create_backend = create_backend_d3d12;
    backend->destroy_backend = destroy_backend_d3d12;

    backend->create_buffer = create_buffer_d3d12;
    backend->destroy_buffer = destroy_buffer_d3d12;
    backend->swap_buffers = swap_buffers_d3d12;

    return backend;
}
