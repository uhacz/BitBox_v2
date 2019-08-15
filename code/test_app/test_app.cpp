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
#include "util\signal_filter.h"
#include "math\math_common.h"

static CMNGroundMesh g_ground_mesh;
static GFXCameraInputContext g_camera_input_ctx = {};
static GFXCameraID g_idcamera = { 0 };
static GFXSceneID g_idscene = { 0 };


struct Anim
{
    struct setup
    {
        enum : u8
        {
            IDX_HIPS,
            IDX_LHAND,
            IDX_RHAND,
            IDX_LFOOT,
            IDX_RFOOT,
        };

        static constexpr i16 trajectory_index = 1;
        static constexpr i16 joint_indices[] =
        {
            1, // hips
            12, // lhand
            36, // rhand
            60, // lfoot
            65, // rfoot
        };
        static constexpr char* joint_names[] =
        {
            "Hips",
            "LeftHand",
            "RightHand",
            "LeftFoot",
            "RightFoot"
        };
    };
    common::AssetFileHelper<ANIMClip> _clip_file;
    common::AssetFileHelper<ANIMSkel> _skel_file;

    ANIMSimplePlayer _player;
    array_t<ANIMJoint> _joints_ms;
    array_t<mat44_t> _matrices_ms;
};
static Anim g_anim = {};

using Vec3Array = eastl::vector<vec3_t, bx_container_allocator>;
using ANIMJointArray = eastl::vector<ANIMJoint, bx_container_allocator>;
struct Controller
{
    struct Input
    {
        f32 forward = 0.f;
        f32 backward = 0.f;
        f32 left = 0.f;
        f32 right = 0.f;
    } _input;

    vec3_t _up_dir = vec3_t::ay();
    vec3_t _fw_dir = vec3_t::az();
    vec3_t _side_dir = vec3_t::ax();

    f32 _leg_phase[2] = {};
    vec3_t _leg_pos[2] = {};
    vec3_t _leg_wheels[2] = {};
    vec3_t _hips_pos = vec3_t(0.f);

    f32 _max_length_between_legs = 0.88f;
    f32 _rest_pose_length_between_legs = _max_length_between_legs * 0.025f;
    f32 _leg_length = .99f;
    f32 _speed = 2.f;
    f32 _leg_elevation = 0.2f;
    f32 _blend_rc = 0.1f;

    u32 _flag_current_leg : 1;
    u32 _flag_leg_histeresis : 1;


    ANIMJointArray _curr_pose;
    ANIMJointArray _tmp_joints;
    ANIMJointArray _tmp_joints1;

    ANIMSimplePlayer _player;
    ANIMJointArray _joints_ms;
    u32 _prev_best_frame = UINT32_MAX;
    f32 _prev_best_time = 0.f;

    void Init( const vec3_t& point_between_legs, const vec3_t& facing, BXIAllocator* allocator )
    {
        const f32 space_between_legs = _max_length_between_legs * 0.5f;
        const f32 leg_to_center_length = space_between_legs * 0.5f;
        const f32 hips_height = ::sqrtf( sqr( _leg_length ) - sqr( leg_to_center_length ) );

        const vec3_t side = cross( _up_dir, facing );

        _hips_pos = point_between_legs + _up_dir * hips_height;
        _leg_pos[0] = point_between_legs + side * leg_to_center_length;
        _leg_pos[1] = point_between_legs - side * leg_to_center_length;

        _fw_dir = facing;
        _side_dir = side;
        _flag_current_leg = 0;
        _flag_leg_histeresis = 0;


        _player.Prepare( g_anim._skel_file.data, allocator );
        _joints_ms.resize( g_anim._skel_file.data->numJoints );
    }

