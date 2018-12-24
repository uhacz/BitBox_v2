#include "asset_app.h"

#include <memory\memory.h>
#include <rtti\rtti.h>
#include <plugin\plugin_registry.h>
#include <plugin\plugin_load.h>

#include <window\window.h>
#include <window\window_interface.h>

#include <filesystem\filesystem_plugin.h>

#include <string.h>
#include <foundation/time.h>
#include <foundation/array.h>
#include <foundation/id_array.h>
#include <foundation/id_table.h>
#include <foundation/string_util.h>
#include <foundation\buffer.h>
#include <foundation\id_allocator_dense.h>
#include <foundation\container_soa.h>

#include <util/guid.h>
#include <util/random\random.h>
#include <util/grid.h>
#include <util/poly_shape/poly_shape.h>

#include <resource_manager\resource_manager.h>
#include <rdix/rdix.h>
#include <rdix/rdix_command_buffer.h>

#include <gfx/gfx.h>
#include <gfx/gfx_camera.h>
#include <gfx/gfx_shader_interop.h>


#include <gui/gui.h>
#include <3rd_party/imgui/imgui.h>

#include "entity/entity_system.h"

#include "material_editor.h"
#include "mesh_tool.h"
#include "common\ground_mesh.h"

#include "components.h"



namespace
{
    CMNGroundMesh g_ground_mesh;
    
    GFXCameraInputContext g_camera_input_ctx = {};
    GFXCameraID g_idcamera = { 0 };
    GFXSceneID g_idscene = { 0 };
}



static MATEditor g_mat_editor;
static MESHTool g_mesh_tool;

bool BXAssetApp::Startup( int argc, const char** argv, BXPluginRegistry* plugins, BXIAllocator* allocator )
{
    CMNEngine::Startup( (CMNEngine*)this, argc, argv, plugins, allocator );

    {
        RegisterComponent< TOOLMeshComponent >( _ecs, "TOOL_Mesh" );
        RegisterComponent< TOOLAnimComponent >( _ecs, "TOOL_Anim" );
    }

    GFXSceneDesc desc;
    desc.max_renderables = 16 + 1;
    desc.name = "test scene";
    g_idscene = _gfx->CreateScene( desc );

    CreateGroundMesh( &g_ground_mesh, _gfx, g_idscene, vec3_t(100.f, 0.5f, 100.f), mat44_t::translation( vec3_t( 0.f, -2.f, 0.f ) ) );
    
    {// sky
        BXFileWaitResult filewait = _filesystem->LoadFileSync( _filesystem, "texture/sky_cubemap.dds", BXEFIleMode::BIN, allocator );
        if( filewait.file.pointer )
        {
            if( _gfx->SetSkyTextureDDS( g_idscene, filewait.file.pointer, filewait.file.size ) )
            {
                _gfx->EnableSky( g_idscene, true );
            }
        }
        _filesystem->CloseFile( &filewait.handle );

    }
    g_idcamera = _gfx->CreateCamera( "main", GFXCameraParams(), mat44_t( mat33_t::identity(), vec3_t( 0.f, 0.f, 5.f ) ) );

    g_mat_editor.StartUp( _gfx, g_idscene, allocator );
    g_mesh_tool.StartUp( this, ".src/mesh/", g_idscene, allocator );

    return true;
}

void BXAssetApp::Shutdown( BXPluginRegistry* plugins, BXIAllocator* allocator )
{
    g_mesh_tool.ShutDown( this );
    g_mat_editor.ShutDown( _gfx );
    
    DestroyGroundMesh( &g_ground_mesh, _gfx );
    _gfx->DestroyScene( g_idscene );
    _gfx->DestroyCamera( g_idcamera );

    CMNEngine::Shutdown( (CMNEngine*)this, allocator );
}

bool BXAssetApp::Update( BXWindow* win, unsigned long long deltaTimeUS, BXIAllocator* allocator )
{
    if( win->input.IsKeyPressedOnce( BXInput::eKEY_ESC ) )
        return false;

    GUI::NewFrame();

	const float delta_time_sec = (float)BXTime::Micro_2_Sec( deltaTimeUS );

    _ecs->Update();

    if( !ImGui::GetIO().WantCaptureMouse )
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
    const GFXCameraMatrices& current_camera_matrices = _gfx->CameraMatrices( g_idcamera );

	const mat44_t new_camera_world = CameraMovementFPP( g_camera_input_ctx, current_camera_matrices.world, delta_time_sec * 10.f );
    _gfx->SetCameraWorld( g_idcamera, new_camera_world );
    _gfx->ComputeCamera( g_idcamera );

    g_mat_editor.Tick( _gfx, _filesystem );
    g_mesh_tool.Tick( this );

    GFXFrameContext* frame_ctx = _gfx->BeginFrame( _rdicmdq );
    {
        _gfx->GenerateCommandBuffer( frame_ctx, g_idscene, g_idcamera );
    }

    if( ImGui::Begin( "Frame info" ) )
    {
        ImGui::Text( "dt: %2.4f", delta_time_sec );
    }
    ImGui::End();
		
    BindRenderTarget( _rdicmdq, _gfx->Framebuffer() );
    ClearRenderTarget( _rdicmdq, _gfx->Framebuffer(), 0.f, 0.f, 0.f, 1.f, 1.f );
	
    _gfx->SubmitCommandBuffer( frame_ctx, g_idscene );

    _gfx->PostProcess( frame_ctx, g_idcamera );

    _gfx->RasterizeFramebuffer( _rdicmdq, 1, g_idcamera );

    GUI::Draw();

    _gfx->EndFrame( frame_ctx );

    return true;
}

BX_APPLICATION_PLUGIN_DEFINE( asset_app, BXAssetApp )