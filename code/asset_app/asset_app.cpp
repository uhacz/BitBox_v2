#include "asset_app.h"

#include <window\window.h>
#include <window\window_interface.h>

#include <filesystem\filesystem_plugin.h>

#include <foundation\plugin\plugin_registry.h>
#include <foundation\memory\memory.h>
#include <foundation/time.h>

#include <util/poly_shape/poly_shape.h>

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
	// camera
	static mat44_t g_camera_world = mat44_t::identity();
	static GFXCameraParams g_camera_params = {};
	static GFXCameraMatrices g_camera_matrices = {};

    GFXCameraInputContext g_camera_input_ctx = {};

    constexpr uint32_t MAX_MESHES = 3;
    constexpr uint32_t MAX_MESH_INSTANCES = 32;

    GFXSceneID g_idscene = { 0 };
    GFXMeshID g_idmesh[MAX_MESHES] = {};
    GFXMeshInstanceID g_meshes[MAX_MESH_INSTANCES] = {};
}

bool BXAssetApp::Startup( int argc, const char** argv, BXPluginRegistry* plugins, BXIAllocator* allocator )
{
 
	_filesystem = (BXIFilesystem*)BXGetPlugin( plugins, BX_FILESYSTEM_PLUGIN_NAME );
	_filesystem->SetRoot( "x:/dev/assets/" );

	BXIWindow* win_plugin = (BXIWindow*)BXGetPlugin( plugins, BX_WINDOW_PLUGIN_NAME );
	const BXWindow* window = win_plugin->GetWindow();
	::Startup( &_rdidev, &_rdicmdq, window->GetSystemHandle( window ), window->width, window->height, 0, allocator );


    GFXDesc gfxdesc = {};
    _gfx = GFX::Allocate( allocator );
    _gfx->StartUp( _rdidev, gfxdesc, _filesystem, allocator );

    g_idscene = _gfx->CreateScene( GFXSceneDesc() );
	
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
        poly_shape_t shapes[MAX_MESHES] = {};
        poly_shape::createShpere( &shapes[0], 8, allocator );
        poly_shape::createBox( &shapes[1], 4, allocator );
        poly_shape::createShpere( &shapes[2], 2, allocator );
        const uint32_t n_shapes = (uint32_t)sizeof_array( shapes );
        for( uint32_t i = 0; i < n_shapes; ++i )
        {
            GFXMeshDesc desc;
            desc.rsouce = CreateRenderSourceFromShape( _rdidev, &shapes[i], allocator );
            g_idmesh[i] = _gfx->CreateMesh( desc );
        }
        for( uint32_t i = 0; i < n_shapes; ++i )
        {
            poly_shape::deallocateShape( &shapes[i] );
        }


        const char* materials[] =
        {
            "red", "green", "blue",
        };
        const uint32_t n_materials = sizeof_array( materials );

        for( uint32_t i = 0; i < 4; ++i )
        {
            GFXMeshInstanceDesc desc = {};
            desc.idmesh = g_idmesh[i % MAX_MESHES];
            desc.idmaterial = _gfx->FindMaterial( materials[i % n_materials] );
            g_meshes[i] = _gfx->AddMeshToScene( g_idscene, desc, mat44_t( quat_t::identity(), vec3_t( i * 4.f - 1.f, 0.f, 0.f ) ) );
        }
	}
    
	g_camera_world = mat44_t( mat33_t::identity(), vec3_t( 0.f, 0.f, 5.f ) );


    return true;
}

void BXAssetApp::Shutdown( BXPluginRegistry* plugins, BXIAllocator* allocator )
{
    _gfx->DestroyScene( g_idscene );

    for( uint32_t i = 0; i < MAX_MESHES; ++i )
    {
        _gfx->DestroyMesh( g_idmesh[i] );
    }

    _gfx->DestroyMaterial( _gfx->FindMaterial( "blue" ) );
    _gfx->DestroyMaterial( _gfx->FindMaterial( "green" ) );
    _gfx->DestroyMaterial( _gfx->FindMaterial( "red" ) );
    
    _gfx->ShutDown();
    GFX::Free( &_gfx, allocator );

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

    GFXFrameContext* frame_ctx = _gfx->BeginFrame( _rdicmdq );
    static bool tmp = false;
    if( !tmp )
    {
        _gfx->GenerateCommandBuffer( frame_ctx, g_idscene, g_camera_params, g_camera_matrices );
        tmp = true;
    }

    _gfx->BindMaterialFrame( frame_ctx );
		
    BindRenderTarget( _rdicmdq, _gfx->Framebuffer() );
    ClearRenderTarget( _rdicmdq, _gfx->Framebuffer(), 0.f, 0.f, 0.f, 1.f, 1.f );
	
    _gfx->SubmitCommandBuffer( frame_ctx, g_idscene );

    _gfx->RasterizeFramebuffer( _rdicmdq, 0, g_camera_params.aspect() );
    _gfx->EndFrame( frame_ctx );

    return true;
}

BX_APPLICATION_PLUGIN_DEFINE( asset_app, BXAssetApp )