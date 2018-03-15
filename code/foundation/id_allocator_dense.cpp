#include "id_allocator_dense.h"
#include "buffer.h"
#include <string.h>
#include <memory/memory.h>

namespace id_allocator
{
    id_allocator_dense_t* create_dense( uint32_t capacity, BXIAllocator* allocator )
    {
        uint32_t mem_size = 0;
        mem_size += sizeof( id_allocator_dense_t );
        mem_size += capacity * sizeof( *id_allocator_dense_t::sparse_id );
        mem_size += capacity * sizeof( *id_allocator_dense_t::sparse_to_dense_index );
        mem_size += capacity * sizeof( *id_allocator_dense_t::dense_to_sparse_index );

        void* memory = BX_MALLOC( allocator, mem_size, sizeof( void* ) );
        memset( memory, 0x00, mem_size );

        BufferChunker chunker( memory, mem_size );
        id_allocator_dense_t* alloc = chunker.Add<id_allocator_dense_t>();
        alloc->sparse_id = chunker.Add<id_t>( capacity );
        alloc->sparse_to_dense_index = chunker.Add<uint16_t>( capacity );
        alloc->dense_to_sparse_index = chunker.Add<uint16_t>( capacity );
        chunker.Check();

        alloc->allocator = allocator;
        alloc->free_index = 0xFFFF;
        alloc->capacity = capacity;
        alloc->size = 0;

        for( uint32_t i = 0; i < capacity; ++i )
            alloc->sparse_id[i].id = 1;

        return alloc;
    }

    void destroy( id_allocator_dense_t** idalloc )
    {
        if( !idalloc[0] )
            return;

        SYS_ASSERT( idalloc[0]->size == 0 );

        BXIAllocator* allocator = idalloc[0]->allocator;
        BX_FREE0( allocator, idalloc[0] );
    }

    id_t alloc( id_allocator_dense_t* a )
    {
        SYS_ASSERT_TXT( a->size < a->capacity, "Object list full" );

        id_t id = makeInvalidHandle<id_t>();

        if( a->free_index != 0xFFFF )
        {
            id.index = a->free_index;
            a->free_index = a->sparse_id[a->free_index].index;
        }
        else
        {
            id.index = a->size;
        }

        id.id = a->sparse_id[id.index].id;
        a->sparse_id[id.index].index = id.index;

        a->sparse_to_dense_index[id.index] = a->size;
        a->dense_to_sparse_index[a->size] = id.index;
        a->size++;

        return id;
    }

    id_t invalidate( id_allocator_dense_t* a, id_t id )
    {
        SYS_ASSERT_TXT( has( a, id ), "IdArray does not have ID: %d,%d", id.id, id.index );
        a->sparse_id[id.index].id += 1;

        return a->sparse_id[id.index];
    }

    id_allocator_dense_t::delete_info_t free( id_allocator_dense_t* a, id_t id )
    {
        SYS_ASSERT_TXT( has( a, id ), "IdArray does not have ID: %d,%d", id.id, id.index );

        a->sparse_id[id.index].id += 1;
        a->sparse_id[id.index].index = a->free_index;
        a->free_index = id.index;

        // Swap with last element
        const uint32_t last = a->size - 1;
        SYS_ASSERT_TXT( last >= a->sparse_to_dense_index[id.index], "Swapping with previous item" );

        id_allocator_dense_t::delete_info_t ret;
        ret.copy_data_from_index = last;
        ret.copy_data_to_index = a->sparse_to_dense_index[id.index];

        // Update tables
        uint16_t std = a->sparse_to_dense_index[id.index];
        uint16_t dts = a->dense_to_sparse_index[last];
        a->sparse_to_dense_index[dts] = std;
        a->dense_to_sparse_index[std] = dts;
        a->size--;

        return ret;
    }

}//