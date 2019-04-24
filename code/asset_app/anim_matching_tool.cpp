#include "anim_matching_tool.h"
#include <3rd_party/imgui/imgui.h>
#include <foundation/array.h>

#include "common/common.h"
#include "anim/anim_struct.h"
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

namespace setup
{
    const i16 trajectory_index = 1;
    const i16 joint_indices[] =
    {
        1, // hips
        12, // lhand
        36, // rhand
        60, // lfoot
        66, // rfoot
    };
    const char* joint_names[] = 
    {
        "Hips",
        "LeftHand",
        "RightHand",
        "LeftFoot",
        "RightFoot"
    };
}//

void ANIMMatchingTool::StartUp( CMNEngine* e, const char* src_folder, BXIAllocator* allocator )
{
    _allocator = allocator;

    _db = anim_match::CreateDatabase( allocator );
    

    anim_match::LoadSkel( _db, e->filesystem, "anim/human.skel" );
    anim_match::LoadClip( _db, e->filesystem, "anim/yelling.clip" );
    //anim_match::LoadClip( _db, e->filesystem, "anim/Catwalk_Sequence_01.clip" );
    //anim_match::LoadClip( _db, e->filesystem, "anim/Catwalk_Walk_Forward_01.clip" );
    //anim_match::LoadClip( _db, e->filesystem, "anim/Catwalk_Walk_Forward_Arc_90L.clip" );
    //anim_match::LoadClip( _db, e->filesystem, "anim/Catwalk_Walk_Forward_Arc_90R.clip" );

    _ctx = anim_match::CreateContext( _db->skel, _allocator );
}

void ANIMMatchingTool::ShutDown( CMNEngine* e )
{
    anim_match::DestroyDatabase( &_db );
    anim_match::DestroyContext( &_ctx );
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
                    anim_match::LoadSkel( _db, e->filesystem, selected.pointer );
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

        if( _db->skel )
        {
            const char* names = TYPE_OFFSET_GET_POINTER( const char, _db->skel->offsetJointNamesStrings );
            if( names )
            {
                if( ImGui::TreeNodeEx( "Joints", ImGuiTreeNodeFlags_Framed ) )
                {
                    const char* current = names;
                    for( u32 i = 0; i < _db->skel->numJoints; ++i )
                    {
                        ImGui::TreeNode( current );
                        current += strlen( current ) + 1;
                    }
                    ImGui::TreePop();
                }
                
            }
        }
        if( _db->clip_metadata.size )
        {
            if( ImGui::TreeNodeEx( "Clips", ImGuiTreeNodeFlags_Framed ) )
            {
                for( u32 i = 0; i < _db->clip_metadata.size; ++i )
                {
                    const ANIMatchDatabase::ClipMetadata& clip = _db->clip_metadata[i];
                    if( ImGui::Selectable( clip.filename.AbsolutePath(), i == _selected_clip ) )
                    {
                        _selected_clip = i;
                    }
                }
                ImGui::TreePop();
            }
        }

        if( HasSelectedClip() )
        {
            ImGui::Separator();
            ImGui::BeginChild("Selected Clip", ImVec2(), true );
            //ImGui::SliderFloat( "Phase", &_selected_phase, 0.f, 1.f, "%.4f" );
            
            {
                const ANIMClip* clip = _db->clip_metadata[_selected_clip].file->data<ANIMClip>();
                ImGui::SliderInt( "Frame", &_selected_frame, 0, clip->numFrames );
            }
            ImGui::Checkbox( "Isolate frame", &_isolate_frame );
            
            
            const u32 nb_joints = (u32)sizeof_array( setup::joint_names );
            const char* current = "all";
            if( _selected_joint != UINT32_MAX )
            {
                for( u32 i = 0; i < nb_joints; ++i )
                {
                    if( setup::joint_indices[i] == _selected_joint )
                    {
                        current = setup::joint_names[i];
                        break;
                    }
                }
            }
            if( ImGui::BeginCombo( "Joint", current ) )
            {
                if( ImGui::Selectable( "none", _selected_joint == UINT32_MAX ) )
                {
                    _selected_joint = UINT32_MAX;
                }
                
                for( u32 ijoint = 0; ijoint < nb_joints; ++ijoint )
                {
                    const u32 index = setup::joint_indices[ijoint];
                    if( ImGui::Selectable( setup::joint_names[ijoint], _selected_joint == index ) )
                    {
                        _selected_joint = index;
                    }
                }
                ImGui::EndCombo();
            }
            ImGui::EndChild();
        }
        if( ImGui::BeginChild( "Controller", ImVec2(), true ) )
        {
            ImGui::SliderFloat3( "dir", _controller.input_dir.xyz, -1.f, 1.f );
            ImGui::SliderFloat( "speed", &_controller.input_speed, 0.f, 10.f );
        }
        ImGui::EndChild();

    }
    ImGui::End();

    const mat44_t entity_pose = common::GetEntityRootTransform( e->ecs, ctx.entity );
    TickController();
    DrawController( entity_pose );

    anim_match::Update( _db );
    anim_match::Update( _ctx, _db, _controller.vel, dt );

    if( HasSelectedClip() )
    {
        anim_match::DebugDraw( entity_pose, _db, _selected_clip, _selected_joint, _selected_frame, _isolate_frame );
    }

    if( !_ctx->player.Empty() )
    {
        const ANIMJoint* local_joints = _ctx->player.LocalJoints();
        array::resize( _ctx->world_joints, _ctx->player._num_joints );
    
        const ANIMJoint root_joint = toAnimJoint_noScale( entity_pose );
        const u16* parent_indices = (u16*)ParentIndices( _db->skel );

        LocalJointsToWorldJoints( &_ctx->world_joints[0], local_joints, parent_indices, _ctx->player._num_joints, root_joint );
        anim_debug::DrawPose( _db->skel, to_array_span( _ctx->world_joints ), color32_t::AQUA(), 0.15f );
    }



}