    void CollectInput( const BXInput& input, f32 dt )
    {
        Input raw;
        raw.forward  = input.IsKeyPressed( 'W' );
        raw.backward = input.IsKeyPressed( 'S' );
        raw.left     = input.IsKeyPressed( 'A' );
        raw.right    = input.IsKeyPressed( 'D' );

        const f32 rc = 0.1f;
        _input.forward  = LowPassFilter( raw.forward , _input.forward , rc, dt );
        _input.backward = LowPassFilter( raw.backward, _input.backward, rc, dt );
        _input.left     = LowPassFilter( raw.left    , _input.left    , rc, dt );
        _input.right    = LowPassFilter( raw.right   , _input.right   , rc, dt );
    }
    void Tick( f32 dt )
    {
        const f32 fwd_force = _input.forward - _input.backward;
        const f32 side_force = _input.left - _input.right;
        const f32 speed = _speed;

        vec3_t& active_leg = _leg_pos[_flag_current_leg];
        vec3_t& passive_leg = _leg_pos[!_flag_current_leg];

        vec3_t force_dir = (_fw_dir * fwd_force) + (_side_dir * side_force);
        f32 force_mag = min_of_2( 1.f, normalized( &force_dir ) );

        _leg_phase[_flag_current_leg] += dt;
        active_leg += force_dir * force_mag * speed * dt;

        { // try to keep legs close in YZ plane when moving
            const vec3_t center = (active_leg + passive_leg) * 0.5f;
            const vec3_t to_leg = active_leg - center;

            f32 projection = dot( force_dir, to_leg );
            vec3_t point_on_path = force_dir * projection;
            RDIXDebug::AddSphere( point_on_path + center, 0.1f );

            vec3_t attraction_dir = point_on_path - to_leg;
            f32 displ = normalized( &attraction_dir );


            f32 factor = displ - (_rest_pose_length_between_legs * 0.5f);
            factor *= dt * 15.f;
            active_leg += attraction_dir * factor;
        }

        if( force_mag > FLT_EPSILON )
        {
            const vec4_t yz_plane = MakePlane( normalize( cross( _up_dir, force_dir ) ), _hips_pos );
            const vec3_t active_leg_on_YZ_plane = ProjectPointOnPlane( active_leg, yz_plane );
            const vec3_t passive_leg_on_YZ_plane = ProjectPointOnPlane( passive_leg, yz_plane );
            RDIXDebug::AddSphere( active_leg_on_YZ_plane, 0.025f );
            RDIXDebug::AddSphere( passive_leg_on_YZ_plane, 0.025f );

            const vec3_t to_active_vec = active_leg_on_YZ_plane - passive_leg_on_YZ_plane;
            const f32 leg_distance = length( to_active_vec );
            if( (leg_distance > _max_length_between_legs) && !_flag_leg_histeresis )
            {
                _flag_current_leg = !_flag_current_leg;
                _flag_leg_histeresis = 1;
            }
            
            if( _flag_leg_histeresis )
            {
                _flag_leg_histeresis = leg_distance > (_max_length_between_legs * 0.75f);
            }
            
        }
        

        {
            const f32 leg_distance = length( active_leg - passive_leg );
            const vec3_t center = (active_leg + passive_leg) * 0.5f;
            const f32 hip_height = ::sqrtf( sqr( _leg_length ) - sqr( leg_distance * 0.5f ) );
            _hips_pos = center + _up_dir * hip_height;
        }

        {
            for( u32 j = 0; j < 2; ++j )
                _leg_wheels[j] = _leg_pos[j];

            const vec3_t center = (active_leg + passive_leg) * 0.5f;
            const vec3_t to_active_leg = _leg_pos[_flag_current_leg] - center;
            const vec4_t plane = MakePlane( _side_dir, center );

            const vec3_t to_active_on_plane = ProjectVectorOnPlane( to_active_leg, plane );
            const f32 len = length( to_active_on_plane );
            const f32 phase_raw = len / (_max_length_between_legs * 0.5f);
            const f32 phase_dir = dot( to_active_on_plane, _fw_dir ) >= 0.f ? 1.f : -1.f;
            const f32 phase = phase_raw * phase_dir;
            const f32 phase01 = phase * 0.5f + 0.5f;

            //const f32 func1 = phase01 * phase01 * phase01 * phase01 * 0.7f;
            const f32 func1 = smoothstep( 0.f, 1.f, phase01 ) * 0.5f;
            const f32 func2 = 1.f - smoothstep( 0.9f, 1.f, phase01 );

            const f32 y = func1 * func2;

            _leg_wheels[_flag_current_leg] = _leg_pos[_flag_current_leg];
            _leg_wheels[_flag_current_leg].y += y * force_mag * _leg_elevation;


        }

        if( g_anim._clip_file.data )
        {
            const vec3_t center = (active_leg + passive_leg) * 0.5f;
            const xform_t basis = xform_t( quat_t( mat33_t( _side_dir, _up_dir, _fw_dir ) ), center );

            const vec3_t in_hips_local  = xform_inv_point( basis, _hips_pos );
            const vec3_t in_lfoot_local = xform_inv_point( basis, _leg_wheels[0] );
            const vec3_t in_rfoot_local = xform_inv_point( basis, _leg_wheels[1] );


            const ANIMClip* clip = g_anim._clip_file.data;
            const u32 n_frames = clip->numFrames;
            const u32 n_joints = clip->numJoints;
            const f32 sample_dt = 1.f / clip->sampleFrequency;

            _tmp_joints.resize( n_joints );
            _tmp_joints1.resize( n_joints );

            u32 best_frame = UINT32_MAX;
            f32 best_score = FLT_MAX;
            f32 best_time = FLT_MAX;

            const vec4_t xz_plane = MakePlane( _up_dir, vec3_t( 0.f ) );

            const u16* parent_indices = (u16*)ParentIndices( g_anim._skel_file.data );

            const u32 steps = n_frames; // *10;

            for( u32 iframe = 0; iframe < steps; ++iframe )
            {
                const f32 sample_phase = (f32)iframe / f32(steps - 1);
                const f32 sample_time = sample_phase * clip->duration;
                EvaluateClip( _tmp_joints.data(), clip, sample_time );
                LocalJointsToWorldJoints( _tmp_joints1.data(), _tmp_joints.data(), parent_indices, n_joints, ANIMJoint::identity() );

                const u32 hips_index = Anim::setup::joint_indices[Anim::setup::IDX_HIPS];
                const u32 lfoot_index = Anim::setup::joint_indices[Anim::setup::IDX_LFOOT];
                const u32 rfoot_index = Anim::setup::joint_indices[Anim::setup::IDX_RFOOT];
            
                const vec3_t hips_pos  = _tmp_joints1[hips_index].position.xyz();
                const vec3_t lfoot_pos = _tmp_joints1[lfoot_index].position.xyz();
                const vec3_t rfoot_pos = _tmp_joints1[rfoot_index].position.xyz();

                const f32 hips_diff  = length( in_hips_local - hips_pos );
                const f32 lfoot_diff = length( in_lfoot_local - lfoot_pos );
                const f32 rfoot_diff = length( in_rfoot_local - rfoot_pos );

                const f32 score = lfoot_diff + rfoot_diff; // +hips_diff;
                
                if( score < best_score )
                {
                    best_score = score;
                    best_frame = iframe;
                    best_time = sample_time;
                }
            }

            if( best_frame != UINT32_MAX )
            {
                const u32 prev_frame = (_prev_best_frame < best_frame) ? _prev_best_frame : best_frame;//(best_frame > 0) ? best_frame - 1 : best_frame;
                const u32 next_frame = (_prev_best_frame > best_frame) ? _prev_best_frame : best_frame; //(best_frame < n_frames) ? best_frame + 1 : best_frame;

                //if( prev_frame != next_frame )
                {
                    modulo!!!
                    const f32 begin_time = (_prev_best_time < best_time ) ? _prev_best_time : best_time; // prev_frame * sample_dt;
                    const f32 end_time = (_prev_best_time > best_time ) ? _prev_best_time : best_time; // next_frame * sample_dt;
                    const f32 range = end_time - begin_time;
                    const f32 step = range * 0.05f;

                    if( step >= 2 * FLT_EPSILON )
                    {

                        for( f32 t = begin_time; t <= end_time; t += step )
                        {
                            EvaluateClip( _tmp_joints.data(), clip, t );
                            LocalJointsToWorldJoints( _tmp_joints1.data(), _tmp_joints.data(), parent_indices, n_joints, ANIMJoint::identity() );

                            const u32 hips_index = Anim::setup::joint_indices[Anim::setup::IDX_HIPS];
                            const u32 lfoot_index = Anim::setup::joint_indices[Anim::setup::IDX_LFOOT];
                            const u32 rfoot_index = Anim::setup::joint_indices[Anim::setup::IDX_RFOOT];

                            const vec3_t hips_pos = _tmp_joints1[hips_index].position.xyz();
                            const vec3_t lfoot_pos = _tmp_joints1[lfoot_index].position.xyz();
                            const vec3_t rfoot_pos = _tmp_joints1[rfoot_index].position.xyz();

                            const f32 hips_diff = length( in_hips_local - hips_pos );
                            const f32 lfoot_diff = length( in_lfoot_local - lfoot_pos );
                            const f32 rfoot_diff = length( in_rfoot_local - rfoot_pos );

                            const f32 score = lfoot_diff + rfoot_diff; // +hips_diff;

                            if( score < best_score )
                            {
                                best_score = score;
                                best_time = t;
                            }
                        }
                    }
                }

                
            }

            //if( _prev_best_frame != best_frame )
            //{
            //    if( _prev_best_frame > best_frame )
            //    {
            //        const f32 clip_dt = 1.f / clip->sampleFrequency;
            //        const f32 frame_time = best_frame / clip->sampleFrequency;
            //        _player.Play( clip, frame_time, clip_dt, 0 );
            //    }
            //}
            if( best_frame != UINT32_MAX )
            {
                EvaluateClip( _tmp_joints.data(), clip, best_time );
                LocalJointsToWorldJoints( _tmp_joints1.data(), _tmp_joints.data(), parent_indices, n_joints, toAnimJoint_noScale( basis ) );
            
                const u32 n_setup_joints = sizeof_array( Anim::setup::joint_indices );

                if( _curr_pose.empty() )
                {
                    _curr_pose = _tmp_joints1;
                }
                else
                {
                    const f32 rc = _blend_rc;
                    for( u32 ijoint = 0; ijoint < n_joints; ++ijoint )
                    {
                        ANIMJoint& curr_joint = _curr_pose[ijoint];
                        const ANIMJoint& next_joint = _tmp_joints1[ijoint];
            
                        bool found = false;
                        for( u32 i = 0; i < n_setup_joints && !found; ++i )
                        {
                            found = ijoint == Anim::setup::joint_indices[i];
                        }

                        curr_joint.position = LowPassFilter( next_joint.position, curr_joint.position, rc, dt );
                        curr_joint.rotation = LowPassFilter( next_joint.rotation, curr_joint.rotation, rc, dt );
                        curr_joint.scale = LowPassFilter( next_joint.scale, curr_joint.scale, rc, dt );
            
                        curr_joint.rotation = normalize( curr_joint.rotation );
                    }
                }
            
                auto joints_span = array_span_t<const ANIMJoint>( _curr_pose.data(), n_joints );
                anim_debug::DrawPose( g_anim._skel_file.data, joints_span, 0xFF6666FF, 0.05f );
            }

            _prev_best_frame = best_frame;
            _prev_best_time = best_time;
            //if( best_frame != _prev_best_frame )
            //    _player.Play( clip, frame_time, 0.1f, 0 );
            
            //_player.Play( clip, best_time, 0.1f, 0 );

            //_prev_best_frame = best_frame;
            //_player.Tick( dt );

            //const ANIMJoint* local_joints = _player.LocalJoints();
            //LocalJointsToWorldJoints( _joints_ms.data(), local_joints, parent_indices, n_joints, toAnimJoint_noScale( basis ) );

            //auto joints_span = array_span_t<const ANIMJoint>( _joints_ms.data(), n_joints );
            //anim_debug::DrawPose( g_anim._skel_file.data, joints_span, 0xFF6666FF, 0.05f );
        }

    }

