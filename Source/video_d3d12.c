#include "video_d3d12.h"

#define COBJMACROS
#include <initguid.h>
#include <d3d12.h>
#include <dxgi1_6.h>

#include "assert.h"
#include "id_pool.h"


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
    ID3D12Device* device;
    Buffer* buffers;
    IdPool buffer_id_pool;
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
    switch (index_type)
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


static void create_backend_d3d12(Backend* backend_base, Heap* heap)
{
    BackendD3d12* backend = (BackendD3d12*) backend_base;

    create_id_pool(&backend->buffer_id_pool, heap, 32);

    backend->buffers = HEAP_ALLOCATE(heap, Buffer, backend->buffer_id_pool.cap);
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


Backend* setup_backend_d3d12(Heap* heap)
{
    BackendD3d12* backend_d3d12 = HEAP_ALLOCATE(heap, BackendD3d12, 1);
    Backend* backend = &backend_d3d12->base;

    backend->create_backend = create_backend_d3d12;
    backend->destroy_backend = destroy_backend_d3d12;

    backend->create_buffer = create_buffer_d3d12;
    backend->destroy_buffer = destroy_buffer_d3d12;

    return backend;
}
