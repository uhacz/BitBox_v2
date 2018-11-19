#pragma once

#include "dll_interface.h"
#include "allocator.h"
#include "pool.h"

struct MEMORY_PLUGIN_EXPORT PoolAllocator : BXIAllocator
{
    static void Create( PoolAllocator* allocator, void* memory, size_t size, size_t chunk_size );
    static void Destroy( PoolAllocator* allocator );

    pool_t _pool;
};

struct MEMORY_PLUGIN_EXPORT DynamicPoolAllocator : BXIAllocator
{
    static void Create( DynamicPoolAllocator* allocator, BXIAllocator* backent_allocator, size_t chunk_size, size_t alignment, size_t num_chunks_per_pool );
    static void Destroy( DynamicPoolAllocator* allocator );

    dynamic_pool_t _pool;
};