    void DebugDraw()
    {

        RDIXDebug::AddSphere( _leg_pos[0], 0.1f, RDIXDebugParams( 0xFF0000FF ) );
        RDIXDebug::AddSphere( _leg_pos[1], 0.1f, RDIXDebugParams( 0x0000FFFF ) );
        RDIXDebug::AddSphere( _hips_pos, 0.1f, RDIXDebugParams( 0x00FF00FF ) );

        RDIXDebug::AddSphere( _leg_wheels[0], 0.11f, RDIXDebugParams( 0x00ffFFFF ) );
        RDIXDebug::AddSphere( _leg_wheels[1], 0.11f, RDIXDebugParams( 0x00ffFFFF ) );

        RDIXDebug::AddLine( _leg_pos[0], _hips_pos, { 0x666666FF } );
        RDIXDebug::AddLine( _leg_pos[1], _hips_pos, { 0x666666FF } );
        RDIXDebug::AddLine( _leg_pos[0], _leg_pos[1], { 0x666666FF } );

        const vec3_t legs_center = (_leg_pos[0] + _leg_pos[1]) * 0.5f;
        RDIXDebug::AddAxes( mat44_t( mat33_t( _side_dir, _up_dir, _fw_dir ), legs_center ) );

        {
            const vec3_t& base = _hips_pos;
            RDIXDebug::AddLine( base, base + _fw_dir * _input.forward, { 0x000000FF } );
            RDIXDebug::AddLine( base, base - _fw_dir * _input.backward, { 0x000000FF } );
            RDIXDebug::AddLine( base, base + _side_dir * _input.left, { 0x000000FF } );
            RDIXDebug::AddLine( base, base - _side_dir * _input.right, { 0x000000FF } );
        }


        if( ImGui::Begin( "AnimCtrl" ) )
        {
            ImGui::SliderFloat( "speed", &_speed, 0.f, 10.f );
            ImGui::SliderFloat( "leg_elev", &_leg_elevation, 0.f, 1.f );
            ImGui::SliderFloat( "leg_dist", &_max_length_between_legs, 0.f, 2.f );
            ImGui::SliderFloat( "rc", &_blend_rc, 0.001f, 1.f );
        }
        ImGui::End();

    }
};

