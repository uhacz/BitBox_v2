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

struct RSM_EXPORT RSMLoader
{
    virtual ~RSMLoader() {}

    virtual const char* SupportedType() const = 0;
    virtual bool Load( const void* data, uint32_t size ) = 0;
    virtual void Unload( const void* data, uint32_t size ) = 0;
};

struct RSM_EXPORT RSM
{
    static RSMResourceHash CreateHash( const char* relative_path );

    RSMResourceID Load( const char* relative_path );
    RSMResourceID Create( const char* name, const void* data );

    RSMResourceID Find( const char* relative_path );
    RSMResourceID Find( RSMResourceHash hash );

    void* Acquire( RSMResourceID id );
    void  Release( RSMResourceID id );


    // --- private
    static RSM* StartUp( BXIFilesystem* filesystem, BXIAllocator* allocator );
    static void ShutDown( RSM** ptr );

    struct RSMImpl;
    RSMImpl* _rsm = nullptr;
};

