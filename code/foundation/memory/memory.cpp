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

static bx::AllocatorDlmalloc __defaultAllocator;
void BXMemoryStartUp( BXIAllocator** defaultAllocator )
{
    __defaultAllocator.Alloc = bx::DefaultAlloc;
    __defaultAllocator.Free  = bx::DefaultFree;

    defaultAllocator[0] = &__defaultAllocator;
}

void BXMemoryShutDown( BXIAllocator** defaultAllocator )
{
    defaultAllocator[0] = nullptr;
    if( __defaultAllocator.allocated_size != 0 )
    {
        perror( "Memory leak!!" );
        system( "PAUSE" );
    }
}

//extern BXIAllocator* BXDefaultAllocator()
//{
//    return &__defaultAllocator;
//}