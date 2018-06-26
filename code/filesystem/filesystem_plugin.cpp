#include "filesystem_plugin.h"
#include "filesystem_windows.h"

#include <util/file_system_name.h>
#include <foundation/string_util.h>
#include <foundation/io.h>
#include "dirent.h"

static BXFileWaitResult LoadFileSync( BXIFilesystem* fs, const char * relativePath, BXEFIleMode::E mode, BXIAllocator* allocator )
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

static void AppendRelativePath( string_buffer_t* s, const char* relative_path )
{
    uint32_t len = string::length( relative_path );
    if( len > 0 )
    {
        if( relative_path[len - 1] == '/' || relative_path[len - 1] == '\\' )
            len -= 1;
    
        string::appendn( s, relative_path, len, '/' );
    }
}

static void ListFiles( BXIFilesystem* fs, string_buffer_t* s, const char* relative_path, uint32_t flags, BXIAllocator* allocator )
{
    FSName abs_path;
    abs_path.Append( fs->GetRoot() );
    abs_path.AppendRelativePath( relative_path );

    DIR *dir;
    struct dirent *ent;

    const bool append_relative_name = (flags & BXEFileListFlag::ONLY_NAMES) == 0;

    if( dir = opendir( abs_path.AbsolutePath() ) )
    {
        while( ent = readdir( dir ) )
        {
            const bool dot = ent->d_namlen == 1 && ent->d_name[0] == '.';
            const bool dotdot = ent->d_namlen == 2 && string::equal( ent->d_name, ".." );
            if( dot || dotdot )
                continue;

            if( ent->d_type == DT_REG )
            {
                string::append( s, "F" );
                if( append_relative_name )
                {
                    AppendRelativePath( s, relative_path );
                }
                string::appendn( s, ent->d_name, (uint32_t)ent->d_namlen );
            }
            else if( ent->d_type == DT_DIR )
            {
                string::append( s, "D+" );
                if( append_relative_name )
                {
                    AppendRelativePath( s, relative_path );
                }
                string::appendn( s, ent->d_name, (uint32_t)ent->d_namlen );
                if( flags & BXEFileListFlag::RECURSE )
                {
                    char child_relative_path[256] = {};
                    sprintf_s( child_relative_path, 255, "%s%s/", relative_path, ent->d_name );
                    ListFiles( fs, s, child_relative_path, flags, allocator );
                }
                string::append( s, "D-" );
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

