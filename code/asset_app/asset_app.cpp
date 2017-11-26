#include "asset_app.h"

#include <window\window.h>
#include <window\window_interface.h>

#include <filesystem\filesystem_plugin.h>

#include <foundation\plugin\plugin_registry.h>
#include <foundation\memory\memory.h>
#include <foundation/time.h>

#include <util/par_shapes/par_shapes.h>

#include <rdix/rdix.h>
#include <rdix/rdix_command_buffer.h>
#include <gfx/gfx.h>
#include <gfx/gfx_camera.h>

#include <string.h>

using float2 = vec2_t;
using float3 = vec3_t;
using float4 = vec4_t;
using float3x3 = mat33_t;
using float4x4 = mat44_t;
using uint = uint32_t;

#include <shaders/hlsl/material_frame_data.h>
#include <shaders/hlsl/material_data.h>
#include <shaders/hlsl/transform_instance_data.h>


namespace
{
	// batch
	static RDIXCommandBuffer* g_cmdbuffer = nullptr;
	static RDIXTransformBuffer* g_transform_buffer = nullptr;
	
	// object
	static RDIXRenderSource* g_rsource = nullptr;
	static mat44_t g_instance_matrix = mat44_t::identity();

	// camera
	static mat44_t g_camera_world = mat44_t::identity();
	static GFXCameraParams g_camera_params = {};
	static GFXCameraMatrices g_camera_matrices = {};

	// material system
	static RDIConstantBuffer g_material_data_gpu = {};
	static RDIConstantBuffer g_material_frame_data_gpu = {};
	static RDIConstantBuffer g_transform_instance_data_gpu = {};

	// material instance
	static Material g_material = {};
	static RDIXResourceBinding* g_material_resource_binding = nullptr;
	
	GFXCameraInputContext g_camera_input_ctx = {};

	GFX g_gfx = {};

}
static void BindShaderSamplers( RDICommandQueue* cmdq, const ShaderSamplers& samplers, uint32_t slot )
{
	
}





bool BXAssetApp::Startup( int argc, const char** argv, BXPluginRegistry* plugins, BXIAllocator* allocator )
{
	_filesystem = (BXIFilesystem*)BXGetPlugin( plugins, BX_FILESYSTEM_PLUGIN_NAME );
	_filesystem->SetRoot( "d:/dev/assets/" );

	BXIWindow* win_plugin = (BXIWindow*)BXGetPlugin( plugins, BX_WINDOW_PLUGIN_NAME );
	const BXWindow* window = win_plugin->GetWindow();
	::Startup( &_rdidev, &_rdicmdq, window->GetSystemHandle( window ), window->width, window->height, 0, allocator );

	
	GFXDesc gfx_desc = {};
	g_gfx.StartUp( gfx_desc, _rdidev, _filesystem, allocator );


	RDIXTransformBufferDesc transform_buffer_desc = {};
	g_transform_buffer = CreateTransformBuffer( _rdidev, transform_buffer_desc, allocator );

	g_cmdbuffer = CreateCommandBuffer( allocator );

	{
		par_shapes_mesh* shape = par_shapes_create_parametric_sphere( 64, 64, FLT_EPSILON );
		//par_shapes_mesh* shape = par_shapes_create_torus( 64, 64, 0.5f);
        //par_shapes_mesh* shape = par_shapes_create_rock( 0xBCAA, 4 );
		g_rsource = CreateRenderSourceFromShape( _rdidev, shape, allocator );
		par_shapes_free_mesh( shape );
	}

	g_material_data_gpu = CreateConstantBuffer( _rdidev, sizeof( Material ) );
	g_material_frame_data_gpu = CreateConstantBuffer( _rdidev, sizeof( MaterialFrameData ) );
	g_transform_instance_data_gpu = CreateConstantBuffer( _rdidev, sizeof( TransformInstanceData ) );

	g_material.specular_albedo = vec3_t( 1.f, 1.f, 1.f );
	g_material.diffuse_albedo = vec3_t( 1.f, 0.f, 0.f );
	g_material.metal = 0.f;
	g_material.roughness = 0.25f;
	UpdateCBuffer( _rdicmdq, g_material_data_gpu, &g_material );
	g_material_resource_binding = CloneResourceBinding( ResourceBinding( g_gfx.MaterialBase() ), allocator );
	SetConstantBuffer( g_material_resource_binding, "MaterialData", &g_material_data_gpu );

	
	GFXUtils::StartUp( _rdidev, _filesystem, allocator );

	g_camera_world = mat44_t( mat33_t::identity(), vec3_t( 0.f, 0.f, 5.f ) );


    return true;
}

