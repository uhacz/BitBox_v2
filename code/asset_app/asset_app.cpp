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

#include <gfx/gfx_shader_interop.h>

#include <shaders/hlsl/material_frame_data.h>
#include <shaders/hlsl/material_data.h>
#include <shaders/hlsl/transform_instance_data.h>

#include <foundation/array.h>
#include <foundation/id_array.h>
#include <foundation/id_table.h>
#include <foundation/dense_container.h>
#include <foundation/string_util.h>
#include "foundation\plugin\plugin_load.h"
#include "foundation\buffer.h"
#include "foundation\id_allocator_dense.h"
#include "foundation\container_soa.h"

namespace
{
	// batch
	static RDIXCommandBuffer* g_cmdbuffer = nullptr;
	static RDIXTransformBuffer* g_transform_buffer = nullptr;
	
	// camera
	static mat44_t g_camera_world = mat44_t::identity();
	static GFXCameraParams g_camera_params = {};
	static GFXCameraMatrices g_camera_matrices = {};

    GFXCameraInputContext g_camera_input_ctx = {};

    constexpr uint32_t MAX_MESHES = 32;
    dense_container_t<MAX_MESHES>* g_meshes = nullptr;
    id_t g_idmesh[MAX_MESHES] = {};

    


    GFXSceneID g_idscene = { 0 };

}



struct SomeContainer
{
    float* stream0;
    uint16_t* stream1;
    mat44_t* stream2;
    uint32_t** stream3;
};

bool BXAssetApp::Startup( int argc, const char** argv, BXPluginRegistry* plugins, BXIAllocator* allocator )
{
    //container_soa_desc_t desc;
    //container_soa_add_stream( desc, SomeContainer, stream0 );
    //container_soa_add_stream( desc, SomeContainer, stream1 );
    //container_soa_add_stream( desc, SomeContainer, stream2 );
    //container_soa_add_stream( desc, SomeContainer, stream3 );

    //const uint32_t N = 10;

    //SomeContainer* c = container_soa::create<SomeContainer>( desc, N, allocator );

    //const uint32_t cap = container_soa::capacity( c );
    //for( uint32_t i = 0; i < N; ++i )
    //{
    //    uint32_t index = container_soa::push_back( c );
    //    
    //}

    //id_allocator_dense_t* id_alloc = id_allocator::create_dense( N, allocator );
    //id_t id[N];
    //for( uint32_t i = 0; i < cap; ++i )
    //{
    //    id[i] = id_allocator::alloc( id_alloc );
    //    uint32_t index = id_allocator::dense_index( id_alloc, id[i] );

    //    c->stream0[index] = (float)index;
    //    c->stream1[index] = index;
    //    c->stream2[index] = mat44_t( (float)index );
    //    c->stream3[index] = (uint32_t*)(cap + index);
    //}

    //id_allocator_dense_t::delete_info_t di = id_allocator::free( id_alloc, id[5] );
    //container_soa::remove_packed( c, di.removed_at_index );

    //di = id_allocator::free( id_alloc, id[0] );
    //container_soa::remove_packed( c, di.removed_at_index );

    //di = id_allocator::free( id_alloc, id[7] );
    //container_soa::remove_packed( c, di.removed_at_index );

    //id[0] = id_allocator::alloc( id_alloc );

    //container_soa::destroy( &c );

    {
        dense_container_desc_t desc = {};
        desc.add_stream<mat44_t>( "world" );
        desc.add_stream<RDIXRenderSource*>( "rsource" );
        desc.add_stream<GFXMaterialID>( "mat" );
        g_meshes = dense_container::create<MAX_MESHES>( desc, allocator );
    }
    
	_filesystem = (BXIFilesystem*)BXGetPlugin( plugins, BX_FILESYSTEM_PLUGIN_NAME );
	_filesystem->SetRoot( "x:/dev/assets/" );

	BXIWindow* win_plugin = (BXIWindow*)BXGetPlugin( plugins, BX_WINDOW_PLUGIN_NAME );
	const BXWindow* window = win_plugin->GetWindow();
	::Startup( &_rdidev, &_rdicmdq, window->GetSystemHandle( window ), window->width, window->height, 0, allocator );


    GFXDesc gfxdesc = {};
    _gfx = GFX::Allocate( allocator );
    _gfx->StartUp( _rdidev, gfxdesc, _filesystem, allocator );

    g_idscene = _gfx->CreateScene( GFXSceneDesc() );
	
	RDIXTransformBufferDesc transform_buffer_desc = {};
	g_transform_buffer = CreateTransformBuffer( _rdidev, transform_buffer_desc, allocator );
	g_cmdbuffer = CreateCommandBuffer( allocator );


    //g_material_frame_data_gpu = CreateConstantBuffer( _rdidev, sizeof( MaterialFrameData ) );
    { // red
        GFXMaterialDesc mat;
        mat.data.specular_albedo = vec3_t( 1.f, 0.f, 0.f );
        mat.data.diffuse_albedo = vec3_t( 1.f, 0.f, 0.f );
        mat.data.metal = 0.f;
        mat.data.roughness = 0.15f;
        _gfx->CreateMaterial( "red", mat );
    }
    { // green
        GFXMaterialDesc mat;
        mat.data.specular_albedo = vec3_t( 0.f, 1.f, 0.f );
        mat.data.diffuse_albedo = vec3_t( 0.f, 1.f, 0.f );
        mat.data.metal = 0.f;
        mat.data.roughness = 0.25f;
        _gfx->CreateMaterial( "green", mat );
    }
    { // blue
        GFXMaterialDesc mat;
        mat.data.specular_albedo = vec3_t( 0.f, 0.f, 1.f );
        mat.data.diffuse_albedo = vec3_t( 0.f, 0.f, 1.f );
        mat.data.metal = 0.f;
        mat.data.roughness = 0.25f;
        _gfx->CreateMaterial( "blue", mat );
    }

    {
        par_shapes_mesh* shapes[] =
        {
            par_shapes_create_parametric_sphere( 64, 64, FLT_EPSILON ),
            par_shapes_create_torus( 64, 64, 0.5f ),
            par_shapes_create_rock( 0xBCAA, 4 ),
        };

        const char* materials[] =
        {
            "red", "green", "blue"
        };

        const uint32_t n_shapes = (uint32_t)sizeof_array( shapes );
        for( uint32_t i = 0; i < n_shapes; ++i )
        {
            g_idmesh[i] = dense_container::create( g_meshes );
            RDIXRenderSource* rsource = CreateRenderSourceFromShape( _rdidev, shapes[i], allocator );
            g_meshes->set( g_idmesh[i], "rsource", rsource );
            g_meshes->set( g_idmesh[i], "world", mat44_t( quat_t::identity(), vec3_t( i * 4.f - 1.f, 0.f, 0.f ) ) );
            GFXMaterialID idmat = _gfx->FindMaterial( materials[i % n_shapes] );
            g_meshes->set( g_idmesh[i], "mat", idmat );
        }

        for( uint32_t i = 0; i < n_shapes; ++i )
        {
            par_shapes_free_mesh( shapes[i] );
        }
	}
    	
	g_camera_world = mat44_t( mat33_t::identity(), vec3_t( 0.f, 0.f, 5.f ) );


    return true;
}

