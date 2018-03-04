#include "plugin_registry.h"
#include "../memory/memory.h"
#include "../hashmap.h"
#include "../hash.h"
#include "../debug.h"
#include <string.h>

static uint64_t CreatePluginNameHash( const char* name )
{
    const uint32_t len = (uint32_t)strlen( name );
    const uint32_t lo = murmur3_hash32( name, len, 0x666 );
    const uint32_t hi = murmur3_hash32( name, len, lo );

    return uint64_t( hi ) << 32 | uint64_t( lo );
}

struct BXPluginRegistry
{
    using Plugins = hash_t<void*>;
    Plugins _plugs;
    uint32_t _count = 0;

    BXPluginRegistry( BXIAllocator* allocator )
        : _plugs( allocator )
    {}

    ~BXPluginRegistry()
    {
    }
};

void BXAddPlugin( BXPluginRegistry* reg, const char* name, void* plugin )
{
    const uint64_t name_hash = CreatePluginNameHash( name );

    SYS_ASSERT( hash::has( reg->_plugs, name_hash ) == false );
    hash::set( reg->_plugs, name_hash, plugin );        
    reg->_count += 1;
}

void BXRemovePlugin( BXPluginRegistry* reg, const char* name )
{
    const uint64_t name_hash = CreatePluginNameHash( name );
    if( !hash::has( reg->_plugs, name_hash ) )
        return;
    
    hash::remove( reg->_plugs, name_hash );
    SYS_ASSERT( reg->_count > 0 );
    reg->_count -= 1;
}

void* BXGetPlugin( BXPluginRegistry* reg, const char* name )
{
    const uint64_t name_hash = CreatePluginNameHash( name );

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
