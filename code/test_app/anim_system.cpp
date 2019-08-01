#include "anim_system.h"
#include "resource_manager/resource_manager.h"
#include "debug.h"
#include "anim/anim.h"


void ANIM::StartUp( BXIAllocator* allocator )
{
    _allocator = allocator;
}

void ANIM::ShutDown()
{
}

template< typename Tid, typename Tid_alloc, typename Tcontainer, typename Tgeneration>
bool LoadResource( Tid* id, Tid_alloc& id_alloc, Tcontainer& container, Tgeneration& generation, const char* path )
{
    RSMResourceID resource_id = RSM::Load( path );
    if( resource_id.i == 0 )
    {
        return false;
    }

    id->index = id_alloc.Allocate();
    id->generation = generation[id->index];

    container[id->index] = resource_id;

    return true;
}

ANIMSkelID ANIM::LoadSkel( const char* skel_path )
{
    ANIMSkelID id = ANIMSkelID::Null();
    LoadResource( &id, _id_alloc_skel, _skels, _generation_skel, skel_path );
    return id;
}

ANIMClipID ANIM::LoadClip( const char* clip_path )
{
    ANIMClipID id = ANIMClipID::Null();
    LoadResource( &id, _id_alloc_clip, _clips, _generation_clip, clip_path );
    return id;
}

void ANIM::FreeClip( ANIMClipID id )
{
    if( !IsAlive( id ) )
        return;

    _generation_clip[id.index] += 1;
    _dead_clip.set( id.index );
}

void ANIM::FreeSkel( ANIMSkelID id )
{
    if( !IsAlive( id ) )
        return;

    _generation_skel[id.index] += 1;
    _dead_skel.set( id.index );
}

template< typename Tid, typename Tid_alloc, typename Tgeneration>
inline bool IsAlive( const Tid& id, const Tid_alloc& alloc, const Tgeneration& generation )
{
    return alloc.IsAlive( id.index ) && generation[id.index] == id.generation;
}

bool ANIM::IsAlive( ANIMClipID id )
{
    return ::IsAlive( id, _id_alloc_clip, _generation_clip );
}

bool ANIM::IsAlive( ANIMSkelID id )
{
    return ::IsAlive( id, _id_alloc_skel, _generation_skel );
}

bool ANIM::IsAlive( ANIMPlayID id )
{
    return ::IsAlive( id, _id_alloc_play, _generation_play );
}

ANIMPlayID ANIM::PlayClip( ANIMClipID clip, ANIMSkelID skel )
{
    const u32 index = _id_alloc_play.Allocate();
    if( index == _id_alloc_play.INVALID )
        return ANIMPlayID::Null();

    ANIMPlayID id;
    id.index = index;
    id.generation = _generation_play[index];

    ClipContext& ctx = _plays[id.index];
    ctx.eval_time = 0.f;
    ctx.flags.all = 0;
    ctx.local_joints.clear();
    ctx.prev_local_joints.clear();
    ctx.model_joints.clear();
    ctx.prev_model_joints.clear();

    return id;
}

void ANIM::PauseClip( ANIMPlayID id, bool value )
{
    if( !IsAlive( id ) )
        return;

    _plays[id.index].flags.paused = value;
}

void ANIM::StopClip( ANIMPlayID id )
{
    if( !IsAlive( id ) )
        return;

    _generation_play[id.index] += 1;

    ClipContext& ctx = _plays[id.index];
    ctx.eval_time = 0.f;
    ctx.flags.all = 0;
    ctx.local_joints.clear();
    ctx.prev_local_joints.clear();
    ctx.model_joints.clear();
    ctx.prev_model_joints.clear();

    ctx.local_joints.shrink_to_fit();
    ctx.prev_local_joints.shrink_to_fit();
    ctx.model_joints.shrink_to_fit();
    ctx.prev_model_joints.shrink_to_fit();

    _id_alloc_play.Free( id.index );
}

void ANIM::SetTimeScale( ANIMPlayID id, f32 value )
{
    if( !IsAlive( id ) )
        return;

    _plays[id.index].time_scale = value;
}

ANIMJointSpan ANIM::ClipLocalJoints( ANIMClipID id )
{
    return {};
}

ANIMJointSpan ANIM::ClipPrevLocalJoints( ANIMClipID id )
{
    return {};
}

ANIMJointSpan ANIM::CLipModelJoints( ANIMClipID id )
{
    return {};
}

