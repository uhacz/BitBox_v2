#pragma once

#include <foundation/type.h>
#include <foundation/math/vmath_type.h>

struct BXIAllocator;

struct ANIMJoint;
struct ANIMSkel;
struct ANIMClip;
struct ANIMContext;

struct ANIMCascadePlayer
{
    enum {
        eMAX_NODES = 2,
    };

    struct Node
    {
        const ANIMClip* clip = nullptr;

        /// for leaf
        uint64_t clip_user_data = 0;
        float clip_eval_time = 0.f;

        /// for branch
        uint32_t next = UINT32_MAX;
        float blend_time = 0.f;
        float blend_duration = 0.f;

        bool isLeaf() const { return next == UINT32_MAX; }
        bool isEmpty() const { return clip == nullptr; }
    };

    BXIAllocator* _allocator = nullptr;
    ANIMContext* _ctx = nullptr;
    Node _nodes[eMAX_NODES];
    uint32_t _root_node_index = UINT32_MAX;

    void prepare( const ANIMSkel* skel, BXIAllocator* allcator = nullptr );
    void unprepare();

    bool play( const ANIMClip* clip, float startTime, float blendTime, uint64_t userData, bool replaceLastIfFull );
    void tick( float deltaTime );

    bool empty() const { return _root_node_index == UINT32_MAX; }
    const ANIMJoint* localJoints() const;
    ANIMJoint*       localJoints();
    bool userData( uint64_t* dst, uint32_t depth );

private:
    void _Tick_processBlendTree();
    void _Tick_updateTime( float deltaTime );

    uint32_t _AllocateNode();
};

//////////////////////////////////////////////////////////////////////////
struct ANIMSimplePlayer
{
    enum {
        eMAX_NODES = 1,
    };

    struct Clip
    {
        const ANIMClip* clip = nullptr;
        uint64_t user_data = 0;
        float eval_time = 0.f;
    };

    BXIAllocator* _allocator = nullptr;
    ANIMContext* _ctx = nullptr;
    ANIMJoint* _prev_joints = nullptr;
    Clip _clips[2];
    float _blend_time = 0.f;
    float _blend_duration = 0.f;
    u32 _num_clips = 0;
    u32 _num_joints = 0;

    void Prepare( const ANIMSkel* skel, BXIAllocator* allcator = nullptr );
    void Unprepare();

    void Play( const ANIMClip* clip, float startTime, float blendTime, uint64_t userData );
    void Tick( float deltaTime );

    bool Empty() const { return _num_clips == 0; }
    const ANIMJoint* LocalJoints() const;
    ANIMJoint*       LocalJoints();
    vec3_t           GetRootTranslation() const;
    vec3_t           GetRootVelocity( float dt ) const;

    const ANIMJoint* PrevLocalJoints() const;
    ANIMJoint*       PrevLocalJoints();
    bool UserData( uint64_t* dst, uint32_t depth );
    bool EvalTime( float* dst, uint32_t depth );
    bool Duration( float* dst, uint32_t depth );
    bool BlendAlpha( float* dst );

private:
    static void _ClipUpdateTime( Clip* clip, float deltaTime );
    static float _ClipPhase( const Clip& clip );
    void _Tick_processBlendTree();
    void _Tick_updateTime( float deltaTime );
};
