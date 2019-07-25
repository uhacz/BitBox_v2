#include "test_app.h"
#include <window\window.h>
#include <memory\memory.h>
#include <plugin/plugin_load.h>

#include <plugin/plugin_registry.h>
#include <filesystem/filesystem_plugin.h>

#include <window/window.h>
#include <window/window_interface.h>

#include <stdio.h>
#include <atomic>

#include <gfx/gfx.h>
#include <entity/entity_system.h>
#include <entity\components\name_component.h>
#include "common\ground_mesh.h"
#include "common\sky.h"
#include "gui\gui.h"
#include "3rd_party\imgui\imgui.h"
#include "rdix\rdix.h"
#include "rdix\rdix_debug_draw.h"
#include "foundation\time.h"
#include "anim\anim_player.h"
#include "common\common.h"
#include "anim\anim.h"
#include "foundation\array.h"
#include "util\color.h"
#include "anim\anim_debug.h"
#include "foundation\eastl\vector.h"
#include "foundation\eastl\span.h"

#include "anim_system.h"

static CMNGroundMesh g_ground_mesh;
static GFXCameraInputContext g_camera_input_ctx = {};
static GFXCameraID g_idcamera = { 0 };
static GFXSceneID g_idscene = { 0 };


struct Anim
{
    common::AssetFileHelper<ANIMClip> _clip_file;
    common::AssetFileHelper<ANIMSkel> _skel_file;

    ANIMSimplePlayer _player;
    array_t<ANIMJoint> _joints_ms;
    array_t<mat44_t> _matrices_ms;
};
static Anim g_anim = {};






bool BXTestApp::Startup( int argc, const char** argv, BXPluginRegistry* plugins, BXIAllocator* allocator )
{
    CMNEngine::Startup( this, argc, argv, plugins, allocator );

    ANIMClipID clipId = ToID<ANIMClipID>( 666u );
    u32 hash = ToHash( clipId );

    ANIMSkelID skelId;
    skelId.index = 10;
    skelId.generation = 20;

    u32 h = ToHash( skelId );


    GFXSceneDesc desc;
    desc.max_renderables = 16 + 1;
    desc.name = "test scene";
    g_idscene = gfx->CreateScene( desc );

    CreateGroundMesh( &g_ground_mesh, gfx, g_idscene, vec3_t( 100.f, 1.f, 100.f ), mat44_t::translation( vec3_t( 0.f, -0.5f, 0.f ) ) );
    CreateSky( "texture/sky_cubemap.dds", g_idscene, gfx, filesystem, allocator );

    g_idcamera = gfx->CreateCamera( "main", GFXCameraParams(), mat44_t( mat33_t::identity(), vec3_t( 0.f, 4.f, 18.f ) ) );

    g_anim._clip_file.LoadSync( filesystem, allocator, "anim/walk.clip" );
    g_anim._skel_file.LoadSync( filesystem, allocator, "anim/human.skel" );

    g_anim._player.Prepare( g_anim._skel_file.data, allocator );
    array::resize( g_anim._joints_ms, g_anim._skel_file.data->numJoints );
    array::resize( g_anim._matrices_ms, g_anim._skel_file.data->numJoints );

    g_anim._player.Play( g_anim._clip_file.data, 0.f, 0.f, 0 );

	return true;
}

void BXTestApp::Shutdown( BXPluginRegistry* plugins, BXIAllocator* allocator )
{
    g_anim._player.Unprepare();
    g_anim._skel_file.FreeFile();
    g_anim._clip_file.FreeFile();

    DestroyGroundMesh( &g_ground_mesh, gfx );
    gfx->DestroyScene( g_idscene );
    gfx->DestroyCamera( g_idcamera );

    CMNEngine::Shutdown( this );
}

bool BXTestApp::Update( BXWindow* win, unsigned long long deltaTimeUS, BXIAllocator* allocator )
{
    if( win->input.IsKeyPressedOnce( BXInput::eKEY_ESC ) )
        return false;
	
    GUI::NewFrame();

    const float delta_time_sec = (float)BXTime::Micro_2_Sec( deltaTimeUS );
    if( ImGui::Begin( "Frame info" ) )
    {
        ImGui::Text( "dt: %2.4f", delta_time_sec );
    }
    ImGui::End();

    {
        g_anim._player.Tick( delta_time_sec );

        anim_ext::LocalJointsToWorldJoints( &g_anim._joints_ms[0], g_anim._player.LocalJoints(), g_anim._skel_file.data, ANIMJoint::identity() );
        anim_debug::DrawPose( g_anim._skel_file.data, to_array_span( g_anim._joints_ms ), color32_t::RED(), 0.01f );
    }


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
    const GFXCameraMatrices& current_camera_matrices = gfx->CameraMatrices( g_idcamera );

    const mat44_t new_camera_world = CameraMovementFPP( g_camera_input_ctx, current_camera_matrices.world, delta_time_sec * 10.f );
    gfx->SetCameraWorld( g_idcamera, new_camera_world );
    gfx->ComputeCamera( g_idcamera );

    {
        if( ImGui::Begin( "Camera" ) )
        {
            const vec3_t camera_pos = new_camera_world.translation();
            ImGui::InputFloat3( "position", (float*)&camera_pos.x );
        }
        ImGui::End();
    }

    gfx->DoSkinningCPU( rdicmdq );
    gfx->DoSkinningGPU( rdicmdq );

    GFXFrameContext* frame_ctx = gfx->BeginFrame( rdicmdq );

    gfx->GenerateCommandBuffer( frame_ctx, g_idscene, g_idcamera );

    BindRenderTarget( rdicmdq, gfx->Framebuffer() );
    ClearRenderTarget( rdicmdq, gfx->Framebuffer(), 0.f, 0.f, 0.f, 1.f, 1.f );

    gfx->SubmitCommandBuffer( frame_ctx, g_idscene );

    gfx->PostProcess( frame_ctx, g_idcamera );

    {
        const GFXCameraMatrices& camera_matrices = gfx->CameraMatrices( g_idcamera );
        const mat44_t viewproj = camera_matrices.proj_api * camera_matrices.view;

        BindRenderTarget( rdicmdq, gfx->Framebuffer(), { 1 }, true );
        RDIXDebug::Flush( rdicmdq, viewproj );
    }

    gfx->RasterizeFramebuffer( rdicmdq, 1, g_idcamera );

    GUI::Draw();

    gfx->EndFrame( frame_ctx );

    return true;
}

BX_APPLICATION_PLUGIN_DEFINE( test_app, BXTestApp );