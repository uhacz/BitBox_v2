#pragma once

#include <foundation/type_compound.h>
#include <foundation/containers.h>
#include <rdi_backend/rdi_backend_type.h>

#include <atomic>

struct RDIDevice;
struct RDICommandQueue;
struct RDIXRenderSource;

static constexpr uint32_t GFX_DEFAULT_SKINNING_PIN = UINT32_MAX;


struct LocklessLinearStaticAllocator
{
    void Initialize( void* data, size_t size );
    void Uninitialize();
    bool IsValid() const { return !(_begin == _end); }

    void* Allocate( size_t num_bytes );
    uint64_t Clear();

    uint64_t Offset() const { return std::atomic_load( &_offset ); }
    void* Begin() { return (void*)(_begin); }
    void* End() { return (void*)(_end); }

    uint64_t _begin = 0;
    uint64_t _end = 0;
    std::atomic<uint64_t> _offset{ 0 };
};

struct GFXMeshSkinningData
{
    RDIXRenderSource* rsource = nullptr;
    uint32_t pin_index = GFX_DEFAULT_SKINNING_PIN;
    uint32_t num_elements : 31;
    uint32_t flag_gpu : 1;
};

struct GFXSkinningPin
{
    using element_t = float4_t;

    element_t* address;
    uint32_t  index; // in element_t unit
    uint32_t  num_elements;
};
struct GFXSkinningDataCPU
{
    using element_t = float4_t;
    static constexpr uint32_t NUM_BUFFERS = 2;

    BXIAllocator* _main_allocator = nullptr;
    
    element_t* _buffer[NUM_BUFFERS] = {};
    uint64_t   _back_buffer_offset = 0;
    uint32_t   _mapped_buffer_index = 0;
    uint32_t   _max_num_elements = 0;
    
    LocklessLinearStaticAllocator _mapped_allocator = {};

    void Initialize( BXIAllocator* allocator, uint32_t capacity );
    void Uninitialize();

    void Swap();

    GFXSkinningPin AcquirePin( uint32_t size_in_bytes );

    template< typename T >
    array_span_t< const T> BackBuffer() const
    {
        SYS_ASSERT( _back_buffer_offset < UINT32_MAX );
        const uint32_t size_in_bytes = (uint32_t)_back_buffer_offset * sizeof( element_t );
        SYS_ASSERT( (size_in_bytes % sizeof( T )) == 0 );

        return array_span_t<const T>( (const T*)_buffer[!_mapped_buffer_index], size_in_bytes / sizeof( T ) );
    }

};

struct GFXSkinningDataGPU
{
    using element_t = float4_t;
    static constexpr uint32_t NUM_BUFFERS = 2;

    RDIConstantBuffer _gpu_cbuffer_data        = {};
    RDIBufferRO       _gpu_buffer[NUM_BUFFERS] = {};
    uint64_t          _back_buffer_offset      = 0;
    uint32_t          _max_num_elements        = 0;
    uint32_t          _mapped_buffer_index     = 0;

    LocklessLinearStaticAllocator _mapped_allocator = {};

    void Initialize( RDIDevice* dev, uint32_t capacity );
    void Uninitialize( RDIDevice* dev );

    void Swap( RDICommandQueue* cmdq );
    void Bind( RDICommandQueue* cmdq );
       
    GFXSkinningPin AcquirePin( uint32_t size_in_bytes );
};
