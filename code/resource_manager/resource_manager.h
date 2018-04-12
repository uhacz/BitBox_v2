#pragma once

#include "dll_interface.h"
#include <foundation/type.h>

struct BXIFilesystem;
struct BXIAllocator;

namespace RSMEState
{
    enum E : uint32_t
    {
        UNLOADED = 0,
        LOADING,
        READY,
        UNLOADING,
        FAIL,
    };
}//

struct RSMResourceHash
{
    uint64_t h;
};
struct RSMResourceID
{
    uint32_t i;
};

struct RSMResourceData
{
    void* pointer = nullptr;
    size_t size = 0;
    BXIAllocator* allocator = nullptr;
};
inline bool IsAlive( RSMResourceID id ) { return id.i != 0; }

struct RSM_EXPORT RSMLoader
{
    virtual ~RSMLoader() {}

    virtual const char* SupportedType() const = 0;
    virtual bool IsBinary() const = 0;
    virtual bool Load( RSMResourceData* out, const void* data, uint32_t size, BXIAllocator* allocator ) = 0;
    virtual void Unload( RSMResourceData* in_out ) = 0;
};

struct RSM_EXPORT RSM
{
    static RSMResourceHash CreateHash( const char* relative_path );

    RSMResourceID Load( const char* relative_path );
    RSMResourceID Create( const char* name, const void* data );
    RSMEState::E  Wait( RSMResourceID rid );

    RSMResourceID Find( const char* relative_path );
    RSMResourceID Find( RSMResourceHash hash );

    void* Get( RSMResourceID id );
    void  Release( RSMResourceID id );

    void* Acquire( const char* relative_path )
    {
        RSMResourceID rid = Find( relative_path );
        return IsAlive( rid ) ? Get( rid ) : nullptr;
    }


    // --- private
    static RSM* StartUp( BXIFilesystem* filesystem, BXIAllocator* allocator );
    static void ShutDown( RSM** ptr );

    struct RSMImpl;
    RSMImpl* _rsm;
};


