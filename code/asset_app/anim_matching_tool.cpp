#include "anim_matching_tool.h"
#include <3rd_party/imgui/imgui.h>
#include <foundation/array.h>

#include "common/common.h"
#include "anim/anim_struct.h"
#include "anim/anim_mmatch.h"
#include "common/base_engine_init.h"

#include <filesystem/filesystem_plugin.h>
#include "anim/anim_joint_transform.h"
#include "anim/anim.h"
#include "anim/anim_debug.h"
#include "foundation/math/vmath.h"
#include "foundation/math/math_common.h"
#include <algorithm>
#include "rdix/rdix_debug_draw.h"
#include "util/color.h"

void ANIMMatchingTool::StartUp( CMNEngine* e, const char* src_folder, BXIAllocator* allocator )
{
    _allocator = allocator;

    _db = anim_mmatch::CreateDatabase( allocator );
    _dbg = BX_NEW( _allocator, ANIMatchDatabaseDebugInfo );

    anim_mmatch::LoadSkel( _db, e->filesystem, "anim/human.skel" );
    anim_mmatch::LoadClip( _db, e->filesystem, "anim/yelling.clip" );
    //anim_match::LoadClip( _db, e->filesystem, "anim/Catwalk_Sequence_01.clip" );
    //anim_match::LoadClip( _db, e->filesystem, "anim/Catwalk_Walk_Forward_01.clip" );
    //anim_match::LoadClip( _db, e->filesystem, "anim/Catwalk_Walk_Forward_Arc_90L.clip" );
    //anim_match::LoadClip( _db, e->filesystem, "anim/Catwalk_Walk_Forward_Arc_90R.clip" );

    _ctx = anim_mmatch::CreateContext( _db->skel, _allocator );
}

void ANIMMatchingTool::ShutDown( CMNEngine* e )
{
    BX_DELETE0( _allocator, _dbg );
    anim_mmatch::DestroyDatabase( &_db );
    anim_mmatch::DestroyContext( &_ctx );
}

void ANIMMatchingTool::Tick( CMNEngine* e, const TOOLContext& ctx, float dt )
{
    if( ImGui::Begin( "Anim matching", nullptr, ImGuiWindowFlags_MenuBar ) )
    {
        if( ImGui::BeginMenuBar() )
        {
            if( ImGui::BeginMenu( "Load skeleton" ) )
            {
                string_buffer_it selected = common::MenuFileSelector( ctx.folders->anim.FileList() );
                if( selected.pointer )
                {
                    anim_mmatch::LoadSkel( _db, e->filesystem, selected.pointer );
                }
                ImGui::EndMenu();
            }

            if( ImGui::BeginMenu( "Load animation" ) )
            {
                string_buffer_it selected = common::MenuFileSelector( ctx.folders->anim.FileList() );
                if( !selected.null() )
                {
                    LoadAnimation( e, selected.pointer );
                }
                
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }

        anim_mmatch::DrawGUI( _dbg, _db );
        
        if( ImGui::BeginChild( "Controller", ImVec2(), true ) )
        {
            ImGui::SliderFloat3( "dir", _controller.input_dir.xyz, -1.f, 1.f );
            ImGui::SliderFloat( "speed", &_controller.input_speed, 0.f, 10.f );
        }
        ImGui::EndChild();

    }
    ImGui::End();

    const mat44_t entity_pose = common::GetEntityRootTransform( e->ecs, ctx.entity );
    TickController( dt );
    DrawController( entity_pose );

    vec3_t local_vel = rotate_inv( _controller.rot, _controller.input_dir );
    local_vel = normalize( local_vel ) * _controller.input_speed;

    anim_mmatch::Update( _db );
    anim_mmatch::Update( _ctx, _db, local_vel, dt );

    //if( HasSelectedClip() )
    {
        anim_mmatch::DebugDraw( entity_pose, _db, *_dbg );
    }

    if( !_ctx->player.Empty() )
    {
        const ANIMJoint* local_joints = _ctx->player.LocalJoints();
        array::resize( _ctx->world_joints, _ctx->player._num_joints );
    
        ANIMJoint root_joint = toAnimJoint_noScale( entity_pose );
        root_joint.rotation = _controller.rot;
        const u16* parent_indices = (u16*)ParentIndices( _db->skel );

        LocalJointsToWorldJoints( &_ctx->world_joints[0], local_joints, parent_indices, _ctx->player._num_joints, root_joint );
        anim_debug::DrawPose( _db->skel, to_array_span( _ctx->world_joints ), color32_t::AQUA(), 0.15f );
    }



}

void ANIMMatchingTool::LoadAnimation( CMNEngine* e, const char* filename )
{
    anim_mmatch::LoadClip( _db, e->filesystem, filename );
}

void ANIMMatchingTool::TickController( float dt )
{
    const f32 turn_speed = 4.f;

    const vec3_t input_dir = normalize( _controller.input_dir );
    const f32 input_mag = length( input_dir );
    const f32 input_mag_rcp = (input_mag > FLT_EPSILON) ? 1.f / input_mag : 0.f;

    const f32 lerp_t = 1 - ::powf( turn_speed, dt );

    const vec3_t dir = rotate( _controller.rot, vec3_t::az() );
    const vec3_t diff = input_dir - dir;
    const f32 scaler = length( diff ) * min_of_2( 1.f, dt * turn_speed );
    const vec3_t new_dir = dir + normalize(diff) * scaler;


    //const vec3_t new_dir = lerp( lerp_t, dir, _controller.input_dir );
    const quat_t drot = ShortestRotation( dir, new_dir );
    _controller.rot *= drot;

    //const vec3_t acc = _controller.input_dir * (_controller.input_speed * input_mag_rcp);
    //const vec3_t dir = rotate( _controller.rot, vec3_t::az() );
    //const vec3_t up = rotate( _controller.rot, vec3_t::ay() );
    //const vec3_t ang_momentum = up * length( _controller.input_dir - dir ) * (_controller.input_speed * input_mag_rcp);
    //const vec3_t ang_velocity = ang_momentum;

    //const quat_t drot = quat_t( ang_velocity, 0.f ) * _controller.rot * 0.5f * dt;
    //_controller.rot += drot;
    _controller.rot = normalize( _controller.rot );
    _controller.vel = rotate( _controller.rot, vec3_t::az() ) * (_controller.input_speed * input_mag);
}

void ANIMMatchingTool::DrawController( const mat44_t& basis )
{
    const vec3_t pos = basis.translation();
    RDIXDebug::AddSphere( pos, 0.1f );
    RDIXDebug::AddLine( pos, pos + _controller.vel, RDIXDebugParams( color32_t::RED() ) );
}

//
//
//
