#include "filesystem_plugin.h"
#include "filesystem_windows.h"



BXFileWaitResult LoadFileSync( BXIFilesystem* fs, const char * relativePath, BXIFilesystem::EMode mode, BXIAllocator* allocator )
{
	BXFileWaitResult result;
	result.handle = fs->LoadFile( relativePath, mode, allocator );

	result.status = fs->File( &result.file, result.handle );
	while( result.status == BXEFileStatus::LOADING )
	{
		result.status = fs->File( &result.file, result.handle );
	}
	return result;
}


PLUGIN_EXPORT void* BXLoad_filesystem( BXIAllocator * allocator )
{
	bx::FilesystemWindows* fs = BX_NEW( allocator, bx::FilesystemWindows, allocator );
	fs->LoadFileSync = LoadFileSync;
	fs->Startup();
	return fs;
}

PLUGIN_EXPORT void BXUnload_filesystem( void* plugin, BXIAllocator * allocator )
{
	bx::FilesystemWindows* fs = (bx::FilesystemWindows*)plugin;
	fs->Shutdown();
	BX_DELETE( allocator, fs );
}

