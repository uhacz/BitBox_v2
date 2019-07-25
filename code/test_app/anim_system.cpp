#include "anim_system.h"
#include "resource_manager/resource_manager.h"


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

}

ANIMJointSpan ANIM::ClipPrevLocalJoints( ANIMClipID id )
{

}

ANIMJointSpan ANIM::CLipModelJoints( ANIMClipID id )
{

}

ANIMJointSpan ANIM::ClipPrevModelJoints( ANIMClipID id )
{

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