void BXAssetApp::Shutdown( BXPluginRegistry* plugins, BXIAllocator* allocator )
{
	g_gfx.ShutDown();
	GFXUtils::ShutDown( _rdidev );

	Destroy( &g_transform_instance_data_gpu );
	Destroy( &g_material_frame_data_gpu );
	Destroy( &g_material_data_gpu );

	DestroyResourceBinding( &g_material_resource_binding, allocator );
	DestroyRenderSource( _rdidev, &g_rsource, allocator );
	DestroyTransformBuffer( _rdidev, &g_transform_buffer, allocator );
	DestroyCommandBuffer( &g_cmdbuffer, allocator );

	::Shutdown( &_rdidev, &_rdicmdq, allocator );
    _filesystem = nullptr;
}

bool BXAssetApp::Update( BXWindow* win, unsigned long long deltaTimeUS, BXIAllocator* allocator )
{
    if( win->input.IsKeyPressedOnce( BXInput::eKEY_ESC ) )
        return false;

	const float delta_time_sec = (float)BXTime::Micro_2_Sec( deltaTimeUS );

	{
		const float sensitivity_in_pix = delta_time_sec;
		BXInput* input = &win->input;
		BXInput::Mouse* input_mouse = &input->mouse;

		const BXInput::MouseState* current_state = input_mouse->CurrentState();

		g_camera_input_ctx.UpdateInput( 
			current_state->lbutton,
			current_state->mbutton,
			current_state->rbutton,
			current_state->dx,
			current_state->dy,
			sensitivity_in_pix,
			delta_time_sec );
	}

	g_camera_world = CameraMovementFPP( g_camera_input_ctx, g_camera_world, delta_time_sec * 10.f );

	ComputeMatrices( &g_camera_matrices, g_camera_params, g_camera_world );
	{
		MaterialFrameData fdata;
		fdata.camera_world = g_camera_matrices.world;
		fdata.camera_view = g_camera_matrices.view;
		fdata.camera_view_proj = g_camera_matrices.view_proj;
		fdata.camera_eye = vec4_t( g_camera_matrices.eye(), 1.f );
		fdata.camera_dir = vec4_t( g_camera_matrices.dir(), 0.f );
		UpdateCBuffer( _rdicmdq, g_material_frame_data_gpu, &fdata );
	};
	
	ClearTransformBuffer( g_transform_buffer );
	uint32_t instance_offset = AppendMatrix( g_transform_buffer, g_instance_matrix );
	
	{
		ClearCommandBuffer( g_cmdbuffer, allocator );
		BeginCommandBuffer( g_cmdbuffer );

		RDIXTransformBufferCommands transform_buff_cmds = UploadAndSetTransformBuffer( g_cmdbuffer, nullptr, g_transform_buffer, TRANSFORM_INSTANCE_WORLD_SLOT, RDIEPipeline::VERTEX_MASK );

		RDIXUpdateConstantBufferCmd* instance_offset_cmd = AllocateCommand<RDIXUpdateConstantBufferCmd>( g_cmdbuffer, sizeof( uint32_t ), transform_buff_cmds.last );
		instance_offset_cmd->cbuffer = g_transform_instance_data_gpu;
		memcpy( instance_offset_cmd->DataPtr(), &instance_offset, 4 );

		RDIXSetPipelineCmd* pipeline_cmd = AllocateCommand<RDIXSetPipelineCmd>( g_cmdbuffer, instance_offset_cmd );
		pipeline_cmd->pipeline = g_gfx.MaterialBase();
		pipeline_cmd->bindResources = false;

		RDIXSetResourcesCmd* resources_cmd = AllocateCommand<RDIXSetResourcesCmd>( g_cmdbuffer, pipeline_cmd );
		resources_cmd->rbind = g_material_resource_binding;
		
		RDIXDrawCmd* draw_cmd = AllocateCommand<RDIXDrawCmd>( g_cmdbuffer, resources_cmd );
		draw_cmd->rsource = g_rsource;
		draw_cmd->num_instances = 1;

		SubmitCommand( g_cmdbuffer, transform_buff_cmds.first, 0 );

		EndCommandBuffer( g_cmdbuffer );
	}

	g_gfx.BeginFrame( _rdicmdq );
	
	SetCbuffers( _rdicmdq, &g_material_frame_data_gpu, MATERIAL_FRAME_DATA_SLOT, 1, RDIEPipeline::ALL_STAGES_MASK );
		
	BindRenderTarget( _rdicmdq, g_gfx.Framebuffer() );
	ClearRenderTarget( _rdicmdq, g_gfx.Framebuffer() , 0.f, 0.f, 0.f, 1.f, 1.f );
	
	SubmitCommandBuffer( _rdicmdq, g_cmdbuffer );

	g_gfx.RasterizeFramebuffer( _rdicmdq, 0, g_camera_params.aspect() );
	g_gfx.EndFrame( _rdicmdq );
	
    return true;
}

BX_APPLICATION_PLUGIN_DEFINE( asset_app, BXAssetApp )