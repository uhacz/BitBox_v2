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
    static constexpr uint8_t INVALID_LOADER_INDEX = 0xFF;
    
    mutex_t id_lock;
    id_table_t<MAX_RESOURCES> id_alloc;

    string_t        rname        [MAX_RESOURCES] = {};
    RSMResourceHash rhash        [MAX_RESOURCES] = {};
    id_t            rid          [MAX_RESOURCES] = {};
    RSMEState::E    rstate       [MAX_RESOURCES] = {};
    RSMResourceData rdata        [MAX_RESOURCES] = {};
    uint8_t         rloader_index[MAX_RESOURCES] = {};
    uint16_t        rrefcount    [MAX_RESOURCES] = {};

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

    BXIAllocator* main_allocator = nullptr;
    BXIAllocator* string_allocator = nullptr;
    BXIAllocator* default_resource_allocator = nullptr;
    BXIAllocator* pending_resources_allocator = nullptr;

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
            bool load_ok = false;
            if( rsm->IsAlive( pending.id ) )
            {
                BXFile file = {};
                BXEFileStatus::E status = rsm->filesystem->File( &file, pending.hfile );
                SYS_ASSERT( status == BXEFileStatus::READY );

                RSMResourceData* data = &rsm->rdata[pending.id.index];

                const uint32_t loader_index = rsm->rloader_index[pending.id.index];
                RSMLoader* loader = rsm->loader[loader_index];
                load_ok = loader->Load( data, file.pointer, file.size, file.allocator );
                if( load_ok )
                {
                    rsm->rstate[pending.id.index] = RSMEState::READY;
                }
                else
                {
                    SYS_LOG_ERROR( "Resource failed to load (%s)", rsm->rname[pending.id.index].c_str() );
                    rsm->rstate[pending.id.index] = RSMEState::FAIL;
                }
            }
            rsm->filesystem->CloseFile( pending.hfile, !load_ok );
        }

        while( PopFrontQueue( &pending, rsm->to_unload, rsm->to_unload_lock ) )
        {
            if( rsm->IsAlive( pending.id ) )
            {
                SYS_ASSERT( rsm->rrefcount[pending.id.index] == 0 );

                const uint32_t loader_index = rsm->rloader_index[pending.id.index];
                RSMLoader* loader = rsm->loader[loader_index];
                
                RSMResourceData* data = &rsm->rdata[pending.id.index];                
                loader->Unload( data );
                BX_FREE( data->allocator, data->pointer );

                string::free( &rsm->rname[pending.id.index] );
                rsm->rhash[pending.id.index]         = { 0 };
                rsm->rid[pending.id.index]           = { 0 };
                rsm->rstate[pending.id.index]        = RSMEState::UNLOADED;
                rsm->rdata[pending.id.index]         = {};
                rsm->rloader_index[pending.id.index] = RSM::RSMImpl::INVALID_LOADER_INDEX;
                {
                    scope_mutex_t guard( rsm->id_lock );
                    id_table::destroy( rsm->id_alloc, pending.id );
                }
            }
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
    hash_decoder.type = murmur3_hash32( type, (uint32_t)strlen( type ), seed );
    hash_decoder.name = murmur3_hash32( name, (uint32_t)strlen( name ), hash_decoder.type );
    
    return { hash_decoder.hash };
}

static uint8_t FindLoader( RSM::RSMImpl* impl, RSMResourceHash rhash )
{
    const RSMResourceHashDecoder rhash_decoded = { rhash.h };

    for( uint32_t i = 0; i < impl->nb_loaders; ++i )
    {
        if( rhash_decoded.type == impl->loader_supported_type[i] )
        {
            return uint8_t( i );
        }
    }

    return RSM::RSMImpl::INVALID_LOADER_INDEX;
}

static void FileLoadCallback( BXIFilesystem* fs, BXFileHandle fhandle, BXEFileStatus::E file_status, void* user_data0, void* user_data1, void* user_data2 )
{
    RSM::RSMImpl* rsm = (RSM::RSMImpl*)user_data0;
    id_t id = { (uint32_t)(uintptr_t)user_data1 };

    RSMPendingResource pending = {};
    pending.id = id;

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

    rsm->rrefcount[id.index] = 1;
    hash::set( rsm->lookup, rhash.h, id );

}
static bool LookupRemove( RSM::RSMImpl* rsm, RSMResourceHash rhash )
{
    scope_mutex_t guard( rsm->lookup_lock );
    SYS_ASSERT( hash::has( rsm->lookup, rhash.h ) == true );

    const id_t null_id{ 0 };
    const id_t id = hash::get( rsm->lookup, rhash.h, null_id );
    if( --rsm->rrefcount[id.index] == 0 )
    {
        hash::remove( rsm->lookup, rhash.h );
        return true;
    }
    return false;
}

static id_t LookupFind( RSM::RSMImpl* rsm, RSMResourceHash rhash )
{
    const id_t null_id{ 0 };

    scope_mutex_t guard( rsm->lookup_lock );
    const id_t result = hash::get( rsm->lookup, rhash.h, null_id );
    if( result.hash != null_id.hash )
    {
        rsm->rrefcount[result.index] += 1;
    }
    return result;
}

