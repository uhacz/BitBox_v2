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


namespace
{
	// batch
	static RDIXCommandBuffer* g_cmdbuffer = nullptr;
	static RDIXTransformBuffer* g_transform_buffer = nullptr;
	
	// camera
	static mat44_t g_camera_world = mat44_t::identity();
	static GFXCameraParams g_camera_params = {};
	static GFXCameraMatrices g_camera_matrices = {};

	// material system
	static RDIConstantBuffer g_material_frame_data_gpu = {};
    
    constexpr uint32_t MAX_MATERIALS = 32;
    static id_table_t<MAX_MATERIALS> g_idmaterial_table;
    static Material                  g_material_data    [MAX_MATERIALS] = {};
    static RDIConstantBuffer         g_material_data_gpu[MAX_MATERIALS] = {};
    static RDIXResourceBinding*      g_material_binding [MAX_MATERIALS] = {};

    // material map
    static id_t                      g_idmat  [MAX_MATERIALS] = {};
    static string_t                  g_matname[MAX_MATERIALS] = {};
    static uint32_t                  g_nbmat = 0;


    GFXCameraInputContext g_camera_input_ctx = {};

	GFX* g_gfx = {};

    constexpr uint32_t MAX_MESHES = 32;
    dense_container_t<MAX_MESHES>* g_meshes = nullptr;
    id_t g_idmesh[MAX_MESHES] = {};
}

bool IsMaterialAlive( id_t id )
{
    return id_table::has( g_idmaterial_table, id );
}
id_t FindMaterial( const char* name )
{
    for( uint32_t i = 0; i < g_nbmat; ++i )
    {
        id_t id = g_idmat[i];
        if( string::equal( g_matname[i].c_str(), name ) )
            return id;
    }
    return makeInvalidHandle<id_t>();
}

RDIXResourceBinding* GetMaterialBinding( id_t id )
{
    if( !IsMaterialAlive( id ) )
        return nullptr;

    return g_material_binding[id.index];
}

id_t CreateMaterial( RDIDevice* dev, const char* name, const Material& data, BXIAllocator* allocator )
{
    id_t id = FindMaterial( name );
    if( IsMaterialAlive( id ) )
        return id;

    id = id_table::create( g_idmaterial_table );

    g_material_data_gpu[id.index] = CreateConstantBuffer( dev, sizeof( Material ) );

    RDIConstantBuffer* cbuffer = &g_material_data_gpu[id.index];
    RDIXResourceBinding* binding = CloneResourceBinding( ResourceBinding( g_gfx->MaterialBase() ), allocator );

    UpdateCBuffer( GetImmediateCommandQueue( dev ), *cbuffer, &data );
    SetConstantBuffer( binding, "MaterialData", cbuffer );

    g_material_binding[id.index] = binding;
    g_material_data[id.index] = data;
    

    const uint32_t map_index = g_nbmat++;
    string::create( &g_matname[map_index], name, allocator );
    g_idmat[map_index] = id;


    return id;
}

void DestroyMaterial( id_t idmat, BXIAllocator* allocator )
{
    const uint32_t map_index = array::find( g_idmat, g_nbmat, idmat );
    if( map_index == TYPE_NOT_FOUND<uint32_t>() )
        return;

    if( g_nbmat == 0 )
        return;

    {
        const uint32_t last_index = --g_nbmat;
        string::free( &g_matname[map_index] );
        g_matname[map_index] = g_matname[last_index];
        memset( &g_matname[last_index], 0x00, sizeof( string_t ) );
        g_idmat[map_index] = g_idmat[last_index];
    }

    DestroyResourceBinding( &g_material_binding[idmat.index], allocator );
    Destroy( &g_material_data_gpu[idmat.index] );

    id_table::destroy( g_idmaterial_table, idmat );
}


