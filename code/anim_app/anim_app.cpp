#include "anim_app.h"
#include <memory\memory.h>
#include <plugin\plugin_registry.h>

#include <filesystem\filesystem_plugin.h>
#include <window\window.h>
#include <window\window_interface.h>

#include <resource_manager\resource_manager.h>
#include <gui\gui.h>
#include <gfx\gfx.h>
#include <entity\entity.h>

#include <common\ground_mesh.h>

#include <3rd_party/imgui/imgui.h>
#include <foundation\time.h>
#include <rdix\rdix.h>
#include "rdix\rdix_debug_draw.h"
#include "util\grid.h"
#include "foundation\type_compound.h"
#include "foundation\math\math_common.h"
#include "util\signal_filter.h"

#include <3rd_party/assimp/Importer.hpp>
#include "anim_tool.h"

static GFXSceneID g_idscene = { 0 };
static GFXCameraID g_idcamera = { 0 };

static GFXCameraInputContext g_camera_input_ctx = {};
static CMNGroundMesh g_ground_mesh = {};

static AnimTool g_tool = {};

bool AnimApp::Startup( int argc, const char** argv, BXPluginRegistry* plugins, BXIAllocator* allocator )
{
    ::Startup( (CMNEngine*)this, argc, argv, plugins, allocator );

    GFXSceneDesc desc;
    desc.max_renderables = 16 + 1;
    desc.name = "anim app scene";
    g_idscene = _gfx->CreateScene( desc );

    {
        CreateGroundMesh( &g_ground_mesh, _gfx, g_idscene, _rsm, vec3_t( 100.f, 0.5f, 100.f ), mat44_t::translation( vec3_t( 0.f, -32.f, 0.f ) ) );
    }

    {// sky
        BXFileWaitResult filewait = _filesystem->LoadFileSync( _filesystem, "texture/sky_cubemap.dds", BXEFIleMode::BIN, allocator );
        if( filewait.file.pointer )
        {
            if( _gfx->SetSkyTextureDDS( g_idscene, filewait.file.pointer, filewait.file.size ) )
            {
                _gfx->EnableSky( g_idscene, true );
            }
        }
        _filesystem->CloseFile( filewait.handle );

    }

    GFXCameraParams camera_params = {};
    camera_params.zfar = 1024;
    g_idcamera = _gfx->CreateCamera( "main", camera_params, mat44_t( mat33_t::identity(), vec3_t( 0.f, 16.f, 64.f ) ) );

    g_tool.StartUp( ".src/anim/", "anim/", allocator );

    return true;
}

void AnimApp::Shutdown( BXPluginRegistry* plugins, BXIAllocator* allocator )
{
    g_tool.ShutDown();

    _gfx->DestroyCamera( g_idcamera );
    _gfx->DestroyScene( g_idscene );
    DestroyGroundMesh( &g_ground_mesh, _gfx );
    ::Shutdown( (CMNEngine*)this, allocator );
}

bool AnimApp::Update( BXWindow* win, unsigned long long deltaTimeUS, BXIAllocator* allocator )
{
    if( win->input.IsKeyPressedOnce( BXInput::eKEY_ESC ) )
        return false;

    GUI::NewFrame();

    const float delta_time_sec = (float)BXTime::Micro_2_Sec( deltaTimeUS );

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

    const mat44_t new_camera_world = CameraMovementFPP( g_camera_input_ctx, current_camera_matrices.world, delta_time_sec*40.f );
    _gfx->SetCameraWorld( g_idcamera, new_camera_world );
    _gfx->ComputeCamera( g_idcamera );

    {
        ENTSystemInfo ent_sys_info = {};
        ent_sys_info.ent = _ent;
        ent_sys_info.gfx = _gfx;
        ent_sys_info.rsm = _rsm;
        ent_sys_info.default_allocator = allocator;
        ent_sys_info.scratch_allocator = allocator;
        ent_sys_info.gfx_scene_id = g_idscene;
        _ent->Step( &ent_sys_info, deltaTimeUS );
    }

    {
        g_tool.Tick( _filesystem );
        g_tool.DrawMenu();
    }


    GFXFrameContext* frame_ctx = _gfx->BeginFrame( _rdicmdq, _rsm );

    _gfx->GenerateCommandBuffer( frame_ctx, g_idscene, g_idcamera );

    if( ImGui::Begin( "Frame info" ) )
    {
        ImGui::Text( "dt: %2.4f", delta_time_sec );
    }
    ImGui::End();

    RDIXDebug::AddAxes( mat44_t::identity() );

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


BX_APPLICATION_PLUGIN_DEFINE( anim_app, AnimApp )