#include "anim_player.h"
#include "anim.h"

#include <memory/memory.h>
#include <foundation/common.h>

void ANIMCascadePlayer::prepare( const ANIMSkel* skel, BXIAllocator* allocator /*= nullptr */ )
{
    _allocator = allocator;
    _ctx = ContextInit( *skel, allocator );

}

void ANIMCascadePlayer::unprepare()
{
    ContextDeinit( &_ctx );
}

namespace
{
    static void makeLeaf( ANIMCascadePlayer::Node* node, const ANIMClip* clip, float startTime, uint64_t userData )
    {
        *node = {};
        node->clip = clip;
        node->clip_eval_time = startTime;
        node->clip_user_data = userData;
    }
    static void makeBranch( ANIMCascadePlayer::Node* node, uint32_t nextNodeIndex, float blendDuration )
    {
        node->next = nextNodeIndex;
        node->blend_time = 0.f;
        node->blend_duration = blendDuration;
    }
}

bool ANIMCascadePlayer::play( const ANIMClip* clip, float startTime, float blendDuration, uint64_t userData, bool replaceLastIfFull )
{
    uint32_t node_index = _AllocateNode();
    if( node_index == UINT32_MAX )
    {
        if( replaceLastIfFull )
        {
            node_index = _root_node_index;
            while( _nodes[node_index].next != UINT32_MAX )
                node_index = _nodes[node_index].next;
        }
        else
        {
            SYS_LOG_ERROR( "Cannot allocate node" );
            return false;
        }
    }

    if( _root_node_index == UINT32_MAX )
    {
        _root_node_index = node_index;
        _nodes[node_index] = {};

        Node& node = _nodes[node_index];
        makeLeaf( &node, clip, startTime, userData );
    }
    else
    {
        uint32_t last_node = _root_node_index;
        if( _nodes[last_node].next != UINT32_MAX )
        {
            last_node = _nodes[last_node].next;
        }

        if( last_node == node_index ) // when replaceLastIfFull == true and there is no space for new nodes
        {
            Node& node = _nodes[node_index];
            makeLeaf( &node, clip, startTime, userData );
        }
        else
        {
            Node& prev_node = _nodes[last_node];
            makeBranch( &prev_node, node_index, blendDuration );
        
            Node& node = _nodes[node_index];
            makeLeaf( &node, clip, startTime, userData );
        }
    }
    return true;
}

void ANIMCascadePlayer::tick( float deltaTime )
{
    if( _root_node_index == UINT32_MAX ) 
    {
        // no animations to play
        return;
    }

    _Tick_processBlendTree();
    _Tick_updateTime( deltaTime );    
}

const ANIMJoint* ANIMCascadePlayer::localJoints() const
{
    return PoseFromStack( _ctx, 0 );
}

ANIMJoint* ANIMCascadePlayer::localJoints()
{
    return PoseFromStack( _ctx, 0 );
}

bool ANIMCascadePlayer::userData( uint64_t* dst, uint32_t depth )
{
    uint32_t index = _root_node_index;
    while( index != UINT32_MAX )
    {
        const Node& node = _nodes[index];
        
        if( depth == 0 )
        {
            dst[0] = node.clip_user_data;
            return true;
        }
        depth -= 1;
        index = node.next;
    }

    return false;
}

void ANIMCascadePlayer::_Tick_processBlendTree()
{
    if( _nodes[_root_node_index].isLeaf() )
    {
        const Node& node = _nodes[_root_node_index];
        ANIMBlendLeaf leaf( node.clip, node.clip_eval_time );

        anim_ext::ProcessBlendTree( _ctx,
                                    0 | ANIMEBlendTreeIndex::LEAF,
                                    nullptr, 0,
                                    &leaf, 1
                                    );
    }
    else
    {
        ANIMBlendLeaf leaves[eMAX_NODES] = {};
        ANIMBlendBranch branches[eMAX_NODES] = {};

        uint32_t num_leaves = 0;
        uint32_t num_branches = 0;

        uint32_t node_index = _root_node_index;
        while( node_index != UINT32_MAX )
        {
            SYS_ASSERT( num_leaves < eMAX_NODES );
            SYS_ASSERT( num_branches < eMAX_NODES );

            const Node& node = _nodes[node_index];

            const uint32_t leaf_index = num_leaves++;
            ANIMBlendLeaf* leaf = &leaves[leaf_index];
            leaf[0] = ANIMBlendLeaf( node.clip, node.clip_eval_time );

            if( node.isLeaf() )
            {
                SYS_ASSERT( node.next == UINT32_MAX );
                SYS_ASSERT( num_branches > 0 );

                const uint32_t last_branch = num_branches - 1;
                ANIMBlendBranch* branch = &branches[last_branch];

                branch->right = (uint16_t)leaf_index | ANIMEBlendTreeIndex::LEAF;
            }
            else
            {
                ANIMBlendBranch* last_branch = nullptr;
                if( node_index != _root_node_index )
                {
                    const uint32_t last_branch_index = num_branches - 1;
                    last_branch = &branches[last_branch_index];
                    
                }

                const uint32_t branch_index = num_branches++;
                if( last_branch )
                {
                    last_branch->right = (uint16_t)branch_index | ANIMEBlendTreeIndex::BRANCH;
                }

                const float blend_alpha = min_of_2( 1.f, node.blend_time / node.blend_duration );
                ANIMBlendBranch* branch = &branches[branch_index];
                branch[0] = ANIMBlendBranch( (uint16_t)leaf_index | ANIMEBlendTreeIndex::LEAF, 0, blend_alpha );
            }

            node_index = node.next;
        }

        anim_ext::ProcessBlendTree( _ctx,
                                     0 | ANIMEBlendTreeIndex::BRANCH,
                                     branches, num_branches,
                                     leaves, num_leaves
                                    );


    }
}

