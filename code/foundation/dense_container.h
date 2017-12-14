#pragma once

#include "containers.h"

namespace dense_container
{
    template< class Cnt >
    id_t create( Cnt& cnt )
    {
        id_t id = id_array::create( cnt._id );
        return id;
    }

    template< class Cnt, size_t STREAM_INDEX>
    struct swap_helper
    {
        static void swap( Cnt& cnt, uint32_t to, uint32_t from )
        {
            auto stream = cnt.stream<STREAM_INDEX>();
            stream[to] = stream[from];

            swap_helper<Cnt, STREAM_INDEX - 1>::swap( cnt, to, from );
        }
    };
    template< class Cnt >
    struct swap_helper<Cnt, 0>
    {
        static void swap( Cnt& cnt, uint32_t to, uint32_t from )
        {
            auto stream = cnt.stream<0>();
            stream[to] = stream[from];
        }
    };

    template< class Cnt >
    void destroy( Cnt& cnt, id_t id )
    {
        if( !id_array::has( cnt._id, id ) )
            return;

        const id_array_destroy_info_t copy_info = id_array::destroy( cnt._id, id );
        swap_helper<Cnt, cnt.NUM_STREAMS - 1>::swap( cnt, copy_info.copy_data_to_index, copy_info.copy_data_from_index );
    }
}