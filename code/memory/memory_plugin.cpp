#include "memory_plugin.h"
#include "dlmalloc.h"
#include "allocator.h"

#include <stdlib.h>
#include <assert.h>

namespace bx
{
    struct AllocatorDlmalloc : BXIAllocator
    {
        size_t allocated_size = 0;
    };

    static void* DefaultAlloc( BXIAllocator* _this, size_t size, size_t align )
    {
        void* pointer = dlmemalign( align, size );
        size_t usable_size = dlmalloc_usable_size( pointer );

        AllocatorDlmalloc* alloc = (AllocatorDlmalloc*)_this;
        alloc->allocated_size += usable_size;

        return pointer;
    }
    static void DefaultFree( BXIAllocator* _this, void* ptr )
    {
        size_t usable_size = dlmalloc_usable_size( ptr );
        dlfree( ptr );

        AllocatorDlmalloc* alloc = (AllocatorDlmalloc*)_this;
        assert( alloc->allocated_size >= usable_size );
        alloc->allocated_size -= usable_size;
    }
}

static __declspec(align( sizeof(void*))) bx::AllocatorDlmalloc __default_allocator;

extern "C"
{
    MEMORY_PLUGIN_EXPORT void BXMemoryStartUp()
    {
        __default_allocator.Alloc = bx::DefaultAlloc;
        __default_allocator.Free = bx::DefaultFree;
    }

    MEMORY_PLUGIN_EXPORT void BXMemoryShutDown()
    {
        if( __default_allocator.allocated_size != 0 )
        {
            perror( "Memory leak!!" );
            system( "PAUSE" );
        }
    }

    MEMORY_PLUGIN_EXPORT BXIAllocator* BXDefaultAllocator()
    {
        return &__default_allocator;
    }
}//