#include "gfx_skinning.h"
#include <rdi_backend/rdi_backend.h>

static inline RDIBufferRO GetMappedBuffer( const GFXSkinningDataGPU& data )
{
    return data._gpu_buffer[data._mapped_buffer_index];
}
static inline RDIBufferRO GetBackBuffer( const GFXSkinningDataGPU& data )
{
    return data._gpu_buffer[!data._mapped_buffer_index];
}

template< typename TElement >
static inline GFXSkinningPin AcquirePin( LocklessLinearStaticAllocator* allocator, uint32_t size_in_bytes )
{
    SYS_ASSERT( (size_in_bytes % sizeof( TElement )) == 0 );

    GFXSkinningPin pin;

    if( TElement* allocated_data = (TElement*)allocator->Allocate( size_in_bytes ) )
    {
        pin.address = allocated_data;

        ptrdiff_t index = (uintptr_t)allocated_data - (uintptr_t)allocator->Begin();
        SYS_ASSERT( index < UINT32_MAX );
        pin.index = (uint32_t)index;
        pin.num_elements = size_in_bytes / sizeof( TElement );
    }

    return pin;
}

#include "gfx_shader_interop.h"

void GFXSkinningDataGPU::Initialize( RDIDevice* dev, uint32_t capacity )
{
    SYS_ASSERT( (capacity % sizeof( element_t )) == 0 );
    _max_num_elements = capacity / sizeof( element_t );

    _gpu_cbuffer_data = CreateConstantBuffer( dev, sizeof( gfx_shader::SkinningData ) );

    for( uint32_t i = 0; i < NUM_BUFFERS; ++i )
    {
        _gpu_buffer[i] = CreateBufferRO( dev, _max_num_elements, RDIFormat::Float4(), RDIECpuAccess::WRITE );
    }

    Swap( GetImmediateCommandQueue( dev ) );
}

static void Unmap( RDICommandQueue* cmdq, GFXSkinningDataGPU* data )
{
    if( data->_mapped_allocator.IsValid() )
    {
        RDIBufferRO buff = GetMappedBuffer( *data);
        Unmap( cmdq, buff );
        data->_mapped_allocator.Uninitialize();
    }
}

void GFXSkinningDataGPU::Uninitialize( RDIDevice* dev )
{
    RDICommandQueue* cmdq = GetImmediateCommandQueue( dev );
    Unmap( cmdq, this );

    for( uint32_t i = 0; i < NUM_BUFFERS; ++i )
    {
        Destroy( &_gpu_buffer[i] );
    }

    Destroy( &_gpu_cbuffer_data );
}

void GFXSkinningDataGPU::Swap( RDICommandQueue* cmdq )
{
    Unmap( cmdq, this );

    _mapped_buffer_index = !_mapped_buffer_index;

    RDIBufferRO buff = GetMappedBuffer( *this );
    void* mappded_data = Map( cmdq, buff, 0, RDIEMapType::WRITE );
    _mapped_allocator.Initialize( mappded_data, _max_num_elements * sizeof( element_t ) );
}

void GFXSkinningDataGPU::Bind( RDICommandQueue* cmdq )
{
    RDIBufferRO buff = GetBackBuffer( *this );
    SetResourcesRO( cmdq, &buff, SKINNING_INPUT_MATRICES_SLOT, 1, RDIEPipeline::COMPUTE_MASK );
    SetCbuffers( cmdq, &_gpu_cbuffer_data, SKINNING_CBUFFER_SLOT, 1, RDIEPipeline::COMPUTE_MASK );
}

GFXSkinningPin GFXSkinningDataGPU::AcquirePin( uint32_t size_in_bytes )
{
    return ::AcquirePin<element_t>( &_mapped_allocator, size_in_bytes );
}


//
// CPU
//
void GFXSkinningDataCPU::Initialize( BXIAllocator* allocator, uint32_t capacity )
{
    SYS_ASSERT( (capacity % sizeof( element_t )) == 0 );
    SYS_ASSERT( allocator != nullptr );

    _main_allocator = allocator;
    _max_num_elements = capacity / sizeof( element_t );
       
    for( uint32_t i = 0; i < NUM_BUFFERS; ++i )
    {
        _buffer[i] = (element_t*)BX_MALLOC( allocator, capacity, 16 );
    }

    _back_buffer_offset = 0;
    _mapped_buffer_index = 1;
    Swap();
}

void GFXSkinningDataCPU::Uninitialize()
{
    for( uint32_t i = 0; i < NUM_BUFFERS; ++i )
    {
        BX_FREE0( _main_allocator, _buffer[i] );
    }

    _main_allocator = nullptr;
    _max_num_elements = 0;
    _back_buffer_offset = 0;
    _mapped_buffer_index = 0;
    _mapped_allocator.Uninitialize();
}

void GFXSkinningDataCPU::Swap()
{
    _back_buffer_offset = _mapped_allocator.Clear();
    _mapped_buffer_index = !_mapped_buffer_index;
    _mapped_allocator.Initialize( _buffer[_mapped_buffer_index], _max_num_elements * sizeof( element_t ) );
}


GFXSkinningPin GFXSkinningDataCPU::AcquirePin( uint32_t size_in_bytes )
{
    return ::AcquirePin<element_t>( &_mapped_allocator, size_in_bytes );
}

// allocator
void LocklessLinearStaticAllocator::Initialize( void* data, size_t size )
{
    _begin = (uint64_t)data;
    _end = _begin + size;
    _offset = 0;
}

void LocklessLinearStaticAllocator::Uninitialize()
{
    _begin = _end = 0;
    _offset = 0;
}

void* LocklessLinearStaticAllocator::Allocate( size_t num_bytes )
{
    const uint64_t offset = std::atomic_fetch_add( &_offset, num_bytes );
    if( ( _begin + offset + num_bytes ) > _end )
    {
        return nullptr;
    }
    return (void*)(_begin + offset);
}

uint64_t LocklessLinearStaticAllocator::Clear()
{
    return std::atomic_exchange( &_offset, 0 );
}
