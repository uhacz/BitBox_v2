#include "test_app.h"
#include <window\window.h>
#include <foundation\memory\memory.h>
#include <foundation/plugin/plugin_load.h>

#include <foundation/plugin/plugin_registry.h>
#include <filesystem/filesystem_plugin.h>

#include <window/window.h>
#include <window/window_interface.h>

#include <stdio.h>
#include <atomic>
//////////////////////
struct StateChunk
{
	uint32_t offset : 24;
	uint32_t stateId : 8;
};
struct StateBlock
{
	uint32_t proxyId = 0;
	uint32_t num_chunks = 0;
	StateChunk chunks[30];
};

struct StateAllocator
{
	StateBlock* AllocateBlock();
	uint8_t* AllocateData( uint32_t size );

	void Clear();

	uint8_t* _data = nullptr;
	uint32_t _capacity = 0;
	std::atomic_uint32_t _size = 0;
};


StateVectorWriter* state_vec = proxy.BeginWrite();
state_vec->Set<State>( value );
proxy.Commit( state_vec );

StateVectorReader* state_vec = proxy.BeginRead();
state_vec->Get<State>();



//////////////////////



bool BXTestApp::Startup( int argc, const char** argv, BXPluginRegistry* plugins, BXIAllocator* allocator )
{
	_filesystem = (BXIFilesystem*)BXGetPlugin( plugins, BX_FILESYSTEM_PLUGIN_NAME );
	if( _filesystem )
	{
		_filesystem->SetRoot( "x:/dev/assets/" );
	}

	BXIWindow* win_plugin = (BXIWindow*)BXGetPlugin( plugins, BX_WINDOW_PLUGIN_NAME );
	const BXWindow* window = win_plugin->GetWindow();
	::Startup( &_rdidev, &_rdicmdq, window->GetSystemHandle( window ), 800, 600, 0, allocator );

	return true;
}

void BXTestApp::Shutdown( BXPluginRegistry* plugins, BXIAllocator* allocator )
{
	::Shutdown( &_rdidev, &_rdicmdq, allocator );
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