#pragma once

#include "allocator.h"
#include "tlsf.h"

#include <foundation\thread\mutex.h>

struct TLSFAllocator : BXIAllocator
{
    static void Create( TLSFAllocator* allocator, void* memory, size_t size );
    static void Destroy( TLSFAllocator* allocator );

    tlsf_t _tlsf;
};

struct TLSFAllocatorThreadSafe : BXIAllocator
{
    static void Create( TLSFAllocatorThreadSafe* allocator, void* memory, size_t size );
    static void Destroy( TLSFAllocatorThreadSafe* allocator );

    tlsf_t _tlsf;
    mutex_t _lock;
};
