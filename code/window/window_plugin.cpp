#include "window_plugin.h"
#include "window_internal.h"
#include "window_interface.h"

#include <foundation/memory/memory.h>

PLUGIN_EXPORT void* BXLoad_window( BXIAllocator* allocator )
{
    BXIWindow* plugin = BX_ALLOCATE( allocator, BXIWindow );
    plugin->Create = bx::WindowCreate;
    plugin->Release = bx::WindowRelease;
    return plugin;
}

PLUGIN_EXPORT void BXUnload_window( void* plugin, BXIAllocator* allocator )
{
    BX_FREE( allocator, plugin );
}
