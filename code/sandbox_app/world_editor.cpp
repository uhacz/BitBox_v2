#include "world_editor.h"

#include "../input/input.h"
#include "../rdix/rdix.h"
#include "../rdix/rdix_debug_draw.h"
#include "../gfx/gfx.h"
#include "../gui/gui.h"

#include "../common/base_engine_init.h"
#include "../common/sky.h"

#include "../3rd_party/imgui/imgui.h"

namespace sandbox
{

    void WorldEditor::Initialize( CMNEngine* e, BXIAllocator* allocator )
    {
        _allocator = allocator;
        GFXSceneDesc scene_desc;
        _gfx_scene_id = e->gfx->CreateScene( scene_desc );

        GFXCameraParams camera_params;
        _camera.id = e->gfx->CreateCamera( "persp", camera_params, mat44_t::translation( vec3_t( 0.f, 1.f, 1.f ) ) );

        const GFXCameraMatrices& current_camera_matrices = e->gfx->CameraMatrices( _camera.id );
        _camera.prev_matrix = current_camera_matrices.world;
        _camera.next_matrix = current_camera_matrices.world;

        CreateSky( "texture/sky_cubemap.dds", _gfx_scene_id, e->gfx, e->filesystem, allocator );
    }

    void WorldEditor::Uninitialize( CMNEngine* e )
    {
        e->gfx->DestroyCamera( _camera.id );
        e->gfx->DestroyScene( _gfx_scene_id );
    }

    void WorldEditor::TickCamera( GFX* gfx, const BXInput& input, f32 dt )
    {
        const BXInput::Mouse* input_mouse = &input.mouse;
        const BXInput::MouseState& current_state = input_mouse->CurrentState();

        GFXCameraInputContext raw_input;
        raw_input.ReadInput( current_state.lbutton,
            current_state.mbutton,
            current_state.rbutton,
            current_state.dx,
            current_state.dy,
            _camera.sensitivity );
        CameraInputFilter( &_camera.input_ctx, raw_input, _camera.input_ctx, 0.1f, dt );

        _camera.time_acc += dt;
        while( _camera.time_acc >= _camera.tick_dt )
        {
            _camera.prev_matrix = _camera.next_matrix;
            _camera.next_matrix = CameraMovementFPP( _camera.input_ctx, _camera.prev_matrix, _camera.tick_dt );

            _camera.time_acc -= _camera.tick_dt;
        }

        const f32 t = _camera.time_acc / _camera.tick_dt;
        vec3_t pos = lerp( t, _camera.prev_matrix.translation(), _camera.next_matrix.translation() );
        quat_t rot = slerp( t, quat_t( _camera.prev_matrix.upper3x3() ), quat_t( _camera.next_matrix.upper3x3() ) );
        const mat44_t new_camera_world = mat44_t( rot, pos );

        gfx->SetCameraWorld( _camera.id, new_camera_world );
        gfx->ComputeCamera( _camera.id );
    }

    void WorldEditor::RenderGUI( GFX* gfx )
    {
        if( ImGui::Begin( "World" ) )
        {
            RenderGUI_Camera( gfx );
        }
        ImGui::End();

        RDIXDebug::AddAxes( mat44_t::identity() );
        {
            const f32 ext = (f32)_ground_plane.extent;
            for( i32 i = -_ground_plane.extent; i <= _ground_plane.extent; ++i )
            {
                const f32 fi = (f32)i;
                RDIXDebug::AddLine( vec3_t( fi, 0.f, -ext ), vec3_t( fi, 0.f, ext ), RDIXDebugParams( _ground_plane.color ) );
                RDIXDebug::AddLine( vec3_t( -ext, 0.f, fi ), vec3_t( ext, 0.f, fi ), RDIXDebugParams( _ground_plane.color ) );
            }
        }
    }

    void WorldEditor::RenderGUI_Camera( GFX* gfx )
    {
        if( ImGui::TreeNodeEx( "Camera", ImGuiTreeNodeFlags_CollapsingHeader ) )
        {
            ImGui::SliderFloat( "sensitivity", &_camera.sensitivity, 0.f, 1.f );

            GFXCameraParams camera_params = gfx->CameraParams( _camera.id );
            bool camera_params_changed = false;
            camera_params_changed |= ImGui::InputFloat2( "Aperture", &camera_params.h_aperture );
            camera_params_changed |= ImGui::InputFloat( "Focal length", &camera_params.focal_length );
            camera_params_changed |= ImGui::InputFloat2( "Clip plane", &camera_params.znear );
            camera_params_changed |= ImGui::InputFloat2( "Ortho size", &camera_params.ortho_width );
            if( camera_params_changed )
            {
                gfx->SetCameraParams( _camera.id, camera_params );
            }

            ImGui::TreePop();
        }
    }

    void WorldEditor::RenderFrame( RDICommandQueue* rdicmdq, GFX* gfx )
    {
        gfx->DoSkinningCPU( rdicmdq );
        gfx->DoSkinningGPU( rdicmdq );

        GFXFrameContext* frame_ctx = gfx->BeginFrame( rdicmdq );

        gfx->GenerateCommandBuffer( frame_ctx, _gfx_scene_id, _camera.id );

        BindRenderTarget( rdicmdq, gfx->Framebuffer() );
        ClearRenderTarget( rdicmdq, gfx->Framebuffer(), 0.f, 0.f, 0.f, 1.f, 1.f );

        gfx->SubmitCommandBuffer( frame_ctx, _gfx_scene_id );

        gfx->PostProcess( frame_ctx, _camera.id );

        {
            const GFXCameraMatrices& camera_matrices = gfx->CameraMatrices( _camera.id );
            const mat44_t viewproj = camera_matrices.proj_api * camera_matrices.view;

            BindRenderTarget( rdicmdq, gfx->Framebuffer(), { 1 }, true );
            RDIXDebug::Flush( rdicmdq, viewproj );
        }

        gfx->RasterizeFramebuffer( rdicmdq, 1, _camera.id );

        GUI::Draw();

        gfx->EndFrame( frame_ctx );
    }

}//