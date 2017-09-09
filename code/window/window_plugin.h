#pragma once

#include <foundation/plugin/plugin_interface.h>

extern "C" {          // we need to export the C interface

PLUGIN_EXPORT void* BXLoad_window( BXIAllocator* allocator );
PLUGIN_EXPORT void BXUnload_window( void* plugin, BXIAllocator* allocator );

}
