#include "asset_app.h"

#include <window\window.h>
#include <window\window_interface.h>

#include <foundation/time.h>
#include <rdix/rdix.h>
#include <rdix/rdix_debug_draw.h>

#include <gui/gui.h>
#include <3rd_party/imgui/imgui.h>

#include <entity/entity_system.h>
#include <common/ground_mesh.h>

#include "components.h"

#include "material_tool.h"
#include "mesh_tool.h"
#include "anim_tool.h"

namespace
{
    CMNGroundMesh g_ground_mesh;
    
    GFXCameraInputContext g_camera_input_ctx = {};
    GFXCameraID g_idcamera = { 0 };
    GFXSceneID g_idscene = { 0 };
    ECSEntityID g_identity = {};
}

static MATERIALTool g_mat_tool;
static MESHTool g_mesh_tool;
static ANIMTool g_anim_tool;

bool BXAssetApp::Startup( int argc, const char** argv, BXPluginRegistry* plugins, BXIAllocator* allocator )
{
    CMNEngine::Startup( (CMNEngine*)this, argc, argv, plugins, allocator );

    {
        RegisterComponent< TOOLMeshComponent >( _ecs, "TOOL_Mesh" );
        RegisterComponent< TOOLAnimComponent >( _ecs, "TOOL_Anim" );
        RegisterComponentNoPOD< TOOLSkinningComponent >( _ecs, "TOOL_Skinning" );
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

    g_identity = _ecs->CreateEntity();

    g_mat_tool.StartUp( _gfx, allocator );
    g_mesh_tool.StartUp( this, ".src/mesh/", allocator );
    g_anim_tool.StartUp( ".src/anim/", "anim/", allocator );

    return true;
}

void BXAssetApp::Shutdown( BXPluginRegistry* plugins, BXIAllocator* allocator )
{
    g_anim_tool.ShutDown();
    g_mesh_tool.ShutDown( this );
    g_mat_tool.ShutDown( _gfx );
    
    _ecs->MarkForDestroy( g_identity );

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

    {
        TOOLContext tool_ctx;
        tool_ctx.camera = g_idcamera;
        tool_ctx.gfx_scene = g_idscene;
        tool_ctx.entity = g_identity;

        g_mat_tool.Tick( _gfx, _filesystem );
        g_mesh_tool.Tick( this, tool_ctx );
        g_anim_tool.Tick( this, tool_ctx, delta_time_sec );
        
    }

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

    {
        const GFXCameraMatrices& camera_matrices = _gfx->CameraMatrices( g_idcamera );
        const mat44_t viewproj = camera_matrices.proj_api * camera_matrices.view;

        BindRenderTarget( _rdicmdq, _gfx->Framebuffer(), { 1 }, true );
        RDIXDebug::Flush( _rdicmdq, viewproj );
    }

    _gfx->RasterizeFramebuffer( _rdicmdq, 1, g_idcamera );

    GUI::Draw();

    _gfx->EndFrame( frame_ctx );

    return true;
}

BX_APPLICATION_PLUGIN_DEFINE( asset_app, BXAssetApp )