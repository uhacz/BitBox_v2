#pragma once

#include "debug.h"
#include "../foundation/type.h"
#include "../foundation/id.h"
#include "../foundation/eastl/span.h"
#include "../foundation/eastl/vector.h"
#include "../foundation/container_allocator.h"
#include "../anim/anim_joint_transform.h"
#include "../resource_manager/resource_manager.h"
#include "../foundation/eastl/bitset.h"
#include "eastl/fixed_vector.h"



struct BXIAllocator;


BX_DEFINE_ID( ANIMClipID, u32, 16 );
BX_DEFINE_ID( ANIMSkelID, u32, 16 );
BX_DEFINE_ID( ANIMPlayID, u32, 12 );
BX_DEFINE_ID( ANIMJointsID, u32, 16 );

using ANIMJointSpan = eastl::span<ANIMJoint>;
using ANIMJointArray = eastl::vector<ANIMJoint, bx_container_allocator>;

struct ANIM
{
    void StartUp( BXIAllocator* allocator );
    void ShutDown();

    ANIMSkelID LoadSkel( const char* anim_path );
    ANIMClipID LoadClip( const char* clip_path );

    void FreeClip( ANIMClipID id );
    void FreeSkel( ANIMSkelID id );

    bool IsAlive( ANIMClipID id );
    bool IsAlive( ANIMSkelID id );
    bool IsAlive( ANIMPlayID id );

    ANIMPlayID PlayClip( ANIMClipID clip, ANIMSkelID skel );
    void PauseClip     ( ANIMPlayID id, bool value );
    void StopClip      ( ANIMPlayID id );
    void SetTimeScale  ( ANIMPlayID id, f32 value );

    ANIMJointSpan ClipLocalJoints    ( ANIMClipID id );
    ANIMJointSpan ClipPrevLocalJoints( ANIMClipID id );
    ANIMJointSpan CLipModelJoints    ( ANIMClipID id );
    ANIMJointSpan ClipPrevModelJoints( ANIMClipID id );

    void UpdateLoading();
    void UpdateClips();
    void ComputePoses();     
   
    struct ClipContext
    {
        ANIMSkelID skel;
        ANIMClipID clip;
        f32 eval_time;
        f32 time_scale;

        union
        {
            u32 all;
            struct  
            {
                u32 paused : 1;
            };
        } flags;

        ANIMJointArray local_joints;
        ANIMJointArray model_joints;

        ANIMJointArray prev_local_joints;
        ANIMJointArray prev_model_joints;
    };

    BXIAllocator* _allocator = nullptr;

    using IDAllocatorSkel = IDAllocator<1 << 9>;
    using IDAllocatorClip = IDAllocator<1 << 12>;
    using IDAllocatorPlay = IDAllocator<1 << 8 >;

    using DeadMaskSkel = eastl::bitset<IDAllocatorSkel::SIZE>;
    using DeadMaskClip = eastl::bitset<IDAllocatorClip::SIZE>;
    
    using ContainerAllocator = bx_container_allocator;
    using GenerationArrayClip = eastl::fixed_vector<u16, IDAllocatorClip::SIZE>;
    using GenerationArraySkel = eastl::fixed_vector<u16, IDAllocatorSkel::SIZE>;
    using GenerationArrayPlay = eastl::fixed_vector<u16, IDAllocatorPlay::SIZE>;

    eastl::fixed_vector< RSMResourceID, IDAllocatorClip::SIZE > _clips;
    eastl::fixed_vector< RSMResourceID, IDAllocatorSkel::SIZE > _skels;
    eastl::fixed_vector< ClipContext  , IDAllocatorPlay::SIZE>  _plays;

    IDAllocatorSkel _id_alloc_skel;
    IDAllocatorClip _id_alloc_clip;
    IDAllocatorPlay _id_alloc_play;

    GenerationArrayClip _generation_clip;
    GenerationArraySkel _generation_skel;
    GenerationArrayPlay _generation_play;

    DeadMaskSkel _dead_skel;
    DeadMaskClip _dead_clip;
};


struct ANIMClip;
struct ANIMSkel;
struct ANIMBlend
{
    u32 left_index;
    u32 right_index;
    f32 alpha;
};

namespace anim_system
{
    using ClipSpan      = eastl::span<const ANIMClip*>;
    using SkelSpan      = eastl::span<const ANIMSkel*>;
    using InJointSpan   = eastl::span<const ANIMJoint>;
    using OutJointSpan  = eastl::span<ANIMJoint>;
    using OutMatrixSpan = eastl::span<mat44_t>;

    using InPoseSpan        = eastl::span<InJointSpan>;
    using OutPoseSpan       = eastl::span<OutJointSpan>;
    using OutMatrixPoseSpan = eastl::span<OutMatrixSpan>;

    using EvalTimeSpan  = eastl::span<f32>;
    using BlendSpan     = eastl::span<ANIMBlend>;

    void EvaluateAnimations    ( OutPoseSpan out_poses, ClipSpan clips, EvalTimeSpan eval_time );
    void BlendAnimations       ( OutPoseSpan out_poses, BlendSpan blends, InPoseSpan in_poses );
    void ComputeJoints         ( OutPoseSpan out_poses, InPoseSpan in_poses, SkelSpan skels, InJointSpan root_joints );
    void ComputeMatrices       ( OutMatrixPoseSpan out_poses, InPoseSpan in_poses, SkelSpan skels, InJointSpan root_joints );
    void ConvertJoints2Matrices( OutMatrixPoseSpan out_poses, InPoseSpan in_poses );
}//

namespace anim_system
{
    static constexpr u32 CLIP_CHUNK = 64;
    static constexpr u32 SKEL_CHUNK = 32;
    static constexpr u32 PLAY_CHUNK = 32;
    static constexpr u32 POSE_CHUNK = 32;
    
    struct PlayContext
    {
        ANIMSkelID skel;
        ANIMClipID clip;
        ANIMJointsID local_joints;
        ANIMJointsID model_joints;
        f32 eval_time;
        f32 time_scale;

        union
        {
            u32 all;
            struct
            {
                u32 paused : 1;
            };
        } flags;
    };

    using ClipContainer = eastl::fixed_vector<RSMResourceID, CLIP_CHUNK>;
    using SkelContainer = eastl::fixed_vector<RSMResourceID, SKEL_CHUNK>;
    using PoseContainer = eastl::fixed_vector<ANIMJointArray, POSE_CHUNK>;
    using PlayContainer = eastl::fixed_vector<PlayContext, PLAY_CHUNK>;
    using PlayIndexLookup = eastl::fixed_vector<u32, PLAY_CHUNK>;

    struct Data
    {
        SkelContainer _skel;
        ClipContainer _clip;
        PoseContainer _pose;
    };

    struct Context
    {
        PlayIndexLookup _index_lookup;
        PlayContainer _play;
    };


}//