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

using float2 = vec2_t;
using float3 = vec3_t;
using float4 = vec4_t;
using float3x3 = mat33_t;
using float4x4 = mat44_t;
using uint = uint32_t;

#include <shaders/hlsl/material_frame_data.h>
#include <shaders/hlsl/transform_instance_data.h>

namespace
{
	static RDIXCommandBuffer* g_cmdbuffer = nullptr;
	static RDIXTransformBuffer* g_transform_buffer = nullptr;

	static RDIXPipeline* g_pipeline = nullptr;
	static RDIXRenderSource* g_rsource = nullptr;

	static mat44_t g_camera_world = mat44_t::identity();
	static GFXCameraParams g_camera_params = {};
	static GFXCameraMatrices g_camera_matrices = {};

	static RDIConstantBuffer g_material_frame_data_gpu = {};
	static RDIConstantBuffer g_transform_instance_data_gpu = {};

}

bool BXAssetApp::Startup( int argc, const char** argv, BXPluginRegistry* plugins, BXIAllocator* allocator )
{
	_filesystem = (BXIFilesystem*)BXGetPlugin( plugins, BX_FILESYSTEM_PLUGIN_NAME );
	_filesystem->SetRoot( "x:/dev/assets/" );

	BXIWindow* win_plugin = (BXIWindow*)BXGetPlugin( plugins, BX_WINDOW_PLUGIN_NAME );
	const BXWindow* window = win_plugin->GetWindow();
	::Startup( &_rdidev, &_rdicmdq, window->GetSystemHandle( window ), 800, 600, 0, allocator );

	g_material_frame_data_gpu = CreateConstantBuffer( _rdidev, sizeof( MaterialFrameData ) );
	g_transform_instance_data_gpu = CreateConstantBuffer( _rdidev, sizeof( TransformInstanceData ) );

	RDIXTransformBufferDesc transform_buffer_desc = {};
	g_transform_buffer = CreateTransformBuffer( _rdidev, transform_buffer_desc, allocator );

	g_cmdbuffer = CreateCommandBuffer( allocator );

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



	g_camera_world = mat44_t( mat33_t::identity(), vec3_t( 0.f, 1.f, -5.f ) );


    return true;
}

void BXAssetApp::Shutdown( BXPluginRegistry* plugins, BXIAllocator* allocator )
{
	Destroy( &g_transform_instance_data_gpu );
	Destroy( &g_material_frame_data_gpu );

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

	ComputeMatrices( &g_camera_matrices, g_camera_params, g_camera_world );
	{
		MaterialFrameData fdata;
		fdata.camera_world = g_camera_matrices.world;
		fdata.camera_view = g_camera_matrices.view;
		fdata.camera_view_proj = g_camera_matrices.view_proj;
		UpdateCBuffer( _rdicmdq, g_material_frame_data_gpu, &fdata );
	};
		
	ClearState( _rdicmdq );
	
	SetCbuffers( _rdicmdq, &g_material_frame_data_gpu, MATERIAL_FRAME_DATA_SLOT, 1, RDIEPipeline::ALL_STAGES_MASK );



	
	float clear_color[5] = { 0.f, 0.f, 0.f, 1.f, 1.f };
	ClearBuffers( _rdicmdq, nullptr, 0, RDITextureDepth(), clear_color, 1, 1 );
	
	ClearCommandBuffer( g_cmdbuffer, allocator );

	SubmitCommandBuffer( _rdicmdq, g_cmdbuffer );

	Swap( _rdicmdq );

	

    return true;
}

BX_APPLICATION_PLUGIN_DEFINE( asset_app, BXAssetApp )