bool BXAssetApp::Startup( int argc, const char** argv, BXPluginRegistry* plugins, BXIAllocator* allocator )
{
    {
        dense_container_desc_t desc = {};
        desc.add_stream<mat44_t>( "world" );
        desc.add_stream<RDIXRenderSource*>( "rsource" );
        desc.add_stream<id_t>( "mat" );
        g_meshes = dense_container::create<MAX_MESHES>( desc, allocator );
    }
    
	_filesystem = (BXIFilesystem*)BXGetPlugin( plugins, BX_FILESYSTEM_PLUGIN_NAME );
	_filesystem->SetRoot( "x:/dev/assets/" );

	BXIWindow* win_plugin = (BXIWindow*)BXGetPlugin( plugins, BX_WINDOW_PLUGIN_NAME );
	const BXWindow* window = win_plugin->GetWindow();
	::Startup( &_rdidev, &_rdicmdq, window->GetSystemHandle( window ), window->width, window->height, 0, allocator );

	
    g_gfx = BX_NEW( allocator, GFX );

	GFXDesc gfx_desc = {};
	g_gfx->StartUp( gfx_desc, _rdidev, _filesystem, allocator );


	RDIXTransformBufferDesc transform_buffer_desc = {};
	g_transform_buffer = CreateTransformBuffer( _rdidev, transform_buffer_desc, allocator );
	g_cmdbuffer = CreateCommandBuffer( allocator );


    g_material_frame_data_gpu = CreateConstantBuffer( _rdidev, sizeof( MaterialFrameData ) );
    { // red
        Material mat;
        mat.specular_albedo = vec3_t( 1.f, 0.f, 0.f );
        mat.diffuse_albedo = vec3_t( 1.f, 0.f, 0.f );
        mat.metal = 0.f;
        mat.roughness = 0.15f;
        CreateMaterial( _rdidev, "red", mat, allocator );
    }
    { // green
        Material mat;
        mat.specular_albedo = vec3_t( 0.f, 1.f, 0.f );
        mat.diffuse_albedo = vec3_t( 0.f, 1.f, 0.f );
        mat.metal = 0.f;
        mat.roughness = 0.25f;
        CreateMaterial( _rdidev, "green", mat, allocator );
    }
    { // blue
        Material mat;
        mat.specular_albedo = vec3_t( 0.f, 0.f, 1.f );
        mat.diffuse_albedo = vec3_t( 0.f, 0.f, 1.f );
        mat.metal = 0.f;
        mat.roughness = 0.25f;
        CreateMaterial( _rdidev, "blue", mat, allocator );
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
            id_t idmat = FindMaterial( materials[i % n_shapes] );
            g_meshes->set( g_idmesh[i], "mat", idmat );
           

        }

        for( uint32_t i = 0; i < n_shapes; ++i )
        {
            par_shapes_free_mesh( shapes[i] );
        }
	}


	
	GFXUtils::StartUp( _rdidev, _filesystem, allocator );

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

    DestroyMaterial( FindMaterial( "blue" ), allocator );
    DestroyMaterial( FindMaterial( "green" ), allocator );
    DestroyMaterial( FindMaterial( "red" ), allocator );
    Destroy( &g_material_frame_data_gpu );

	g_gfx->ShutDown();
    BX_DELETE0( allocator, g_gfx );

	GFXUtils::ShutDown( _rdidev );

	

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
        const array_span_t<id_t> materials = g_meshes->stream<id_t>( "mat" );
        
        for( uint32_t i = 0; i < rsources.size(); ++i )
        {
            const uint32_t instance_offset = AppendMatrix( g_transform_buffer, matrices[i] );

            RDIXUpdateConstantBufferCmd* instance_offset_cmd = AllocateCommand<RDIXUpdateConstantBufferCmd>( g_cmdbuffer, sizeof( uint32_t ), nullptr );
            instance_offset_cmd->cbuffer = GetInstanceOffsetCBuffer( g_transform_buffer );
            memcpy( instance_offset_cmd->DataPtr(), &instance_offset, 4 );

            RDIXSetPipelineCmd* pipeline_cmd = AllocateCommand<RDIXSetPipelineCmd>( g_cmdbuffer, instance_offset_cmd );
            pipeline_cmd->pipeline = g_gfx->MaterialBase();
            pipeline_cmd->bindResources = false;

            RDIXSetResourcesCmd* resources_cmd = AllocateCommand<RDIXSetResourcesCmd>( g_cmdbuffer, pipeline_cmd );
            resources_cmd->rbind = GetMaterialBinding( materials[i] );

            RDIXDrawCmd* draw_cmd = AllocateCommand<RDIXDrawCmd>( g_cmdbuffer, resources_cmd );
            draw_cmd->rsource = rsources[i];
            draw_cmd->num_instances = 1;

            SubmitCommand( g_cmdbuffer, instance_offset_cmd, 1 );
        }

        RDIXTransformBufferCommands transform_buff_cmds = UploadAndSetTransformBuffer( g_cmdbuffer, nullptr, g_transform_buffer, bind_info );
        SubmitCommand( g_cmdbuffer, transform_buff_cmds.first, 0 );

        EndCommandBuffer( g_cmdbuffer );
    }

	g_gfx->BeginFrame( _rdicmdq );
	
	SetCbuffers( _rdicmdq, &g_material_frame_data_gpu, MATERIAL_FRAME_DATA_SLOT, 1, RDIEPipeline::ALL_STAGES_MASK );
		
	BindRenderTarget( _rdicmdq, g_gfx->Framebuffer() );
	ClearRenderTarget( _rdicmdq, g_gfx->Framebuffer() , 0.f, 0.f, 0.f, 1.f, 1.f );
	
	SubmitCommandBuffer( _rdicmdq, g_cmdbuffer );

	g_gfx->RasterizeFramebuffer( _rdicmdq, 0, g_camera_params.aspect() );
	g_gfx->EndFrame( _rdicmdq );
	
    return true;
}

BX_APPLICATION_PLUGIN_DEFINE( asset_app, BXAssetApp )