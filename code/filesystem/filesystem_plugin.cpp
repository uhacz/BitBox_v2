#include "filesystem_plugin.h"
#include "filesystem_windows.h"

#include <util/file_system_name.h>
#include <foundation/string_util.h>
#include <foundation/io.h>
#include "dirent.h"

static BXFileWaitResult LoadFileSync( BXIFilesystem* fs, const char * relativePath, BXIFilesystem::EMode mode, BXIAllocator* allocator )
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
static int32_t WriteFileSync( BXIFilesystem* fs, const char* relative_path, const void* data, uint32_t data_size )
{
    FSName abs_path;
    abs_path.Append( fs->GetRoot() );
    abs_path.AppendRelativePath( relative_path );

    return WriteFile( abs_path.AbsolutePath(), data, data_size );
}


static void ListFiles( BXIFilesystem* fs, string_buffer_t* s, const char* relative_path, bool recurse, BXIAllocator* allocator )
{
    FSName abs_path;
    abs_path.Append( fs->GetRoot() );
    abs_path.AppendRelativePath( relative_path );

    DIR *dir;
    struct dirent *ent;

    if( dir = opendir( abs_path.AbsolutePath() ) )
    {
        while( ent = readdir( dir ) )
        {
            if( ent->d_type == DT_REG || ent->d_type == DT_DIR )
            {
                string::append( s, ent->d_name );
            }
        }
    
        closedir( dir );
    }
}

PLUGIN_EXPORT void* BXLoad_filesystem( BXIAllocator * allocator )
{
	bx::FilesystemWindows* fs = BX_NEW( allocator, bx::FilesystemWindows, allocator );
	fs->LoadFileSync = LoadFileSync;
    fs->WriteFileSync = WriteFileSync;
    fs->ListFiles = ListFiles;
	fs->Startup();
	return fs;
}

PLUGIN_EXPORT void BXUnload_filesystem( void* plugin, BXIAllocator * allocator )
{
	bx::FilesystemWindows* fs = (bx::FilesystemWindows*)plugin;
	fs->Shutdown();
	BX_DELETE( allocator, fs );
}

