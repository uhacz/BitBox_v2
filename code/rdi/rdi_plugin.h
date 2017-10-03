#pragma once

#include <foundation/plugin/plugin_interface.h>
#define BX_RDI_PLUGIN_NAME "rdi"




struct BXIRenderDevice
{};

struct BXIRenderContext
{};


extern "C" {          // we need to export the C interface

	PLUGIN_EXPORT void* BXLoad_filesystem( BXIAllocator* allocator );
	PLUGIN_EXPORT void  BXUnload_filesystem( void* plugin, BXIAllocator* allocator );

}

