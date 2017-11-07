#include "asset_app.h"

#include <window\window.h>
#include <window\window_interface.h>

#include <filesystem\filesystem_plugin.h>

#include <foundation\plugin\plugin_registry.h>
#include <foundation\memory\memory.h>
#include <util/par_shapes/par_shapes.h>

#include <rdix/rdix.h>
#include <rdix/rdix_command_buffer.h>
#include <gfx/gfx_camera.h>


namespace
{
	static RDIXCommandBuffer* g_cmdbuffer = nullptr;
	static RDIXTransformBuffer* g_transform_buffer = nullptr;

	static RDIXPipeline* g_pipeline = nullptr;
	static RDIXRenderSource* g_rsource = nullptr;

	static GFXCameraParams g_camera_params = {};
	static GFXCameraMatrices g_camera_matrices = {};
	
}

bool BXAssetApp::Startup( int argc, const char** argv, BXPluginRegistry* plugins, BXIAllocator* allocator )
{
	_filesystem = (BXIFilesystem*)BXGetPlugin( plugins, BX_FILESYSTEM_PLUGIN_NAME );
	_filesystem->SetRoot( "x:/dev/assets/" );

	BXIWindow* win_plugin = (BXIWindow*)BXGetPlugin( plugins, BX_WINDOW_PLUGIN_NAME );
	const BXWindow* window = win_plugin->GetWindow();
	::Startup( &_rdidev, &_rdicmdq, window->GetSystemHandle( window ), 800, 600, 0, allocator );

	g_cmdbuffer = CreateCommandBuffer( allocator );

	RDIXTransformBufferDesc transform_buffer_desc = {};
	g_transform_buffer = CreateTransformBuffer( _rdidev, transform_buffer_desc, allocator );

	RDIXShaderFile* shader_file = LoadShaderFile( "shader/hlsl/bin/material.shader", _filesystem, allocator );
	
	RDIXPipelineDesc pipeline_desc = {};
	pipeline_desc.Shader( shader_file, "base" );
	g_pipeline = CreatePipeline( _rdidev, pipeline_desc, allocator );
	
	UnloadShaderFile( &shader_file, allocator );

	{
		par_shapes_mesh* shape = par_shapes_create_parametric_sphere( 5, 5 );
		g_rsource = CreateRenderSourceFromShape( _rdidev, shape, allocator );
		par_shapes_free_mesh( shape );
	}

    return true;
}

void BXAssetApp::Shutdown( BXPluginRegistry* plugins, BXIAllocator* allocator )
{
	DestroyRenderSource( _rdidev, &g_rsource, allocator );
	DestroyPipeline( _rdidev, &g_pipeline, allocator );
	DestroyTransformBuffer( _rdidev, &g_transform_buffer, allocator );
	DestroyCommandBuffer( &g_cmdbuffer, allocator );
	
	::Shutdown( &_rdidev, &_rdicmdq, allocator );
    _filesystem = nullptr;
}

bool BXAssetApp::Update( BXWindow* win, unsigned long long deltaTimeUS, BXIAllocator* allocator )
{
    if( win->input.IsKeyPressedOnce( BXInput::eKEY_ESC ) )
        return false;



    return true;
}

BX_APPLICATION_PLUGIN_DEFINE( asset_app, BXAssetApp )