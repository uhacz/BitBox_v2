#include "test_app.h"
#include <window\window.h>
#include <foundation\memory\memory.h>
#include <foundation/plugin/plugin_load.h>

#include <filesystem\filesystem_plugin.h>

BXFileHandle fhandle = {};
bool BXTestApp::Startup( int argc, const char** argv, BXPluginRegistry* plugins, BXIAllocator* allocator )
{
	_filesystem = (BXIFilesystem*)BXPluginLoad( plugins, BX_FILESYSTEM_PLUGIN_NAME, allocator );
	if( _filesystem )
	{
		_filesystem->SetRoot( "x:/dev/assets/" );
	}

	fhandle = _filesystem->LoadFile( "global.cfg", BXIFilesystem::FILE_MODE_TXT );

    return true;
}

void BXTestApp::Shutdown( BXPluginRegistry* plugins, BXIAllocator* allocator )
{
	BXPluginUnload( plugins, BX_FILESYSTEM_PLUGIN_NAME, allocator );
}

bool BXTestApp::Update( BXWindow* win, unsigned long long deltaTimeUS, BXIAllocator* allocator )
{
    if( win->input.IsKeyPressedOnce( BXInput::eKEY_ESC ) )
        return false;
	
	BXFile file = _filesystem->File( fhandle );
	if( file.status == BXFile::STATUS_READY )
	{
		int aa = 0;
		_filesystem->CloseFile( fhandle, true );
	}

    return true;
}

BX_APPLICATION_PLUGIN_DEFINE( test_app, BXTestApp );