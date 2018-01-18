#pragma once

#include "type.h"
#include "buffer.h"

#define container_soa_add_stream( desc, type, field )\
    desc.add( offsetof( type, field ), sizeof( *type::field ), ALIGNOF( decltype(*type::field ) ) )


struct BXIAllocator;

struct container_soa_desc_t
{
    static constexpr uint32_t MAX_STREAMS = 32;

    uint32_t offset[MAX_STREAMS] = {};
    uint32_t stride[MAX_STREAMS] = {};
    uint32_t align[MAX_STREAMS] = {};
    uint32_t num_streams = 0;

    void add( uint32_t field_offset, uint32_t element_stride, uint32_t alignment )
    {
        uint32_t index = num_streams++;
        offset[index] = field_offset;
        stride[index] = element_stride;
        align[index] = alignment;
    }
};

namespace container_soa
{
    struct header_t
    {
        uint32_t size = 0;
        uint32_t capacity = 0;
        uint32_t num_streams = 0;

        BXIAllocator* allocator = nullptr;
        uint16_t* offset = nullptr;
        uint16_t* stride = nullptr;
    };

    inline header_t* _header( void* cont ) { return (header_t*)cont - 1; }

    
    inline bool     has( void* cont, uint32_t index ) { return index < _header( cont )->size; }
    inline uint32_t size( void* cont ) { return _header( cont )->size; }
    inline uint32_t capacity( void* cont ) { return _header( cont )->capacity; }
    
    void     resize( void* cont, uint32_t new_size );
    uint32_t push_back( void* cont );
    uint32_t remove_packed( void* cont, uint32_t index );


    template< typename T >
    void create( T** out, const container_soa_desc_t& desc, uint32_t capacity, BXIAllocator* allocator )
    {
        uint32_t mem_size = sizeof( header_t );
        mem_size += sizeof( T );
        mem_size += desc.num_streams * sizeof( *header_t::offset );
        mem_size += desc.num_streams * sizeof( *header_t::stride );
        for( uint32_t i = 0; i < desc.num_streams; ++i )
        {
            mem_size += capacity * desc.stride[i];
            mem_size = TYPE_ALIGN( mem_size, desc.align[i] );
        }

        void* memory = BX_MALLOC( allocator, mem_size, 16 );
        memset( memory, 0x00, mem_size );

        BufferChunker chunker( memory, mem_size );
        header_t* header = chunker.Add<header_t>();
        T* object = chunker.Add<T>();

        header->offset = chunker.Add<uint16_t>( desc.num_streams, __alignof(uint16_t) );
        header->stride = chunker.Add<uint16_t>( desc.num_streams, __alignof(uint16_t) );


        for( uint32_t i = 0; i < desc.num_streams; ++i )
        {
            header->offset[i] = desc.offset[i];
            header->stride[i] = desc.stride[i];

            uint8_t* data_ptr = chunker.AddBlock( capacity * desc.stride[i], desc.align[i] );
            memcpy( (uint8_t*)object + desc.offset[i], &data_ptr, sizeof( void* ) );
        }

        chunker.Check();

        header->size = 0;
        header->capacity = capacity;
        header->num_streams = desc.num_streams;
        header->allocator = allocator;

        out[0] = object;
    }

    template< typename T >
    void destroy( T** cont )
    {
        if( !cont[0] )
            return;

        T* c = cont[0];
        header_t* header = (header_t*)c - 1;
        BXIAllocator* allocator = header->allocator;

        BX_FREE0( allocator, header );
        cont[0] = nullptr;
    }
}