void BXAssetApp::Shutdown( BXPluginRegistry* plugins, BXIAllocator* allocator )
{
    {
        auto rsource_stream = g_meshes->stream<RDIXRenderSource*>( "rsource" );
        for( RDIXRenderSource* rsource : rsource_stream )
        {
            DestroyRenderSource( _rdidev, &rsource, allocator );
        }
    }

    dense_container::destroy( &g_meshes );

    _gfx->DestroyMaterial( _gfx->FindMaterial( "blue" ) );
    _gfx->DestroyMaterial( _gfx->FindMaterial( "green" ) );
    _gfx->DestroyMaterial( _gfx->FindMaterial( "red" ) );
    _gfx->DestroyScene( g_idscene );
    _gfx->ShutDown();
    GFX::Free( &_gfx, allocator );

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
    _gfx->SetCamera( g_camera_params, g_camera_matrices );

    {
        ClearTransformBuffer( g_transform_buffer );
        ClearCommandBuffer( g_cmdbuffer, allocator );
        BeginCommandBuffer( g_cmdbuffer );

        RDIXTransformBufferBindInfo bind_info;
        bind_info.instance_offset_slot = TRANSFORM_INSTANCE_DATA_SLOT;
        bind_info.matrix_start_slot = TRANSFORM_INSTANCE_WORLD_SLOT;
        bind_info.stage_mask = RDIEPipeline::VERTEX_MASK;
        
        const array_span_t<RDIXRenderSource*> rsources = g_meshes->stream< RDIXRenderSource*>( "rsource" );
        const array_span_t<mat44_t> matrices = g_meshes->stream< mat44_t >( "world" );
        const array_span_t<GFXMaterialID> materials = g_meshes->stream<GFXMaterialID>( "mat" );
        
        for( uint32_t i = 0; i < rsources.size(); ++i )
        {
            const uint32_t instance_offset = AppendMatrix( g_transform_buffer, matrices[i] );

            RDIXUpdateConstantBufferCmd* instance_offset_cmd = AllocateCommand<RDIXUpdateConstantBufferCmd>( g_cmdbuffer, sizeof( uint32_t ), nullptr );
            instance_offset_cmd->cbuffer = GetInstanceOffsetCBuffer( g_transform_buffer );
            memcpy( instance_offset_cmd->DataPtr(), &instance_offset, 4 );

            RDIXSetPipelineCmd* pipeline_cmd = AllocateCommand<RDIXSetPipelineCmd>( g_cmdbuffer, instance_offset_cmd );
            pipeline_cmd->pipeline = _gfx->MaterialBase();
            pipeline_cmd->bindResources = false;

            RDIXSetResourcesCmd* resources_cmd = AllocateCommand<RDIXSetResourcesCmd>( g_cmdbuffer, pipeline_cmd );
            resources_cmd->rbind = _gfx->MaterialBinding( materials[i] );

            RDIXDrawCmd* draw_cmd = AllocateCommand<RDIXDrawCmd>( g_cmdbuffer, resources_cmd );
            draw_cmd->rsource = rsources[i];
            draw_cmd->num_instances = 1;

            SubmitCommand( g_cmdbuffer, instance_offset_cmd, 1 );
        }

        RDIXTransformBufferCommands transform_buff_cmds = UploadAndSetTransformBuffer( g_cmdbuffer, nullptr, g_transform_buffer, bind_info );
        SubmitCommand( g_cmdbuffer, transform_buff_cmds.first, 0 );

        EndCommandBuffer( g_cmdbuffer );
    }

    GFXFrameContext* frame_ctx = _gfx->BeginFrame( _rdicmdq );
	
    _gfx->BindMaterialFrame( frame_ctx );
		
    BindRenderTarget( _rdicmdq, _gfx->Framebuffer() );
    ClearRenderTarget( _rdicmdq, _gfx->Framebuffer(), 0.f, 0.f, 0.f, 1.f, 1.f );
	
	SubmitCommandBuffer( _rdicmdq, g_cmdbuffer );

    _gfx->RasterizeFramebuffer( _rdicmdq, 0, g_camera_params.aspect() );
    _gfx->EndFrame( frame_ctx );

    return true;
}

BX_APPLICATION_PLUGIN_DEFINE( asset_app, BXAssetApp )