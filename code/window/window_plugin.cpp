#include "window_plugin.h"
#include "window_internal.h"
#include "window_interface.h"

#include <memory/memory.h>

PLUGIN_EXPORT void* BXLoad_window( BXIAllocator* allocator )
{
    //BXDLLSetMemoryHook( allocator );
    BXIWindow* plugin = BX_NEW( allocator, bx::Window );
    return plugin;
}

PLUGIN_EXPORT void BXUnload_window( void* plugin, BXIAllocator* allocator )
{
    BX_FREE( allocator, plugin );
}
