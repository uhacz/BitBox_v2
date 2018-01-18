#pragma once

#include "containers.h"

namespace id_allocator
{
    id_allocator_dense_t* create_dense( uint32_t capacity, BXIAllocator* allocator );
    void destroy( id_allocator_dense_t** idalloc );
    id_t alloc( id_allocator_dense_t* a );
    id_t invalidate( id_allocator_dense_t* a, id_t id );
    id_allocator_dense_t::delete_info_t free( id_allocator_dense_t* a, id_t id );

    inline bool has( id_allocator_dense_t* a, id_t id )
    {
        return id.index < a->capacity && a->sparse_id[id.index].id == id.id;
    }
    inline uint16_t dense_index( id_allocator_dense_t* a, id_t id )
    {
        SYS_ASSERT_TXT( has( a, id ), "IdArray does not have ID: %d,%d", id.id, id.index );
        return (int)a->sparse_to_dense_index[id.index];
    }
}