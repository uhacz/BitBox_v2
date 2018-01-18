#include "container_soa.h"
#include <string.h>

namespace container_soa
{

    void resize( void* cont, uint32_t new_size )
    {
        header_t* h = _header( cont );
        h->size = (new_size <= h->capacity) ? new_size : h->capacity;
    }

    uint32_t push_back( void* cont )
    {
        header_t* h = _header( cont );
        if( h->size >= h->capacity )
            return uint32_t( -1 );

        return h->size++;
    }

    uint32_t remove_packed( void* cont, uint32_t index )
    {
        header_t* h = _header( cont );
        SYS_ASSERT( h->size > 0 );
        if( index >= h->size )
            return uint32_t( -1 );

        uint32_t last_index = --h->size;
        if( last_index != index )
        {
            const uint32_t n = h->num_streams;
            for( uint32_t i = 0; i < n; ++i )
            {
                const uint32_t stride = h->stride[i];
                const uint32_t offset = h->offset[i];

                uint8_t* stream_data = nullptr;
                memcpy( &stream_data, (uint8_t*)cont + offset, sizeof( void* ) );
                memcpy( stream_data + index * stride, stream_data + last_index * stride, stride );
            }
        }

        return last_index;
    }

}//