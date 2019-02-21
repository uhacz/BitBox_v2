#pragma once

#include <foundation/type.h>
#include <foundation/debug.h>
#include <foundation/serializer.h>
#include <foundation/tag.h>

struct ANIMJoint;

struct BIT_ALIGNMENT_16 ANIMSkel
{
    static constexpr uint32_t VERSION = BX_UTIL_MAKE_VERSION( 1, 0, 0 );
    static constexpr uint32_t TAG = BX_UTIL_TAG32( 'S', 'K', 'E', 'L' );

	uint16_t numJoints;
	uint16_t pad0__[1];
	uint32_t offsetBasePose;
	uint32_t offsetParentIndices;
	uint32_t offsetJointNames;

    SRL_TYPE( ANIMSkel,
        SRL_PROPERTY( numJoints );
        SRL_PROPERTY( offsetBasePose );
        SRL_PROPERTY( offsetParentIndices );
        SRL_PROPERTY( offsetJointNames );
    );
};

inline const int16_t*   ParentIndices( const ANIMSkel* skel ) { return TYPE_OFFSET_GET_POINTER( int16_t, skel->offsetParentIndices ); }
inline const uint32_t*  JointNames   ( const ANIMSkel* skel ) { return TYPE_OFFSET_GET_POINTER( uint32_t, skel->offsetJointNames ); }
inline const ANIMJoint* BasePose     ( const ANIMSkel* skel ) { return TYPE_OFFSET_GET_POINTER( ANIMJoint, skel->offsetBasePose ); }


struct BIT_ALIGNMENT_16 ANIMClip
{
    static constexpr uint32_t VERSION = BX_UTIL_MAKE_VERSION( 1, 0, 0 );
    static constexpr uint32_t TAG = BX_UTIL_TAG32( 'C', 'L', 'I', 'P' );
    
	float32_t duration;
	float32_t sampleFrequency;
	uint16_t  numJoints;
	uint16_t  numFrames;
	uint32_t  offsetRotationData;
	uint32_t  offsetTranslationData;
	uint32_t  offsetScaleData;
    uint32_t  __padding[2];

    SRL_TYPE( ANIMClip,
        SRL_PROPERTY( duration );
        SRL_PROPERTY( sampleFrequency );
        SRL_PROPERTY( numJoints );
        SRL_PROPERTY( numFrames );
        SRL_PROPERTY( offsetRotationData );
        SRL_PROPERTY( offsetTranslationData );
        SRL_PROPERTY( offsetScaleData );
    );
};

struct BIT_ALIGNMENT_16 ANIMBlendBranch
{
	inline ANIMBlendBranch( uint16_t left_index, uint16_t right_index, float32_t blend_alpha, uint16_t f = 0 )
		: left( left_index ), right( right_index ), alpha( blend_alpha ), flags(f)
	{}
	
    inline ANIMBlendBranch()
		: left(0), right(0), flags(0), alpha(0.f)
	{}
	uint16_t left;
	uint16_t right;
	uint16_t flags;
	uint16_t pad0__[1];
	float32_t alpha;
	uint32_t pad1__[1];
};

struct BIT_ALIGNMENT_16 ANIMBlendLeaf
{
	inline ANIMBlendLeaf( const ANIMClip* clip, float32_t time )
		: anim( clip ), evalTime( time )
	{}
	inline ANIMBlendLeaf()
		: anim(0), evalTime(0.f)
	{}

	const ANIMClip* anim;
	float32_t evalTime;
	uint32_t pad0__[1];
};

namespace ANIMEBlendTreeIndex
{
    enum Enum
    {
        LEAF = 0x8000,
        BRANCH = 0x4000,
    };
};

namespace ANIMECmdOp
{
    enum Enum : uint16_t
    {
        END_LIST = 0,	/* end of command list marker */

        EVAL,			/* evaluate anim to top of pose cache*/
        PUSH_AND_EVAL,  /* evaluate anim to top of pose stack*/

        BLEND_STACK,	/* blend poses from stack*/
        BLEND_CACHE,	/* blend poses from cache */
    };
};


struct Cmd
{
    ANIMECmdOp::Enum command;		/* see AnimCommandOp */
	uint16_t arg0;			/* helper argument used by some commands */
	union
	{
		const ANIMBlendLeaf*	  leaf;
		const ANIMBlendBranch* branch;
	};
};

struct ANIMJoint;
struct BXIAllocator;
struct BIT_ALIGNMENT_16 ANIMContext
{
	enum
	{
		ePOSE_CACHE_SIZE = 2,
		ePOSE_STACK_SIZE = 4,
		eCMD_ARRAY_SIZE = 64,
	};

    BXIAllocator* allocator = nullptr;

	ANIMJoint* poseCache[ePOSE_CACHE_SIZE];
	ANIMJoint* poseStack[ePOSE_STACK_SIZE];
	Cmd* cmdArray = nullptr;
    uint32_t poseCacheIndex = 0;
    uint32_t poseStackIndex = 0;
    uint32_t cmdArraySize = 0;
    uint32_t numJoints = 0;
};
 
inline ANIMJoint* AllocateTmpPose( ANIMContext* ctx )
{
    const uint32_t current_index = ctx->poseCacheIndex;
    ctx->poseCacheIndex = ( current_index + 1 ) % ANIMContext::ePOSE_CACHE_SIZE;
    return ctx->poseCache[current_index];
}

inline void PoseStackPop( ANIMContext* ctx )
{
    SYS_ASSERT( ctx->poseStackIndex > 0 );
    --ctx->poseStackIndex;
}

inline uint32_t PoseStackPush( ANIMContext* ctx )
{
    ctx->poseStackIndex = ( ctx->poseStackIndex + 1 ) % ANIMContext::ePOSE_STACK_SIZE;
    return ctx->poseStackIndex;
}

/// depth: how many poses we going back in stack (stackIndex decrease)
inline uint32_t PoseStackIndex( const ANIMContext* ctx, int32_t depth )
{
    const int32_t iindex = (int32_t)ctx->poseStackIndex - depth;
    SYS_ASSERT( iindex < ANIMContext::ePOSE_STACK_SIZE );
    SYS_ASSERT( iindex >= 0 );
    //const uint8_t index = (uint8_t)iindex % bxAnim_Context::ePOSE_STACK_SIZE;
    return iindex;
}
inline ANIMJoint* PoseFromStack( const ANIMContext* ctx, int32_t depth )
{
    const uint32_t index = PoseStackIndex( ctx, depth );
    return ctx->poseStack[index];
}