ANIMJointSpan ANIM::ClipPrevModelJoints( ANIMClipID id )
{
    return {};
}

void ANIM::UpdateLoading()
{

}

void ANIM::UpdateClips()
{

}

void ANIM::ComputePoses()
{

}

namespace anim_system
{
    void EvaluateAnimations( OutPoseSpan out_poses, ClipSpan clips, EvalTimeSpan eval_time )
    {
        SYS_ASSERT( out_poses.size() == clips.size() );
        SYS_ASSERT( out_poses.size() == eval_time.size() );

        const u64 n = out_poses.size();
        for( u64 i = 0; i < n; ++i )
        {
            OutJointSpan joints_ls = out_poses[i];
            const ANIMClip* clip = clips[i];
            const f32 time = eval_time[i];

            EvaluateClip( joints_ls.data(), clip, time );
        }
    }

    void BlendAnimations( OutPoseSpan out_poses, BlendSpan blends, InPoseSpan in_poses )
    {
        SYS_ASSERT( out_poses.size() == blends.size() );
        const u64 n = out_poses.size();
        for( u64 i = 0; i < n; ++i )
        {
            const ANIMBlend blend = blends[i];
            const InJointSpan left = in_poses[blend.left_index];
            const InJointSpan right = in_poses[blend.right_index];
            SYS_ASSERT( left.size() == right.size() );

            OutJointSpan blended_joints = out_poses[i];
            SYS_ASSERT( blended_joints.size() == left.size() );

            const u32 num_joints = (u32)blended_joints.size();
            BlendJointsLinear( blended_joints.data(), left.data(), right.data(), blend.alpha, num_joints );
        }
    }

    void ComputeJoints( OutPoseSpan out_poses, InPoseSpan in_poses, SkelSpan skels, InJointSpan root_joints )
    {
        SYS_ASSERT( out_poses.size() == in_poses.size() );
        SYS_ASSERT( out_poses.size() == skels.size() );

        const u64 n = out_poses.size();
        for( u64 i = 0; i < n; ++i )
        {
            const InJointSpan in_joints = in_poses[i];
            const ANIMSkel* skel = skels[i];
            const u16* parent_indices = (u16*)ParentIndices( skel );
            const ANIMJoint root = (i < (u64)root_joints.size()) ? root_joints[i] : ANIMJoint::identity();
            OutJointSpan out_joints = out_poses[i];

            SYS_ASSERT( out_joints.size() == in_joints.size() );
            SYS_ASSERT( out_joints.size() == skel->numJoints );

            const u32 num_joints = (u32)out_joints.size();
            LocalJointsToWorldJoints( out_joints.data(), in_joints.data(), parent_indices, num_joints, root );
        }
    }

    void ComputeMatrices( OutMatrixPoseSpan out_poses, InPoseSpan in_poses, SkelSpan skels, InJointSpan root_joints )
    {
        SYS_ASSERT( out_poses.size() == in_poses.size() );
        SYS_ASSERT( out_poses.size() == skels.size() );

        const u64 n = out_poses.size();
        for( u64 i = 0; i < n; ++i )
        {
            const InJointSpan in_joints = in_poses[i];
            const ANIMSkel* skel = skels[i];
            const u16* parent_indices = (u16*)ParentIndices( skel );
            const ANIMJoint root = (i < (u64)root_joints.size()) ? root_joints[i] : ANIMJoint::identity();
            OutMatrixSpan out_matrices = out_poses[i];

            SYS_ASSERT( out_matrices.size() == in_joints.size() );
            SYS_ASSERT( out_matrices.size() == skel->numJoints );

            const u32 num_joints = (u32)out_matrices.size();
            LocalJointsToWorldMatrices4x4( out_matrices.data(), in_joints.data(), parent_indices, num_joints, root );
        }
    }

    void ConvertJoints2Matrices( OutMatrixPoseSpan out_poses, InPoseSpan in_poses )
    {
        SYS_ASSERT( out_poses.size() == in_poses.size() );
        const u64 n = out_poses.size();
        for( u64 i = 0; i < n; ++i )
        {
            const InJointSpan in_joints = in_poses[i];
            OutMatrixSpan out_matrices = out_poses[i];

            SYS_ASSERT( in_joints.size() == out_matrices.size() );
            const u64 num_joints = out_matrices.size();
            for( u64 ijoint = 0; ijoint < num_joints; ++ijoint )
            {
                out_matrices[ijoint] = toMatrix4( in_joints[ijoint] );
            }
        }
    }
}//
