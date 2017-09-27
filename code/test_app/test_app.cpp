#include "test_app.h"
#include <window\window.h>
#include <foundation\memory\memory.h>
#include <foundation/plugin/plugin_load.h>

#include <foundation/plugin/plugin_registry.h>
#include <filesystem/filesystem_plugin.h>

#include <stdio.h>

//BXFileHandle fhandle = {};
//bool bvalue = false;

bool BXTestApp::Startup( int argc, const char** argv, BXPluginRegistry* plugins, BXIAllocator* allocator )
{
	_filesystem = (BXIFilesystem*)BXGetPlugin( plugins, BX_FILESYSTEM_PLUGIN_NAME );
	if( _filesystem )
	{
		_filesystem->SetRoot( "x:/dev/assets/" );
	}
	return true;
}

void BXTestApp::Shutdown( BXPluginRegistry* plugins, BXIAllocator* allocator )
{
	_filesystem = nullptr;
}

bool BXTestApp::Update( BXWindow* win, unsigned long long deltaTimeUS, BXIAllocator* allocator )
{
    if( win->input.IsKeyPressedOnce( BXInput::eKEY_ESC ) )
        return false;
	
	//if( !bvalue )
	//{
	//	fhandle = _filesystem->LoadFile( "global.1cfg", BXIFilesystem::FILE_MODE_TXT );
	//	bvalue = true;
	//}

	//BXFile file = {};
	//BXEFileStatus::E file_status = _filesystem->File( &file, fhandle );
	//if( file_status != BXEFileStatus::EMPTY )
	//{
	//	printf( "file status: %d\n", file_status );

	//	if( file_status == BXEFileStatus::READY )
	//	{
	//		printf( "file loaded :)" );
	//		_filesystem->CloseFile( fhandle, true );
	//	}
	//	else if( file_status == BXEFileStatus::NOT_FOUND )
	//	{
	//		printf( "file not loaded :)" );
	//		_filesystem->CloseFile( fhandle, true );
	//	}
	//}

	//BXFileWaitResult result = _filesystem->LoadFileSync( _filesystem, "global.cfg", BXIFilesystem::FILE_MODE_TXT );
	//_filesystem->CloseFile( result.handle );

    return true;
}

BX_APPLICATION_PLUGIN_DEFINE( test_app, BXTestApp );