namespace
{
    static inline void updateNodeClip( ANIMCascadePlayer::Node* node, float deltaTime )
    {
        node->clip_eval_time = ::fmodf( node->clip_eval_time + deltaTime, node->clip->duration );
    }
}

void ANIMCascadePlayer::_Tick_updateTime( float deltaTime )
{
    
    
    
    if( _root_node_index != UINT32_MAX && _nodes[_root_node_index].isLeaf() )
    {
        Node* node = &_nodes[_root_node_index];
        updateNodeClip( node, deltaTime );
    }
    else
    {
        const uint32_t node_index = _root_node_index;
        Node* node = &_nodes[node_index];
        
        const uint32_t next_node_index = node->next;
        Node* next_node = &_nodes[next_node_index];

        if( node->blend_time > node->blend_duration )
        {
            node[0] = *next_node;
            next_node[0] = {};
            updateNodeClip( node, deltaTime );
        }
        else
        {
            
            if( next_node->isLeaf() )
            {
                const ANIMClip* clipA = node->clip;
                const ANIMClip* clipB = next_node->clip;
                
                const float blend_alpha = min_of_2( 1.f, node->blend_time / node->blend_duration );
                const float clip_duration = lerp( blend_alpha, clipA->duration, clipB->duration );
                const float delta_phase = deltaTime / clip_duration;
                
                float phaseA = node->clip_eval_time / clipA->duration; //::fmodf( ( ) + delta_phase, 1.f );
                float phaseB = next_node->clip_eval_time / clipB->duration; //::fmodf( ( ) + delta_phase, 1.f );
                phaseA = ::fmodf( phaseA + delta_phase, 1.f );
                phaseB = ::fmodf( phaseB + delta_phase, 1.f );
                
                node->clip_eval_time = phaseA * clipA->duration;
                next_node->clip_eval_time = phaseB * clipB->duration;
            }
            else
            {
                updateNodeClip( node, deltaTime );
            }

            node->blend_time += deltaTime;
        }
    }
}

uint32_t ANIMCascadePlayer::_AllocateNode()
{
    uint32_t index = UINT32_MAX;
    for( uint32_t i = 0; i < eMAX_NODES; ++i )
    {
        if( _nodes[i].isEmpty() )
        {
            index = i;
            break;
        }
    }
    return index;
}


//////////////////////////////////////////////////////////////////////////

void ANIMSimplePlayer::prepare( const ANIMSkel* skel, BXIAllocator* allocator /*= nullptr */ )
{
    _allocator = allocator;
    _ctx = ContextInit( *skel, allocator );
    _prev_joints = (ANIMJoint*)BX_MALLOC( allocator, skel->numJoints * sizeof( ANIMJoint ), 16 );
    
    for( uint32_t i = 0; i < skel->numJoints; ++i )
    {
        _prev_joints[i] = ANIMJoint::identity();
    }
}


void ANIMSimplePlayer::unprepare()
{
    BX_FREE0( _allocator, _prev_joints );
    ContextDeinit( &_ctx );
}

void ANIMSimplePlayer::play( const ANIMClip* clip, float startTime, float blendTime, uint64_t userData )
{
    if( _num_clips == 2 )
        return;

    if( _num_clips == 0 )
    {
        Clip& c0 = _clips[0];
        c0.clip = clip;
        c0.eval_time = startTime;
        c0.user_data = userData;
        _num_clips = 1;
    }
    else
    {
        Clip& c1 = _clips[1];
        c1.clip = clip;
        c1.eval_time = startTime;
        c1.user_data = userData;
        _num_clips = 2;
    }

    _blend_time = 0.f;
    _blend_duration = blendTime;

}

