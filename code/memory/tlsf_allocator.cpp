#include "tlsf_allocator.h"

static void* TLFSAlloc( BXIAllocator* _this, size_t size, size_t align )
{
    TLSFAllocator* allocator = (TLSFAllocator*)_this;
    void* ptr = tlsf_memalign( allocator->_tlsf, align, size );
#if _DEBUG
    if( !ptr )
    {
        // OOM
        __debugbreak();
    }
#endif
    return ptr;
}
static void TLSFFree( BXIAllocator* _this, void* ptr )
{
    TLSFAllocator* allocator = (TLSFAllocator*)_this;
    tlsf_free( allocator->_tlsf, ptr );
}

void TLSFAllocator::Create( TLSFAllocator* allocator, void* memory, size_t size )
{
    allocator->_tlsf = tlsf_create_with_pool( memory, size );
    allocator->Alloc = TLFSAlloc;
    allocator->Free = TLSFFree;
}

void TLSFAllocator::Destroy( TLSFAllocator* allocator )
{
    tlsf_destroy( allocator->_tlsf );
}

// --- 
static void* TLFSAllocWithLock( BXIAllocator* _this, size_t size, size_t align )
{
    TLSFAllocatorThreadSafe* allocator = (TLSFAllocatorThreadSafe*)_this;
    scope_lock_t<mutex_t> guard( allocator->_lock );
    return tlsf_memalign( allocator->_tlsf, align, size );
}
static void TLSFFreeWithLock( BXIAllocator* _this, void* ptr )
{
    TLSFAllocatorThreadSafe* allocator = (TLSFAllocatorThreadSafe*)_this;
    scope_lock_t<mutex_t> guard( allocator->_lock );
    tlsf_free( allocator->_tlsf, ptr );
}

void TLSFAllocatorThreadSafe::Create( TLSFAllocatorThreadSafe* allocator, void* memory, size_t size )
{
    allocator->_tlsf = tlsf_create_with_pool( memory, size );
    allocator->Alloc = TLFSAllocWithLock;
    allocator->Free = TLSFFreeWithLock;
}

void TLSFAllocatorThreadSafe::Destroy( TLSFAllocatorThreadSafe* allocator )
{
    tlsf_destroy( allocator->_tlsf );
}