static Controller g_controller = {};


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

    g_idcamera = gfx->CreateCamera( "main", GFXCameraParams(), mat44_t( mat33_t::identity(), vec3_t( 0.15f, 1.8f, 3.15f ) ) );

    g_anim._clip_file.LoadSync( filesystem, allocator, "anim/walk.clip" );
    g_anim._skel_file.LoadSync( filesystem, allocator, "anim/human.skel" );

    g_anim._player.Prepare( g_anim._skel_file.data, allocator );
    array::resize( g_anim._joints_ms, g_anim._skel_file.data->numJoints );
    array::resize( g_anim._matrices_ms, g_anim._skel_file.data->numJoints );

    g_anim._player.Play( g_anim._clip_file.data, 0.f, 0.f, 0 );

    g_controller.Init( vec3_t( -2.f, 0.f, 0.f ), vec3_t::az(), allocator );

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
        g_controller.CollectInput( win->input, delta_time_sec );
        g_controller.Tick( delta_time_sec );
        g_controller.DebugDraw();
    }

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
        RDIXDebug::AddAxes( mat44_t::identity() );
        RDIXDebug::Flush( rdicmdq, viewproj );
    }

    gfx->RasterizeFramebuffer( rdicmdq, 1, g_idcamera );

    GUI::Draw();

    gfx->EndFrame( frame_ctx );

    return true;
}

