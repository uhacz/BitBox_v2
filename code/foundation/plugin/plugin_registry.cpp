#include "plugin_registry.h"
#include "../memory/memory.h"
#include "../hashmap.h"
#include "../hash.h"
#include "../debug.h"
#include <string.h>

static u64 CreatePluginNameHash( const char* name )
{
    const u32 len = (u32)strlen( name );
    const u32 lo = murmur3_hash32( name, len, 0x666 );
    const u32 hi = murmur3_hash32( name, len, lo );

    return u64( hi ) << 32 | u64( lo );

    //
    //SYS_ASSERT( len <= 8 );
    //
    //union
    //{
    //    u64 tag64;
    //    u8 tag8[8];
    //};
    //memcpy( tag8, name, len );
    //for( size_t i = len; i < 8; ++i )
    //    tag8[i] = 0;
    //
    //return tag64;
}

struct BXPluginRegistry
{
    using Plugins = hash_t<void*>;
    Plugins _plugs;
    u32 _count = 0;

    BXPluginRegistry( BXIAllocator* allocator )
        : _plugs( allocator )
    {}
};

void BXAddPlugin( BXPluginRegistry* reg, const char* name, void* plugin )
{
    const u64 name_hash = CreatePluginNameHash( name );

    SYS_ASSERT( hash::has( reg->_plugs, name_hash ) == false );
    hash::set( reg->_plugs, name_hash, plugin );        
    reg->_count += 1;
}

void BXRemovePlugin( BXPluginRegistry* reg, const char* name )
{
    const u64 name_hash = CreatePluginNameHash( name );
    if( !hash::has( reg->_plugs, name_hash ) )
        return;
    
    hash::remove( reg->_plugs, name_hash );
    SYS_ASSERT( reg->_count > 0 );
    reg->_count -= 1;
}

void* BXGetPlugin( BXPluginRegistry* reg, const char* name )
{
    const u64 name_hash = CreatePluginNameHash( name );

    void* null_plugin = nullptr;
    void* plugin = hash::get( reg->_plugs, name_hash, null_plugin );
    return plugin;
}

void BXPluginRegistryStartup( BXPluginRegistry** regPP, BXIAllocator* allocator )
{
    regPP[0] = BX_NEW( allocator, BXPluginRegistry, allocator );
}

void BXPluginRegistryShutdown( BXPluginRegistry** regPP, BXIAllocator* allocator )
{
    if( !regPP[0] )
        return;

    SYS_ASSERT( regPP[0]->_count == 0 );
    BX_DELETE0( allocator, regPP[0] );
}
