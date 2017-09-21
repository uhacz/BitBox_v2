#include "filesystem_plugin.h"
#include "filesystem_windows.h"

PLUGIN_EXPORT void* BXLoad_filesystem( BXIAllocator * allocator )
{
	bx::FilesystemWindows* fs = BX_NEW( allocator, bx::FilesystemWindows, allocator );
	fs->Startup();
	return fs;
}

PLUGIN_EXPORT void BXUnload_filesystem( void* plugin, BXIAllocator * allocator )
{
	bx::FilesystemWindows* fs = (bx::FilesystemWindows*)plugin;
	fs->Shutdown();
	BX_DELETE( allocator, plugin );
}
