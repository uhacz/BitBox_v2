#pragma once

#include "dll_interface.h"
#include <foundation/type.h>

struct BXIAllocator;

struct RSMResourceData
{
    const void* pointer = nullptr;
    size_t size = 0;
    BXIAllocator* allocator = nullptr;
};

struct RSM_EXPORT RSMLoader
{
    virtual ~RSMLoader() {}

    virtual const char* SupportedType() const = 0;
    virtual bool IsBinary() const = 0;
    virtual bool Load( RSMResourceData* out, const void* data, uint32_t size, BXIAllocator* allocator );
    virtual void Unload( RSMResourceData* in_out );
};

