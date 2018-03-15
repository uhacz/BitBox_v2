#pragma once

struct BXIAllocator
{
    void* ( *Alloc )( BXIAllocator* _this, size_t size, size_t align );
    void  ( *Free )( BXIAllocator* _this, void* ptr );
};


