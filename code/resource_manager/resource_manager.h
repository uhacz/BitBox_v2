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
inline bool IsAlive( RSMResourceID id ) { return id.i != 0; }

struct RSM_EXPORT RSM
{
    static RSMResourceHash CreateHash( const char* relative_path );

    RSMResourceID Load( const char* relative_path );
    RSMResourceID Create( const char* name, const void* data, BXIAllocator* data_allocator );
    RSMEState::E  Wait( RSMResourceID rid );

    RSMResourceID Find( const char* relative_path ) const;
    RSMResourceID Find( RSMResourceHash hash ) const;

    const void* Get( RSMResourceID id ) const;
    void  Release( RSMResourceID id );

    const void* Acquire( const char* relative_path ) const
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


