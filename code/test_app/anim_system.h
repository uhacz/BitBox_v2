#pragma once

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


template< u32 N >
struct IDAllocator
{
    static constexpr u32 INVALID = N;
    static constexpr u32 SIZE = N;

    eastl::bitset<N, u64> free_mask;

    IDAllocator()
    {
        free_mask.set();
    }

    u32 Allocate()
    {
        const u32 index = (u32)free_mask.find_first();
        if( index != INVALID )
        {
            free_mask.set( index, false );
        }
    }
    void Free( u32 index )
    {
        SYS_ASSERT( index < N );
        SYS_ASSERT( IsAlive( index ) );
        free_mask.set( index, true );
    }

    bool IsAlive( u32 index ) const { return !free_mask.test( index ); }
    bool IsDead ( u32 index ) const { return free_mask.test( index ); }
};

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