RSMResourceID RSM::Load( const char* relative_path )
{
    RSMResourceID result = { 0 };
    
    const RSMResourceHash rhash = CreateHash( relative_path );
    const id_t found_id = LookupFind( _rsm, rhash );
    if( found_id.hash )
    {
        return { found_id.hash };
    }

    const uint8_t loader_index = FindLoader( _rsm, rhash );
    if( loader_index != RSM::RSMImpl::INVALID_LOADER_INDEX )
    {
        id_t id = { 0 };
        {
            scope_mutex_t guard( _rsm->id_lock );
            id = id_table::create( _rsm->id_alloc );
        }
    
        LookpuInsert( _rsm, rhash, id );

        const uint32_t index = id.index;
        string::create( &_rsm->rname[index], relative_path, _rsm->string_allocator );
        _rsm->rhash[index] = rhash;
        _rsm->rid[index] = id;
        _rsm->rloader_index[index] = loader_index;
        _rsm->rstate[index] = RSMEState::LOADING;
        
        SYS_ASSERT( _rsm->rdata[index].pointer == nullptr );

        {
            BXPostLoadCallback post_load_cb( FileLoadCallback, _rsm, (void*)(uintptr_t)id.hash );

            RSMLoader* loader = _rsm->loader[loader_index];
            BXIFilesystem::EMode mode = (loader->IsBinary()) ? BXIFilesystem::FILE_MODE_BIN : BXIFilesystem::FILE_MODE_TXT;
            _rsm->filesystem->LoadFile( relative_path, mode, post_load_cb, _rsm->default_resource_allocator );
        }

        result.i = id.hash;
    }

    return result;
}

RSMResourceID RSM::Create( const char* name, const void* data )
{
    const RSMResourceHash rhash = CreateHash( name );

    const id_t found_id = LookupFind( _rsm, rhash );
    if( found_id.hash )
    {
        return { found_id.hash };
    }

    id_t id = { 0 };
    {
        scope_mutex_t guard( _rsm->id_lock );
        id = id_table::create( _rsm->id_alloc );
    }

    LookpuInsert( _rsm, rhash, id );

    const uint32_t index = id.index;
    string::create( &_rsm->rname[index], name, _rsm->string_allocator );
    _rsm->rhash[index] = rhash;
    _rsm->rid[index] = id;
    
    RSMResourceData& rdata = _rsm->rdata[index];
    rdata.pointer = (void*)data;
    rdata.size = 0;
    rdata.allocator = nullptr;

    _rsm->rstate[index] = RSMEState::READY;

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
    id_t id = LookupFind( _rsm, rhash );
    return { id.hash };
}

void* RSM::Get( RSMResourceID id )
{
    id_t iid = { id.i };
    return _rsm->IsAlive( iid ) ? _rsm->rdata[iid.index].pointer : nullptr;
}

void RSM::Release( RSMResourceID id )
{
    id_t iid = { id.i };
    if( _rsm->IsAlive( iid ) )
    {
        RSMResourceHash rhash = _rsm->rhash[iid.index];
        if( LookupRemove( _rsm, rhash ) )
        {
            _rsm->rstate[iid.index] = RSMEState::UNLOADING;
            {
                scope_mutex_t guard( _rsm->id_lock );
                iid = id_table::invalidate( _rsm->id_alloc, iid );
            }

            RSMPendingResource pending = {};
            pending.id = iid;
            {
                scope_mutex_t guard( _rsm->to_unload_lock );
                queue::push_back( _rsm->to_unload, pending );
            }
            _rsm->sema.signal();
        }
    }
}

RSM* RSM::StartUp( BXIFilesystem* filesystem, BXIAllocator* allocator )
{
    uint32_t mem_size = 0;
    mem_size += sizeof( RSM );
    mem_size += sizeof( RSM::RSMImpl );

    void* memory = BX_MALLOC( allocator, mem_size, 8 );
    RSM* parent = (RSM*)memory;
    parent->_rsm = new(parent + 1) RSM::RSMImpl();

    RSM::RSMImpl* rsm = parent->_rsm;

    rsm->filesystem = filesystem;

    rsm->main_allocator = allocator;
    rsm->string_allocator = allocator;
    rsm->default_resource_allocator = allocator;
    rsm->pending_resources_allocator = allocator;

    queue::set_allocator( rsm->to_load, rsm->pending_resources_allocator );
    queue::set_allocator( rsm->to_unload, rsm->pending_resources_allocator );

    rsm->is_running = 1;
    rsm->background_thread = std::thread( BackgroundThread, rsm );
   
    return parent;
}

void RSM::ShutDown( RSM** ptr )
{
    if( !ptr[0] )
        return;

    RSM* parent = ptr[0];
    RSM::RSMImpl* rsm = parent->_rsm;

    rsm->is_running = 0;
    rsm->sema.signal();
    rsm->background_thread.join();

    if( id_table::size( rsm->id_alloc ) > 0 )
    {
        SYS_LOG_ERROR( "There are still loaded resources!!!" );
        for( uint32_t i = 0; i < RSM::RSMImpl::MAX_RESOURCES; ++i )
        {
            const string_t& name = rsm->rname[i];
            if( name.c_str() && string::length( name.c_str() ) )
            {
                SYS_LOG_INFO( " -- %s\n", name.c_str() );
            }
        }
        system( "PAUSE" );
    }

    InvokeDestructor( rsm );
    
    BXIAllocator* allocator = rsm->main_allocator;
    BX_FREE( allocator, ptr[0] );
}