void ANIMSimplePlayer::tick( float deltaTime )
{
    memcpy( _prev_joints, localJoints(), _ctx->numJoints * sizeof( ANIMJoint ) );
    _Tick_processBlendTree();
    _Tick_updateTime( deltaTime );
}

void ANIMSimplePlayer::_ClipUpdateTime( Clip* clip, float deltaTime )
{
    clip->eval_time = ::fmodf( clip->eval_time + deltaTime, clip->clip->duration );
}

float ANIMSimplePlayer::_ClipPhase( const Clip& clip )
{
    return clip.eval_time / clip.clip->duration;
}

void ANIMSimplePlayer::_Tick_processBlendTree()
{
    if( _num_clips == 0 )
    {
        return;
    }
    else if( _num_clips == 1 )
    {
        const Clip& c = _clips[0];

        ANIMBlendLeaf leaf( c.clip, c.eval_time );
        anim_ext::ProcessBlendTree( _ctx, 0 | ANIMEBlendTreeIndex::LEAF, nullptr, 0, &leaf, 1 );
    }
    else
    {
        const Clip& c0 = _clips[0];
        const Clip& c1 = _clips[1];

        ANIMBlendLeaf leaves[2] =
        {
            { c0.clip, c0.eval_time },
            { c1.clip, c1.eval_time },
        };

        const float blend_alpha = min_of_2( 1.f, _blend_time / _blend_duration );
        ANIMBlendBranch branch( 0 | ANIMEBlendTreeIndex::LEAF, 1 | ANIMEBlendTreeIndex::LEAF, blend_alpha );
        anim_ext::ProcessBlendTree( _ctx, 0 | ANIMEBlendTreeIndex::BRANCH, &branch, 1, leaves, 2 );
    }
}

void ANIMSimplePlayer::_Tick_updateTime( float deltaTime )
{
    if( _num_clips == 0 )
    {
 
    }
    else if( _num_clips == 1 )
    {
        _ClipUpdateTime( &_clips[0], deltaTime );
    }
    else
    {
        Clip& c0 = _clips[0];
        Clip& c1 = _clips[1];
        
        //_ClipUpdateTime( &_clips[0], deltaTime );
        //_ClipUpdateTime( &_clips[1], deltaTime );

        const ANIMClip* clipA = c0.clip;
        const ANIMClip* clipB = c1.clip;

        const float blend_alpha = min_of_2( 1.f, _blend_time / _blend_duration );
        const float clip_duration = lerp( blend_alpha, clipA->duration, clipB->duration );
        const float delta_phase = deltaTime / clip_duration;

        float phaseA = _ClipPhase( c0 );
        float phaseB = _ClipPhase( c1 );
        phaseA = ::fmodf( phaseA + delta_phase, 1.f );
        phaseB = ::fmodf( phaseB + delta_phase, 1.f );

        c0.eval_time = phaseA * clipA->duration;
        c1.eval_time = phaseB * clipB->duration;

        if( _blend_time > _blend_duration )
        {
            _clips[0] = _clips[1];
            _clips[1] = {};
            _num_clips = 1;
        }
        else
        {
            _blend_time += deltaTime;
        }
    }
}

bool ANIMSimplePlayer::userData( uint64_t* dst, uint32_t depth )
{
    if( _num_clips == 0 )
        return false;

    if( depth >= _num_clips )
        return false;

    dst[0] = _clips[depth].user_data;
    return true;
}

bool ANIMSimplePlayer::evalTime( float* dst, uint32_t depth )
{
    if( _num_clips == 0 )
        return false;

    if( depth >= _num_clips )
        return false;

    dst[0] = _clips[depth].eval_time;
    return true;
}

bool ANIMSimplePlayer::blendAlpha( float* dst )
{
    if( _num_clips != 2 )
        return false;

    dst[0] = _blend_time / _blend_duration;
    return true;
}

const ANIMJoint* ANIMSimplePlayer::localJoints() const
{
    return PoseFromStack( _ctx, 0 );
}

ANIMJoint* ANIMSimplePlayer::localJoints()
{
    return PoseFromStack( _ctx, 0 );
}

const ANIMJoint* ANIMSimplePlayer::prevLocalJoints() const
{
    return _prev_joints;
}

ANIMJoint* ANIMSimplePlayer::prevLocalJoints()
{
    return _prev_joints;
}