void ANIMMatchingTool::LoadAnimation( CMNEngine* e, const char* filename )
{
    anim_match::LoadClip( _db, e->filesystem, filename );
}

bool ANIMMatchingTool::HasSelectedClip() const
{
    return _selected_clip < _db->clip_metadata.size;
}

void ANIMMatchingTool::TickController()
{
    const f32 input_mag = length( _controller.input_dir );
    const f32 input_mag_rcp = (input_mag > FLT_EPSILON) ? 1.f / input_mag : 0.f;

    _controller.vel = _controller.input_dir * (_controller.input_speed * input_mag_rcp);
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
#include <foundation/array.h>
namespace anim_match
{
    template<typename T, typename ... TArgs>
    T* CreateAndStartUp( BXIAllocator* allocator, TArgs&& ... args )
    {
        T* ptr = BX_NEW( allocator, T );
        ptr->_allocator = allocator;
        StartUp( ptr, std::forward<TArgs>( args )... );
        return ptr;
    }
    template< typename T >
    void ShutDownAndDestroy( T** h )
    {
        if( !h[0] )
            return;

        ShutDown( h[0] );
        BXIAllocator* allocator = h[0]->_allocator;
        BX_DELETE0( allocator, h[0] );
    }

static void StartUp( ANIMAtchContext* ctx, const ANIMSkel* skel )
{
    ctx->current_clip = ctx->CLIP_NONE;
    ctx->current_entry_index = ctx->ENTRY_NONE;
    
    ctx->player.Prepare( skel, ctx->_allocator );
}
static void ShutDown( ANIMAtchContext* ctx )
{
    ctx->player.Unprepare();
}

static void StartUp( ANIMatchDatabase* db )
{}
static void ShutDown( ANIMatchDatabase* db )
{
    {
        BX_DELETE0( db->_allocator, db->skel_metadata.file );
        db->skel = nullptr;
    }

    while( !array::empty( db->clip_metadata ) )
    {
        ANIMatchDatabase::ClipMetadata& md = array::back( db->clip_metadata );
        BX_DELETE0( db->_allocator, md.file );

        array::pop_back( db->clip_metadata );
    }
}

ANIMAtchContext* CreateContext( const ANIMSkel* skel, BXIAllocator* allocator )
{
    return CreateAndStartUp<ANIMAtchContext>( allocator, skel );
}

ANIMatchDatabase* CreateDatabase( BXIAllocator* allocator )
{
    return CreateAndStartUp<ANIMatchDatabase>( allocator );
}

void DestroyContext( ANIMAtchContext** hctx )
{
    ShutDownAndDestroy( hctx );
}

void DestroyDatabase( ANIMatchDatabase** hdb )
{
    ShutDownAndDestroy( hdb );
}

void LoadSkel( ANIMatchDatabase* db, BXIFilesystem* fs, const char* filename )
{
    common::AssetFileHelper<ANIMSkel> fhelper;
    if( fhelper.LoadSync( fs, db->_allocator, filename ) )
    {
        if( db->skel )
        {
            BX_FREE0( db->_allocator, db->skel_metadata.file );
            db->skel = nullptr;
        }

        db->skel = fhelper.data;
        //string::create( &db->skel_metadata.filename, filename, db->_allocator );
        db->skel_metadata.file = (srl_file_t*)fhelper.file;
    }
}

template< typename T, typename U >
static inline T ToEntry( const U& value, f32 phase, u32 frame, u16 clip, u16 joint )
{
    T result;
    result.value = value;
    result.phase = phase;
    result.frame = frame;
    result.clip_index = clip;
    result.joint_index = joint;
    return result;
}

void Differentiate( array_t<ANIMatchEntry3>& dst, const array_span_t<const ANIMatchEntry3> src, u32 nb_frames, f32 sample_frequency )
{
    array::reserve( dst, dst.size + nb_frames );

    for( u32 iframe = 0; iframe < nb_frames - 1; ++iframe )
    {
        const ANIMatchEntry3& entry0 = src[iframe];
        const ANIMatchEntry3& entry1 = src[iframe + 1];

        const vec3_t diff = (entry1.value - entry0.value) * sample_frequency;
        ANIMatchEntry3 diff_entry = entry0;
        diff_entry.value = diff;
        array::push_back( dst, diff_entry );
    }
    {
        ANIMatchEntry3 last = array::back( dst );
        last.phase = 1.0f;
        array::push_back( dst, last );
    }
}

using JointArray = array_t<ANIMJoint>;
void AnalizeClip( ANIMatchDatabase* db, const ANIMClip* clip, u32 clip_index )
{
    const u32 nb_joints = (u32)sizeof_array( setup::joint_indices );
    const u16* parent_indices = TYPE_OFFSET_GET_POINTER( u16, db->skel->offsetParentIndices );
    
    const vec4_t ground_plane = MakePlane( vec3_t::ay(), vec3_t( 0.f ) );
    u32 curr_pose_index = 0;

    JointArray pose[2];
    JointArray scratch[2];

    array::resize( pose[0], clip->numJoints );
    array::resize( pose[1], clip->numJoints );
    array::resize( scratch[0], clip->numJoints );
    array::resize( scratch[1], clip->numJoints );

    for( u16 i = 0; i < clip->numJoints; ++i )
    {
        pose[0][i] = ANIMJoint::identity();
        pose[1][i] = ANIMJoint::identity();
        scratch[0][i] = ANIMJoint::identity();
        scratch[1][i] = ANIMJoint::identity();
    }

    ANIMJoint root_joint = ANIMJoint::identity();

    const u32 nb_frames = clip->numFrames;
    for( u32 iframe = 0; iframe < nb_frames; ++iframe )
    {
        const u32 prev_pose_index = curr_pose_index;
        curr_pose_index = !curr_pose_index;

        const JointArray& prev_pose = pose[prev_pose_index];
        JointArray& curr_pose = pose[curr_pose_index];

        const JointArray& prev_scratch = scratch[prev_pose_index];
        JointArray& curr_scratch = scratch[curr_pose_index];

        const f32 phase = (f32)iframe / (f32)(nb_frames - 1);

        const ANIMJoint& prev_0_joint = prev_pose[setup::joint_indices[0]];
        root_joint.position -= vec4_t( ProjectPointOnPlane( prev_0_joint.position.xyz(), ground_plane ), 1.f );

        EvaluateClip( &curr_scratch[0], clip, iframe, 0.f );
        LocalJointsToWorldJoints( &curr_pose[0], &curr_scratch[0], parent_indices, clip->numJoints, root_joint );

        for( u32 ijoint = 0; ijoint < nb_joints; ++ijoint )
        {
            const u16 index = setup::joint_indices[ijoint];
            const ANIMJoint& j = curr_pose[index];
            ANIMatchEntry3 pos_entry = ToEntry<ANIMatchEntry3>( j.position.xyz(), phase, iframe, clip_index, index );
            ANIMatchEntry4 rot_entry = ToEntry<ANIMatchEntry4>( j.rotation.to_vec4(), phase, iframe, clip_index, index );

            array::push_back( db->pos, pos_entry );
            array::push_back( db->rot, rot_entry );
        }
    }

    // velocties
    {
        array_t<ANIMatchEntry3> tmp;
        for( u32 ijoint = 0; ijoint < nb_joints; ++ijoint )
        {
            array::clear( tmp );

            const u16 index = setup::joint_indices[ijoint];
            root_joint = ANIMJoint::identity();

            pose[0][setup::joint_indices[0]] = ANIMJoint::identity();
            pose[1][setup::joint_indices[0]] = ANIMJoint::identity();

            const u32 vel_begin_index = db->trajectory_vel.size;
            for( u32 iframe = 0; iframe < nb_frames; ++iframe )
            {
                const u32 prev_pose_index = curr_pose_index;
                curr_pose_index = !curr_pose_index;

                const JointArray& prev_pose = pose[prev_pose_index];
                const JointArray& prev_scratch = scratch[prev_pose_index];

                JointArray& curr_pose = pose[curr_pose_index];
                JointArray& curr_scratch = scratch[curr_pose_index];

                //if( index != setup::trajectory_index )
                //{
                //    const ANIMJoint& prev_0_joint = prev_pose[setup::joint_indices[0]];
                //    root_joint.position -= vec4_t( ProjectPointOnPlane( prev_0_joint.position.xyz(), ground_plane ), 1.f );
                //}

                EvaluateClip( &curr_scratch[0], clip, iframe, 0.f, 0, index + 1 );
                LocalJointsToWorldJoints( &curr_pose[0], &curr_scratch[0], parent_indices, index + 1, root_joint );

                const ANIMJoint& joint = curr_pose[index];
                const vec3_t pos = joint.position.xyz();
                //if( index == setup::trajectory_index )
                //{
                //    pos = ProjectPointOnPlane( pos, ground_plane );
                //}
                const f32 phase = (f32)iframe / (f32)(nb_frames - 1);

                ANIMatchEntry3 entry = ToEntry<ANIMatchEntry3>( pos, phase, iframe, clip_index, index );
                array::push_back( tmp, entry );
            }

            array_span_t<const ANIMatchEntry3> pos_span = to_array_span( tmp );
            Differentiate( db->vel, pos_span, nb_frames, clip->sampleFrequency );
        }
    }

    { // trajectory
        array_t<ANIMatchEntry3> tmp;
        array::reserve( tmp, nb_frames );
        for( u32 iframe = 0; iframe < nb_frames; ++iframe )
        {
            const vec3_t root_translation = EvaluateRootTranslation( clip, iframe, 0.f );
            const f32 phase = (f32)iframe / (f32)(nb_frames - 1);

            ANIMatchEntry3 entry = ToEntry<ANIMatchEntry3>( root_translation, phase, iframe, clip_index, setup::trajectory_index );
            array::push_back( tmp, entry );
        }

        const u32 vel_begin_index = db->trajectory_vel.size;

        array_span_t<const ANIMatchEntry3> pos_span = to_array_span( tmp );
        Differentiate( db->trajectory_vel, pos_span, nb_frames, clip->sampleFrequency );

        array_span_t<const ANIMatchEntry3> vel_span = to_array_span( (const ANIMatchEntry3*)&db->trajectory_vel[vel_begin_index], nb_frames );
        Differentiate( db->trajectory_acc, vel_span, nb_frames, clip->sampleFrequency );
    }
}

void LoadClip( ANIMatchDatabase* db, BXIFilesystem* fs, const char* filename )
{
    if( !db->skel )
    {
        return;
    }

    common::AssetFileHelper<ANIMClip> fhelper;
    if( fhelper.LoadSync( fs, db->_allocator, filename ) )
    {
        const ANIMClip* clip = fhelper.data;
        if( clip->offsetRootTranslation )
        {
            ANIMatchDatabase::ClipMetadata metadata;
            metadata.filename.Append( filename );
            metadata.file = (srl_file_t*)fhelper.file;
            array::push_back( db->clip_metadata, std::move( metadata ) );

            u32 clip_index = array::push_back( db->clips, clip );
            AnalizeClip( db, clip, clip_index );
            db->_flags.sort_data = 1;
        }
        else
        {
            fhelper.FreeFile();
        }
    }
}


template< typename T >
static void SortEntries( T& t )
{
    std::sort( array::begin( t ), array::end( t ), []( const T::type_t& a, const T::type_t& b )
    {
        const u64 hash_a = (u64)a.clip_index << 32 | (u64)a.joint_index;
        const u64 hash_b = (u64)b.clip_index << 32 | (u64)b.joint_index;
        if( hash_a == hash_b )
            return a.frame < b.frame;

        return hash_a < hash_b;
    } );
}

template< typename T, typename TEntry = T::type_t >
static const TEntry* FindBegin( const T& t, u32 clip_index )
{
    TEntry dummy;
    dummy.clip_index = clip_index;
    return std::lower_bound( array::begin( t ), array::end( t ), dummy, []( const TEntry& a, const TEntry& b ) 
    {
        return a.clip_index < b.clip_index;
    } );
}
template< typename T, typename TEntry = T::type_t >
static const TEntry* FindEnd( const T& t, u32 clip_index )
{
    TEntry dummy;
    dummy.clip_index = clip_index;
    return std::upper_bound( array::begin( t ), array::end( t ), dummy, []( const TEntry& a, const TEntry& b )
    {
        return a.clip_index < b.clip_index;
    } );
}


void Update( ANIMatchDatabase* db )
{
    if( db->_flags.sort_data )
    {
        db->_flags.sort_data = 0;
        SortEntries( db->trajectory_vel );
        SortEntries( db->trajectory_acc );
        SortEntries( db->pos );
        SortEntries( db->rot );
        SortEntries( db->vel );
    }
}

void Update( ANIMAtchContext* ctx, const ANIMatchDatabase* db, const vec3_t& velocity, float dt )
{
    f32 the_best_diff = FLT_MAX;
    u32 the_best_vel_index = ANIMAtchContext::ENTRY_NONE;

    const u32 nb_entries = db->trajectory_vel.size;
    for( u32 i = 0; i < nb_entries; ++i )
    {
        const ANIMatchEntry3& e = db->trajectory_vel[i];
        const vec3_t diff = e.value - velocity;
        const f32 diff_len = length_sqr( diff );
        if( diff_len < the_best_diff && i != ctx->current_entry_index )
        {
            bool still_the_best = true;
            //if( e.clip_index == ctx->current_clip )
            //{
            //    still_the_best = i > ctx->current_entry_index;
            //}

            if( still_the_best )
            {
                the_best_diff = diff_len;
                the_best_vel_index = i;
            }
        }
    }

    if( the_best_vel_index == UINT32_MAX )
    {
        return;
    }

    const ANIMatchEntry3& the_best_entry_vel = db->trajectory_vel[the_best_vel_index];
    const u32 the_best_clip_index = the_best_entry_vel.clip_index;
    const ANIMClip* clip = db->clips[the_best_clip_index];

    const f32 new_clip_eval_time = the_best_entry_vel.phase * clip->duration;
    
    ctx->current_entry_index = the_best_vel_index;
    if( ctx->player.Empty() )
    {
        ctx->player.Play( clip, new_clip_eval_time, 0.1f, the_best_clip_index );
        ctx->current_clip = the_best_entry_vel.clip_index;
    }
    else
    {
        ANIMSimplePlayer& player = ctx->player;

        f32 curr_clip_eval_time0 = FLT_MAX;
        f32 curr_clip_eval_time1 = FLT_MAX;
        f32 time_diff_0 = FLT_MAX;
        f32 time_diff_1 = FLT_MAX;
        if( player.EvalTime( &curr_clip_eval_time0, 0 ) )
        {
            time_diff_0 = new_clip_eval_time - curr_clip_eval_time0;
        }
        if( player.EvalTime( &curr_clip_eval_time1, 1 ) )
        {
            time_diff_1 = new_clip_eval_time - curr_clip_eval_time1;
        }

        const f32 time_threshold = 0.2f;
        const bool the_same_clip = the_best_entry_vel.clip_index == ctx->current_clip;
        const bool the_same_time = ::fabsf( time_diff_0 ) < time_threshold;// || ::fabsf( time_diff_1 ) < time_threshold;
    
        const vec3_t anim_vel = ctx->player.GetRootVelocity( dt );
        const vec3_t entry_vel = the_best_entry_vel.value;
        const vec3_t vel_diff = entry_vel - anim_vel;

        const bool vel_match = length( vel_diff ) < 0.5f;

        //const float tmp = length( anim_vel ) / length( entry_vel );
        //const vec3_t anim_vel_diff = anim_vel - velocity;
        //const vec3_t entry_vel_diff = entry_vel - velocity;
        ////const bool vel_match = length( vel_diff ) < 0.666f;
        //const bool vel_match = tmp > 0.5f && tmp < 1.5f;
        bool should_change_clip = !(the_same_clip && vel_match);// || !the_same_time;
        if( should_change_clip )
        {
            player.Play( clip, new_clip_eval_time, 0.2f, the_best_clip_index );
            ctx->current_clip = the_best_clip_index;
        }
    }

    ctx->player.Tick( dt );
}

void DebugDraw( const mat44_t& basis, const ANIMatchDatabase* db, u32 clip_index, u32 joint_index, u32 clip_frame, bool isolate_frame )
{
    const ANIMClip* clip = db->clips[clip_index];
    const ANIMatchEntry3* pos_begin = FindBegin( db->pos, clip_index );
    const ANIMatchEntry3* pos_end = FindEnd( db->pos, clip_index );

    const ANIMatchEntry4* rot_begin = FindBegin( db->rot, clip_index );
    const ANIMatchEntry4* rot_end = FindEnd( db->rot, clip_index );

    const ANIMatchEntry3* vel_begin = FindBegin( db->vel, clip_index );
    const ANIMatchEntry3* vel_end = FindEnd( db->vel, clip_index );

    const ANIMatchEntry3* trajectory_vel_begin = FindBegin( db->trajectory_vel, clip_index );
    const ANIMatchEntry3* trajectory_vel_end = FindEnd( db->trajectory_vel, clip_index );

    const u32 nb_entries = (u32)(pos_end - pos_begin);
    SYS_ASSERT( nb_entries == (u32)(rot_end - rot_begin) );
    SYS_ASSERT( nb_entries == (u32)(vel_end - vel_begin) );

    const u32 nb_trajectory_entries = (u32)(trajectory_vel_end - trajectory_vel_begin);

    struct cfg_t
    {
        const color32_t base_color = 0xFFFFFFFF;
        const color32_t pos_color = 0x009900FF;
        const color32_t vel_color = 0x000099FF;
        const float sphere_min_radius = 0.01f;
        const float sphere_max_radius = 0.1f;
        const float axes_min_scale = 0.02f;
        const float axes_max_scale = 0.15f;
        const float line_min_scale = 0.01f;
        const float line_max_scale = 1.f;
    };
    const cfg_t cfg;

    //struct helper_t
    //{
    //    static float T( f32 clip_phase, f32 current_phase, f32 spike = 32.f )
    //    {
    //        float t = cubicpulse( clip_phase, 0.5f, current_phase );
    //        return pow( t, spike );
    //    }
    //};

    //u32 begin_entry = 0;
    //u32 end_entry = nb_entries;
    //if( isolate_frame )
    //{
    //    for( u32 i = 0; i < nb_entries; ++i )
    //    {
    //        if( pos_begin[i].frame == clip_frame )
    //        {
    //            begin_entry = i;
    //            end_entry = i + 1;
    //            break;
    //        }
    //    }
    //}

    for( u32 i = 0; i < nb_entries; ++i )
    {
        const ANIMatchEntry3* it_pos = pos_begin + i;
        const ANIMatchEntry4* it_rot = rot_begin + i;
        const ANIMatchEntry3* it_vel = vel_begin + i;

        if( isolate_frame && it_pos->frame != clip_frame )
            continue;

        if( joint_index != UINT32_MAX && joint_index != it_pos->joint_index )
            continue;

        //const f32 t_pos = helper_t::T( clip_phase, it_pos->phase, 64 );
        //const f32 t_rot = helper_t::T( clip_phase, it_rot->phase, 64 );
        //const f32 t_vel = helper_t::T( clip_phase, it_vel->phase, 64 );

        const f32 t_pos = (it_pos->frame == clip_frame) ? 1.f : 0.f;
        const f32 t_rot = (it_rot->frame == clip_frame) ? 1.f : 0.f;
        const f32 t_vel = (it_vel->frame == clip_frame) ? 1.f : 0.f;

        SYS_ASSERT( it_pos->joint_index == it_rot->joint_index );
        SYS_ASSERT( it_pos->joint_index == it_vel->joint_index );
        
        const float radius = lerp( t_pos, cfg.sphere_min_radius, cfg.sphere_max_radius );
        const color32_t color_pos = color32_t::lerp( t_pos, cfg.base_color, cfg.pos_color );

        const float axes_scale = lerp( t_rot, cfg.axes_min_scale, cfg.axes_max_scale );
        const mat44_t pose_local( quat_t( it_rot->value ), it_pos->value );

        const float vel_scale = lerp( t_vel, cfg.line_min_scale, cfg.line_max_scale );
        const color32_t color_vel = color32_t::lerp( t_vel, cfg.base_color, cfg.vel_color );

        const vec3_t pos = mul_as_point( basis, it_pos->value );
        const vec3_t vel = basis.upper3x3() * it_vel->value;
        const mat44_t pose = basis * pose_local;
        
        RDIXDebug::AddSphere( pos, radius, RDIXDebugParams( color_pos ) );
        RDIXDebug::AddAxes( pose, RDIXDebugParams().Scale( axes_scale ) );

        RDIXDebug::AddLine( pos, pos + vel * vel_scale, RDIXDebugParams( cfg.vel_color ) );
    }

    vec3_t local_prev_pos( 0.f );
    vec3_t local_curr_pos( 0.f );
    for( u32 i = 0; i < nb_trajectory_entries; ++i )
    {
        const ANIMatchEntry3* tr_vel = trajectory_vel_begin + i;
        const f32 t = (tr_vel->frame == clip_frame) ? 1.f : 0.f;

        const float radius = lerp( t, cfg.sphere_min_radius, cfg.sphere_max_radius );
        const color32_t color = color32_t::lerp( t, cfg.base_color, cfg.pos_color );

        local_prev_pos = local_curr_pos;
        local_curr_pos += tr_vel->value / clip->sampleFrequency;

        const vec3_t p0 = mul_as_point( basis, local_prev_pos );
        const vec3_t p1 = mul_as_point( basis, local_curr_pos );

        RDIXDebug::AddSphere( p1, radius, RDIXDebugParams( color ) );
        RDIXDebug::AddLine( p0, p1, RDIXDebugParams( cfg.vel_color ) );
    }

    //anim_debug::DrawPose( db->skel, 

}

}//