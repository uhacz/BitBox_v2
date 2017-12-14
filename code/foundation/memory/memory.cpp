#include "memory.h"
#include "dlmalloc.h"
#include "allocator.h"

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
        alloc->allocated_size -= usable_size;
    }

    


}

BXIAllocator* __default_allocator = nullptr;
static bx::AllocatorDlmalloc __main_allocator;
void BXMemoryStartUp( BXIAllocator** defaultAllocator )
{
    __main_allocator.Alloc = bx::DefaultAlloc;
    __main_allocator.Free  = bx::DefaultFree;

    __default_allocator = &__main_allocator;
    defaultAllocator[0] = __default_allocator;
}

void BXMemoryShutDown( BXIAllocator** defaultAllocator )
{
    defaultAllocator[0] = nullptr;
    if( __main_allocator.allocated_size != 0 )
    {
        perror( "Memory leak!!" );
        system( "PAUSE" );
    }
}

void BXDLLSetMemoryHook( BXIAllocator * allocator )
{
    __default_allocator = allocator;
}

BXIAllocator* BXDefaultAllocator()
{
    return __default_allocator;
}