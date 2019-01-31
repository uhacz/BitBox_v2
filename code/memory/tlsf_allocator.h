#pragma once

#include "dll_interface.h"
#include "allocator.h"
#include "tlsf.h"

#include <mutex>

struct MEMORY_PLUGIN_EXPORT TLSFAllocator : BXIAllocator
{
    static void Create( TLSFAllocator* allocator, void* memory, size_t size );
    static void Destroy( TLSFAllocator* allocator );

    tlsf_t _tlsf;
};

struct MEMORY_PLUGIN_EXPORT TLSFAllocatorThreadSafe : BXIAllocator
{
    static void Create( TLSFAllocatorThreadSafe* allocator, void* memory, size_t size );
    static void Destroy( TLSFAllocatorThreadSafe* allocator );

    tlsf_t _tlsf;
    std::mutex _lock;
};
