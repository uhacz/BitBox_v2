#include "resource_manager.h"
#include <foundation/containers.h>
#include <foundation/hashmap.h>
#include <foundation/queue.h>
#include <foundation/id_table.h>

#include <foundation/hash.h>
#include <foundation/string_util.h>
#include <filesystem/filesystem_plugin.h>

#include <foundation/thread/mutex.h>
#include <foundation/thread/semaphore.h>

#include <atomic>
#include "foundation/tag.h"
#include "foundation/debug.h"

#define RSM_LOG_STATUS 1

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
    BXFileHandle hfile;
    RSMLoader* loader;
};

template< typename T, typename Tlock >
static inline bool PopFrontQueue( T* out, queue_t<T>& q, Tlock& lock )
{
    lock.lock();
    const bool empty = queue::empty( q );
    if( !empty )
    {
        out[0] = queue::front( q );
        queue::pop_front( q );
    }
    lock.unlock();

    return !empty;
}


struct RSM::RSMImpl
{
    static constexpr uint32_t MAX_RESOURCES = 0x2000;
    static constexpr uint32_t MAX_TYPES = 0x40;
    
    mutex_t id_lock;
    id_table_t<MAX_RESOURCES> id_alloc;

    string_t        rname[MAX_RESOURCES] = {};
    RSMResourceHash rhash[MAX_RESOURCES] = {};
    id_t            rid[MAX_RESOURCES] = {};
    RSMEState::E    rstate[MAX_RESOURCES] = {};
    RSMResourceData rdata[MAX_RESOURCES] = {};
    std::atomic_uint32_t rrefcount[MAX_RESOURCES] = {};

    mutex_t lookup_lock;
    hash_t<id_t> lookup;

    mutex_t to_load_lock;
    mutex_t to_unload_lock;

    queue_t<RSMPendingResource> to_load;
    queue_t<RSMPendingResource> to_unload;

    RSMLoader* loader[MAX_TYPES] = {};
    uint32_t loader_supported_type[MAX_TYPES] = {};
    uint32_t nb_loaders = 0;
    
    BXIFilesystem* filesystem = nullptr;
    BXIAllocator* string_allocator = nullptr;
    BXIAllocator* default_resource_allocator = nullptr;

    std::thread background_thread;
    semaphore_t sema;
    std::atomic_uint32_t is_running = 0;

    bool IsAlive( id_t id ) const { return id_table::has( id_alloc, id ); }
};

static void BackgroundThread( RSM::RSMImpl* rsm )
{
    while( rsm->is_running )
    {
        rsm->sema.wait();

        RSMPendingResource pending = {};
        while( PopFrontQueue( &pending, rsm->to_load, rsm->to_load_lock ) )
        {
            if( rsm->IsAlive( pending.id ) )
            {
                BXFile file = {};
                BXEFileStatus::E status = rsm->filesystem->File( &file, pending.hfile );
                SYS_ASSERT( status == BXEFileStatus::READY );

                RSMResourceData* data = &rsm->rdata[pending.id.index];
                const bool load_ok = pending.loader->Load( data, file.pointer, file.size, file.allocator );
                if( load_ok )
                {
                    rsm->rstate[pending.id.index] = RSMEState::READY;
                }
                else
                {
                    SYS_LOG_ERROR( "Resource failed to load (%s)", rsm->rname[pending.id.index].c_str() );
                    rsm->rstate[pending.id.index] = RSMEState::FAIL;
                }
                rsm->filesystem->CloseFile( pending.hfile, !load_ok );
            }
        }

        while( PopFrontQueue( &pending, rsm->to_unload, rsm->to_unload_lock ) )
        {

        }
    }
}

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

