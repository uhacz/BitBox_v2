#pragma once

#include "containers.h"
#include <foundation/hash.h>
#include <string.h>

namespace dense_container
{
    template< typename T >
    inline uint32_t GenerateHashedName( const char* name )
    {
        const uint32_t name_len = (uint32_t)strlen( name );
        return murmur3_32x86_hash( name, name_len, name_len * sizeof( T ) );
    }
}//
struct dense_container_desc_t
{
    uint8_t strides[dense_container::MAX_STREAMS] = {};
    uint32_t names_hash[dense_container::MAX_STREAMS] = {};
    uint32_t num_streams = 0;

    template< typename T >
    void add_stream( const char* name )
    {
        SYS_ASSERT( num_streams < dense_container::MAX_STREAMS );
        const uint32_t index = num_streams++;
        strides[index] = sizeof( T );

        names_hash[index] = dense_container::GenerateHashedName<T>( name );
    }
};

namespace dense_container
{
    template< uint32_t MAX >
    dense_container_t<MAX>* create( const dense_container_desc_t& desc, BXIAllocator* allocator )
    {
        using container_t = dense_container_t<MAX>;
        uint32_t mem_size = 0;
        mem_size += sizeof( container_t );
        for( uint32_t i = 0; i < desc.num_streams; ++i )
            mem_size += desc.strides[i] * MAX;

        void* mem = BX_MALLOC( allocator, mem_size, 16 );
        memset( mem, 0x00, mem_size );

        container_t* c = new(mem) container_t();
        c->_allocator = allocator;
        uint8_t* data_begin = (uint8_t*)(c + 1);

        c->_num_streams = desc.num_streams;
        for( uint32_t i = 0; i < desc.num_streams; ++i )
        {
            c->_streams_stride[i] = desc.strides[i];
            c->_names_hash[i] = desc.names_hash[i];
            c->_streams[i] = data_begin;

            data_begin += MAX * desc.strides[i];
        }

        SYS_ASSERT( data_begin == (uint8_t*)mem + mem_size );

        return c;

    }

    template< uint32_t MAX >
    void destroy( dense_container_t<MAX>** cnt )
    {
        if( !*cnt )
            return;

        BXIAllocator* allocator = cnt[0]->_allocator;
        BX_FREE0( allocator, cnt[0] );
    }

    template< uint32_t MAX >
    id_t create( dense_container_t<MAX>* cnt )
    {
        id_t id = id_array::create( cnt->_ids );
        return id;
    }

    template< uint32_t MAX >
    void destroy( dense_container_t<MAX>* cnt, id_t id )
    {
        if( !id_array::has( cnt->_ids, id ) )
            return;

        const id_array_destroy_info_t copy_info = id_array::destroy( cnt->_ids, id );
        for( uint32_t i = 0; i < cnt->_num_streams; ++i )
        {
            const uint8_t stride = cnt->_streams_stride[i];
            const uint8_t* src = cnt->_streams[i] + copy_info.copy_data_from_index * stride;
            uint8_t* dst = cnt->_streams[i] + copy_info.copy_data_to_index * stride;
            memcpy( dst, src, stride );
        }
    }

    //template< class Cnt >
    //id_t create( Cnt& cnt )
    //{
    //    id_t id = id_array::create( cnt._id );
    //    return id;
    //}

    //template< class Cnt, size_t STREAM_INDEX>
    //struct swap_helper
    //{
    //    static void swap( Cnt& cnt, uint32_t to, uint32_t from )
    //    {
    //        auto stream = cnt.stream<STREAM_INDEX>();
    //        stream[to] = stream[from];

    //        swap_helper<Cnt, STREAM_INDEX - 1>::swap( cnt, to, from );
    //    }
    //};
    //template< class Cnt >
    //struct swap_helper<Cnt, 0>
    //{
    //    static void swap( Cnt& cnt, uint32_t to, uint32_t from )
    //    {
    //        auto stream = cnt.stream<0>();
    //        stream[to] = stream[from];
    //    }
    //};

    //template< class Cnt >
    //void destroy( Cnt& cnt, id_t id )
    //{
    //    if( !id_array::has( cnt._id, id ) )
    //        return;

    //    const id_array_destroy_info_t copy_info = id_array::destroy( cnt._id, id );
    //    swap_helper<Cnt, cnt.NUM_STREAMS - 1>::swap( cnt, copy_info.copy_data_to_index, copy_info.copy_data_from_index );
    //}
}