BX_APPLICATION_PLUGIN_DEFINE( test_app, BXTestApp );


/*
using Vector2Array = eastl::vector<vec2_t, bx_container_allocator>;
using Vector2Span = eastl::span<const vec2_t>;
namespace
{
    bool LineSegmentIntersection(
        vec2_t* output, f32* t,
        vec2_t A, vec2_t B,
        vec2_t C, vec2_t D )
    {
        f32  distAB, theCos, theSin, newX, ABpos;

        //  Fail if either line segment is zero-length.
        if( A.x == B.x && A.y == B.y || C.x == D.x && C.y == D.y ) return false;

        //  Fail if the segments share an end-point.
        if( A.x == C.x && A.y == C.y || B.x == C.x && B.y == C.y
            || A.x == D.x && A.y == D.y || B.x == D.x && B.y == D.y ) {
            return false;
        }

        //  (1) Translate the system so that point A is on the origin.
        B.x -= A.x; B.y -= A.y;
        C.x -= A.x; C.y -= A.y;
        D.x -= A.x; D.y -= A.y;

        //  Discover the length of segment A-B.
        distAB = sqrt( B.x*B.x + B.y * B.y );

        //  (2) Rotate the system so that point B is on the positive X axis.
        theCos = B.x / distAB;
        theSin = B.y / distAB;
        newX = C.x * theCos + C.y * theSin;
        C.y = C.y * theCos - C.x * theSin; C.x = newX;
        newX = D.x * theCos + D.y * theSin;
        D.y = D.y * theCos - D.x * theSin; D.x = newX;

        //  Fail if segment C-D doesn't cross line A-B.
        if( C.y < 0. && D.y < 0. || C.y >= 0. && D.y >= 0. ) return false;

        //  (3) Discover the position of the intersection point along line A-B.
        ABpos = D.x + (C.x - D.x)*D.y / (D.y - C.y);

        //  Fail if segment C-D crosses line A-B outside of segment A-B.
        if( ABpos<0. || ABpos>distAB ) return false;

        //  (4) Apply the discovered position to line A-B in the original coordinate system.
        output->x = A.x + ABpos * theCos;
        output->y = A.y + ABpos * theSin;
        t[0] = ABpos / distAB;

        //  Success.
        return true;
    }
    bool PointInPolygon( Vector2Span polygon, const vec2_t point )
    {
        const u32 polyCount = polygon.size();
        bool  oddNodes = false;

        u32 i, j = polyCount - 1;
        for( i = 0; i < polyCount; i++ )
        {
            const vec2_t poly_i = polygon[i];
            const vec2_t poly_j = polygon[j];

            if( (poly_i.y < point.y && poly_j.y >= point.y ||
                poly_j.y < point.y && poly_i.y >= point.y) &&
                (poly_i.x <= point.x || poly_j.x <= point.x)
                )
            {
                oddNodes ^= (poly_i.x + (point.y - poly_i.y) / (poly_j.y - poly_i.y)*(poly_j.x - poly_i.x) < point.x);
            }
            j = i;
        }

        return oddNodes;
    }

    struct SegmentVsPoly
    {
        vec2_t point = vec2_t( 0.f );
        f32 segment_t = FLT_MAX;
        u32 poly_segment_index = UINT32_MAX;
        u32 result = 0;
    };
    bool Intersect_SegmentVsPoly( SegmentVsPoly* output, const vec2_t& a0, const vec2_t& a1, Vector2Span poly, f32 tMin = 0.f )
    {
        output[0] = {};

        const u64 n = poly.size();
        for( u64 i = 0; i < n; ++i )
        {
            const vec2_t b0 = poly[i];
            const vec2_t b1 = poly[(i + 1) % n];

            f32 factor = FLT_MAX;
            vec2_t int_point;
            if( LineSegmentIntersection( &int_point, &factor, a0, a1, b0, b1 ) )
            {
                if( factor < output->segment_t && factor > tMin )
                {
                    output->point = int_point;
                    output->segment_t = factor;
                    output->poly_segment_index = i;
                    output->result += 1;
                }
            }
        }

        return output->result;
    }

    f32 DistanceSqr_PointSegment( const vec2_t& p, const vec2_t& s0, const vec2_t& s1 )
    {
        const vec2_t vs = s1 - s0;
        const vec2_t vp = p - s0;
        const f32 d = dot( normalize(vs), vp );

        if( d <= 0.f )
        {
            return length_sqr( vp );
        }

        if( d*d >= dot( vs, vs ) )
        {
            return length_sqr( p - s1 );
        }

        const vec2_t point_on_s = normalize( vs ) * d;
        return length_sqr( point_on_s - vp );
    }

    inline u32 NextIndex( u32 current, u32 size )
    {
        return (current + 1) % size;
    }

    inline void AppendPoint( Vector2Array& shape, const vec2_t& p )
    {
        shape.push_back( p );
        RDIXDebug::AddSphere( vec3_t( p.x, 0.5f, p.y ), 0.05f, { 0x00FFFFFF } );
    }
}




static Vector2Array ShapeUnion( Vector2Span shapeA, Vector2Span shapeB )
{
    u32 begin_index = UINT32_MAX;

    for( u64 i = 0; i < shapeA.size(); ++i )
    {
        if( !PointInPolygon( shapeB, shapeA[i] ) )
        {
            begin_index = i;
            break;
        }
    }

    Vector2Array outShape;
    if( begin_index == UINT32_MAX )
    {
        for( auto v : shapeB )
            outShape.push_back( v );

        return outShape;
    }
    const u32 sizeA = shapeA.size();
    const u32 sizeB = shapeB.size();

    u32 iA = begin_index;
    vec2_t base = shapeA[iA];
    AppendPoint( outShape, base );

    u32 safe = 0;

    do
    {
        iA = NextIndex( iA, sizeA );

        const vec2_t nextA = shapeA[iA];
        SegmentVsPoly testA;
        if( Intersect_SegmentVsPoly( &testA, base, nextA, shapeB, 0.01f ) )
        {
            AppendPoint( outShape, testA.point );

            base = testA.point;
            u32 iB = testA.poly_segment_index;
            do
            {
                iB = NextIndex( iB, sizeB );

                SegmentVsPoly testB;
                Intersect_SegmentVsPoly( &testB, base, shapeB[iB], shapeA, 0.01f );
                if( testB.result )
                {
                    AppendPoint( outShape, testB.point );
                    base = testB.point;
                    iA = testB.poly_segment_index;
                    break;
                }

                base = shapeB[iB];
                AppendPoint( outShape, base );

            } while ( true );
        }
        else
        {
            AppendPoint( outShape, nextA );
            base = nextA;
        }
    } while ( iA != begin_index && ++safe < 100 );


    return outShape;
}


void DebugDrawShape( Vector2Span poly, f32 y, color32_t color, const vec3_t& offset = vec3_t(0.f) )
{
    RDIXDebugParams params( color );

    for( u32 i = 0; i < poly.size(); ++i )
    {
        const vec2_t& poly0 = poly[i];
        const vec2_t& poly1 = poly[(i + 1) % poly.size()];
        const vec3_t a = vec3_t( poly0.x, y, poly0.y ) + offset;
        const vec3_t b = vec3_t( poly1.x, y, poly1.y ) + offset;

        RDIXDebug::AddLine( a, b, params );
    }
}
void DebugDrawPoint( const vec2_t p, f32 radius, f32 y, color32_t color )
{
    RDIXDebug::AddSphere( vec3_t( p.x, y, p.y ), radius, RDIXDebugParams( color ) );
}


   {
        Vector2Array shapeA =
        {
            vec2_t( -1.f, 1.f ),
            vec2_t( -0.9f, 1.f ),

            vec2_t( -0.9f,-0.9f ),
            vec2_t( -0.8f,-0.9f ),
            vec2_t( -0.8f, 1.f ),

            vec2_t(  1.f, 1.f ),
            vec2_t(  1.f, 0.9f ),
            vec2_t(  0.1f,  0.9f ),



            vec2_t(  1.f,-1.f ),
            vec2_t( -1.f,-1.f ),
        };

        static eastl::array<vec2_t, 4> shapeB =
        {
            vec2_t( -1.5f , 0.5f ),
            vec2_t( 1.5f, 0.5f ),
            vec2_t( 1.5f,-0.5f ),
            vec2_t( 0.f ,-0.5f ),
        };

        DebugDrawShape( shapeA, 0.5f, 0xFF0000FF );
        DebugDrawShape( shapeB, 0.5f, 0x00FF00FF );

        Vector2Array shapeC = ShapeUnion( shapeA, shapeB );

        static vec3_t shapeCOffset = vec3_t::ax() * 3.f;
        DebugDrawShape( shapeC, 0.5f, 0xFFFF00FF, shapeCOffset );

        static vec2_t point = vec2_t( 0.f );
        if( PointInPolygon( shapeA, point ) )
        {
            DebugDrawPoint( point, 0.1f, 0.5f, 0xFF0000FF );
        }
        else
        {
            DebugDrawPoint( point, 0.1f, 0.5f, 0x0000FFFF );
        }

        vec2_t int_point;
        f32 int_t;
        if( LineSegmentIntersection( &int_point, &int_t, shapeA[0], shapeA[1], vec2_t( 0.f ), point ) )
        {
            DebugDrawPoint( int_point, 0.1f, 0.5f, 0xFFFFFFFF );
        }

        static int test_point_index = 0;
        if( ImGui::Begin( "Poly" ) )
        {
            ImGui::SliderInt( "test_point_index", &test_point_index, 0, shapeB.size()-1 );
            ImGui::SliderFloat2( "test_point", shapeB[test_point_index].xy, -2.f, 2.f );

            ImGui::SliderFloat3( "shapeCoffset", shapeCOffset.xyz, -5.f, 5.f );
        }
        ImGui::End();
    }

*/