static void FileLoadCallback( BXIFilesystem* fs, BXFileHandle fhandle, BXEFileStatus::E file_status, void* user_data0, void* user_data1, void* user_data2 )
{
    RSM::RSMImpl* rsm = (RSM::RSMImpl*)user_data0;
    RSMLoader* loader = (RSMLoader*)user_data1;
    id_t id = { (uint32_t)user_data2 };

    RSMPendingResource pending = {};
    pending.id = id;
    pending.loader = loader;

    if( file_status == BXEFileStatus::READY )
    {
        pending.hfile = fhandle;

        scope_mutex_t guard( rsm->to_load_lock );
        queue::push_back( rsm->to_load, pending );
    }
    else
    {
        rsm->rstate[id.index] = RSMEState::FAIL;
    }

    rsm->sema.signal();
}

static void LookpuInsert( RSM::RSMImpl* rsm, RSMResourceHash rhash, id_t id )
{
    scope_mutex_t guard( rsm->lookup_lock );
    SYS_ASSERT( hash::has( rsm->lookup, rhash.h ) == false );

    hash::set( rsm->lookup, rhash.h, id );
}
static void LookupRemove( RSM::RSMImpl* rsm, RSMResourceHash rhash )
{
    scope_mutex_t guard( rsm->lookup_lock );
    SYS_ASSERT( hash::has( rsm->lookup, rhash.h ) == true );

    hash::remove( rsm->lookup, rhash.h );
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
            id = id_table::create( _rsm->id_alloc );
        }
    
        const uint32_t index = id.index;
        string::create( &_rsm->rname[index], relative_path, _rsm->string_allocator );
        _rsm->rhash[index] = rhash;
        _rsm->rid[index] = id;
        _rsm->rstate[index] = RSMEState::LOADING;
        
        SYS_ASSERT( _rsm->rdata[index].pointer == nullptr );
        SYS_ASSERT( _rsm->rrefcount[index] == 0 );
        _rsm->rrefcount[index] = 1;

        {
            BXPostLoadCallback post_load_cb( FileLoadCallback, _rsm, loader, (void*)id.hash );
            BXIFilesystem::EMode mode = (loader->IsBinary()) ? BXIFilesystem::FILE_MODE_BIN : BXIFilesystem::FILE_MODE_TXT;
            _rsm->filesystem->LoadFile( relative_path, mode, post_load_cb, _rsm->default_resource_allocator );
        }

        result.i = id.hash;

        LookpuInsert( _rsm, rhash, id );
    }

    return result;
}

RSMResourceID RSM::Create( const char* name, const void* data )
{
    const RSMResourceHash rhash = CreateHash( name );

    id_t id = { 0 };
    {
        scope_mutex_t guard( _rsm->id_lock );
        id = id_table::create( _rsm->id_alloc );
    }

    const uint32_t index = id.index;
    string::create( &_rsm->rname[index], name, _rsm->string_allocator );
    _rsm->rhash[index] = rhash;
    _rsm->rid[index] = id;
    
    RSMResourceData& rdata = _rsm->rdata[index];
    rdata.pointer = (void*)data;
    rdata.size = 0;
    rdata.allocator = nullptr;

    _rsm->rstate[index] = RSMEState::READY;

    SYS_ASSERT( _rsm->rrefcount[index] == 0 );
    _rsm->rrefcount[index] = 1;

    LookpuInsert( _rsm, rhash, id );

    return { id.hash };
}

RSMEState::E RSM::Wait( RSMResourceID rid )
{
    id_t id = { rid.i };
    if( !_rsm->IsAlive( id ) )
    {
        return RSMEState::FAIL;
    }

    while( _rsm->rstate[id.index] == RSMEState::LOADING )
    {}

    return _rsm->rstate[id.index];
}

RSMResourceID RSM::Find( const char* relative_path )
{
    const RSMResourceHash rhash = CreateHash( relative_path );
    return Find( rhash );
}

RSMResourceID RSM::Find( RSMResourceHash rhash )
{
    id_t null_id = { 0 };

    scope_mutex_t guard( _rsm->lookup_lock );
    id_t result = hash::get( _rsm->lookup, rhash.h, null_id );
    return { result.hash };
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



