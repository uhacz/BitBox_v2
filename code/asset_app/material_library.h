#pragma once

#include <foundation/type.h>
#include <foundation/string_util.h>
#include <foundation/containers.h>

struct BXIAllocator;
struct BXIFilesystem;
struct GFXMaterialID;

struct MATLibrary
{
    MATLibrary();

    void Load( const char* filename, BXIFilesystem* filesystem, BXIAllocator* allocator );
    void Unload();
    void Reload( BXIFilesystem* filesystem, BXIAllocator* allocator );
    void Tick();

    // ---
    array_t<GFXMaterialID> _materials;
    string_t _filename;
    BXIAllocator* _allocator = nullptr;
};
