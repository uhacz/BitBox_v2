#include "resource_manager.h"
#include <foundation/containers.h>
#include <foundation/hashmap.h>
#include <foundation/id_array.h>
#include <foundation/thread/mutex.h>
#include <foundation/hash.h>
#include <foundation/string_util.h>
#include <filesystem/filesystem_plugin.h>

#include <atomic>
#include "foundation/tag.h"

union RSMResourceHashDecoder
{
    uint64_t hash;
    struct
    {
        uint32_t type;
        uint32_t name;
    };
};

struct RSMPendingResource
{
    id_t id;
    RSMLoader* loader;
};

struct RSM::RSMImpl
{
    static constexpr uint32_t MAX_RESOURCES = 0x2000;
    static constexpr uint32_t MAX_TYPES = 0x40;
    
    mutex_t id_lock;
    id_array_t<MAX_RESOURCES> id_alloc;

    string_t rname[MAX_RESOURCES] = {};
    RSMResourceHash rhash[MAX_RESOURCES] = {};
    RSMResourceID rid[MAX_RESOURCES] = {};
    RSMEState::E rstate[MAX_RESOURCES] = {};
    BXFile rfile[MAX_RESOURCES] = {};
    std::atomic_uint32_t rrefcount[MAX_RESOURCES] = {};

    queue_t<RSMPendingResource> to_load;
    queue_t<RSMPendingResource> to_unload;

    RSMLoader* loader[MAX_TYPES] = {};
    uint32_t loader_supported_type[MAX_TYPES] = {};
    uint32_t nb_loaders = 0;
};



RSMResourceHash RSM::CreateHash( const char* relative_path )
{
    const size_t NAME_SIZE = 256;
    const size_t TYPE_SIZE = 32;
    char name[NAME_SIZE];
    char type[TYPE_SIZE];
    memset( name, 0, sizeof( name ) );
    memset( type, 0, sizeof( type ) );

    char* str = (char*)relative_path;

    str = string::token( str, name, NAME_SIZE - 1, "." );
    if( str )
    {
        string::token( str, type, TYPE_SIZE - 1, " .\n" );
    }

    const uint32_t seed = tag32_t( "RSMR" );

    RSMResourceHashDecoder hash_decoder;
    hash_decoder.type = murmur3_hash32( type, strlen( type ), seed );
    hash_decoder.name = murmur3_hash32( name, strlen( name ), hash_decoder.type );
    
    return { hash_decoder.hash };
}

static RSMLoader* FindLoader( RSM::RSMImpl* impl, RSMResourceHash rhash )
{
    const RSMResourceHashDecoder rhash_decoded = { rhash.h };

    for( uint32_t i = 0; i < impl->nb_loaders; ++i )
    {
        if( rhash_decoded.type == impl->loader_supported_type[i] )
        {
            return impl->loader[i];
        }
    }

    return nullptr;
}

RSMResourceID RSM::Load( const char* relative_path )
{
    RSMResourceID result = { 0 };
    
    const RSMResourceHash rhash = CreateHash( relative_path );
    if( RSMLoader* loader = FindLoader( _rsm, rhash ) )
    {
        id_t id = { 0 };
        {
            scope_mutex_t guard( _rsm->id_lock );
            id = id_array::create( _rsm->id_alloc );
        }
    }

    return result;
}

RSMResourceID RSM::Create( const char* name, const void* data )
{

}

RSMResourceID RSM::Find( const char* relative_path )
{

}

RSMResourceID RSM::Find( RSMResourceHash hash )
{

}

void* RSM::Acquire( RSMResourceID id )
{

}

void RSM::Release( RSMResourceID id )
{

}

RSM* RSM::StartUp( BXIFilesystem* filesystem, BXIAllocator* allocator )
{

}

void RSM::ShutDown( RSM** ptr )
{

}

