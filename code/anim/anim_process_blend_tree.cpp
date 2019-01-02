#include "anim.h"
#include <foundation/debug.h>
#include <string.h>

static inline void PushCmdLeaf( Cmd* cmdArray, uint8_t* cmdArraySize, uint16_t currentLeaf, const ANIMBlendLeaf* blendLeaves, uint32_t numLeaves, ANIMECmdOp::Enum op )
{
	const uint16_t leaf_index = currentLeaf & (~ANIMEBlendTreeIndex::LEAF);
	SYS_ASSERT( leaf_index < numLeaves );
	SYS_ASSERT( *cmdArraySize < ANIMContext::eCMD_ARRAY_SIZE );

	const ANIMBlendLeaf* leaf = blendLeaves + leaf_index;

	const uint8_t currentCmdArraySize = *cmdArraySize;
	cmdArray[currentCmdArraySize].leaf = leaf;
	cmdArray[currentCmdArraySize].command = op;
	*cmdArraySize = currentCmdArraySize + 1;
}
static inline void PushCmdBranch(  Cmd* cmdArray, uint8_t* cmdArraySize, const ANIMBlendBranch* branch, ANIMECmdOp::Enum op )
{
	SYS_ASSERT( *cmdArraySize < ANIMContext::eCMD_ARRAY_SIZE );
	SYS_ASSERT( op >= ANIMECmdOp::BLEND_STACK );

	const uint8_t currentCmdArraySize = *cmdArraySize;
	cmdArray[currentCmdArraySize].branch = branch;
	cmdArray[currentCmdArraySize].command = op;
	*cmdArraySize = currentCmdArraySize + 1;
}

static void TraverseBlendTree( Cmd* cmdArray, uint8_t* cmdArraySize, uint16_t currentBranch, const ANIMBlendBranch* blendBranches, uint32_t numBranches, const ANIMBlendLeaf* blendLeaves, uint32_t numLeaves )
{
	SYS_ASSERT( ( currentBranch & ANIMEBlendTreeIndex::BRANCH ) != 0 );

    const uint16_t branchIndex = currentBranch & ( ~ANIMEBlendTreeIndex::BRANCH );

	SYS_ASSERT( branchIndex < numBranches );

	const ANIMBlendBranch* branch = blendBranches + branchIndex;

    const uint16_t isLeftBranch  = branch->left  & ANIMEBlendTreeIndex::BRANCH;
    const uint16_t isLeftLeaf    = branch->left  & ANIMEBlendTreeIndex::LEAF;
    const uint16_t isRightBranch = branch->right & ANIMEBlendTreeIndex::BRANCH;
    const uint16_t isRightLeaf   = branch->right & ANIMEBlendTreeIndex::LEAF;

	SYS_ASSERT( isLeftBranch != isLeftLeaf );
	SYS_ASSERT( isRightBranch != isRightLeaf );

	if( isLeftBranch )
	{
		TraverseBlendTree( cmdArray, cmdArraySize, branch->left, blendBranches, numBranches, blendLeaves, numLeaves );
	}
	else if( isLeftLeaf )
	{
		const ANIMECmdOp::Enum op = ( isRightLeaf ) ? ANIMECmdOp::EVAL : ANIMECmdOp::PUSH_AND_EVAL;
		PushCmdLeaf( cmdArray, cmdArraySize, branch->left, blendLeaves, numLeaves, op );
	}
	else
	{
		SYS_ASSERT( false );
	}

	if( isRightBranch )
	{
		TraverseBlendTree( cmdArray, cmdArraySize, branch->right, blendBranches, numBranches, blendLeaves, numLeaves );
	}
	else if( isRightLeaf )
	{
        const ANIMECmdOp::Enum op = ( isLeftLeaf ) ? ANIMECmdOp::EVAL : ANIMECmdOp::PUSH_AND_EVAL;
		PushCmdLeaf( cmdArray, cmdArraySize, branch->right, blendLeaves, numLeaves, op );
	}
	else
	{
		SYS_ASSERT( false );
	}

    const ANIMECmdOp::Enum op = ( isLeftLeaf && isRightLeaf ) ? ANIMECmdOp::BLEND_CACHE : ANIMECmdOp::BLEND_STACK;
	PushCmdBranch( cmdArray, cmdArraySize, branch, op );

}

void EvaluateBlendTree( ANIMContext* ctx, const uint16_t root_index , const ANIMBlendBranch* blend_branches, uint32_t num_branches, const ANIMBlendLeaf* blend_leaves, uint32_t num_leaves )
{
	const uint16_t is_root_branch = root_index & ANIMEBlendTreeIndex::BRANCH;
    const uint16_t is_root_leaf   = root_index & ANIMEBlendTreeIndex::LEAF;

	SYS_ASSERT( is_root_leaf != is_root_branch );

	uint8_t cmd_array_size = 0;
	Cmd* cmd_array = ctx->cmdArray;
	memset( cmd_array, 0, sizeof(Cmd) * ANIMContext::eCMD_ARRAY_SIZE );

	if( is_root_leaf )
	{
        PushCmdLeaf( cmd_array, &cmd_array_size, root_index, blend_leaves, num_leaves, ANIMECmdOp::PUSH_AND_EVAL );
	}
	else
	{
		TraverseBlendTree( cmd_array, &cmd_array_size, root_index, blend_branches, num_branches, blend_leaves, num_leaves );
	}

	SYS_ASSERT( cmd_array_size < ANIMContext::eCMD_ARRAY_SIZE );

	Cmd* last_cmd = cmd_array + cmd_array_size;
    last_cmd->command = ANIMECmdOp::END_LIST;
	++cmd_array_size;

	ctx->cmdArraySize = cmd_array_size;
}

void EvaluateCommandList( ANIMContext* ctx )
{
    const Cmd* cmdList = ctx->cmdArray;
    ctx->poseStackIndex = UINT32_MAX;
	
    while( cmdList->command != ANIMECmdOp::END_LIST )
	{
		const uint16_t cmd = cmdList->command;

		switch( cmd )
		{
        case ANIMECmdOp::EVAL:
			{
				ANIMJoint* joints = AllocateTmpPose( ctx );
                ANIMClip* clip = (ANIMClip*)cmdList->leaf->anim;
				EvaluateClip( joints, clip, cmdList->leaf->evalTime );
				break;
			}
        case ANIMECmdOp::PUSH_AND_EVAL:
			{
				PoseStackPush( ctx );
				ANIMJoint* joints = PoseFromStack( ctx, 0 );
                ANIMClip* clip = (ANIMClip*)cmdList->leaf->anim;
				EvaluateClip( joints, clip, cmdList->leaf->evalTime );
				break;
			}
        case ANIMECmdOp::BLEND_STACK:
			{
				ANIMJoint* leftJoints  = PoseFromStack( ctx, 1 );
				ANIMJoint* rightJoints = PoseFromStack( ctx, 0 );
                //poseStackPush( ctx );
                ANIMJoint* outJoints = PoseFromStack( ctx, 1 );

				BlendJointsLinear( outJoints, leftJoints, rightJoints, cmdList->branch->alpha, ctx->numJoints );
                
                PoseStackPop( ctx );
				break;
			}
        case ANIMECmdOp::BLEND_CACHE:
			{
				ANIMJoint* leftJoints  = ctx->poseCache[0];
				ANIMJoint* rightJoints = ctx->poseCache[1];
				
				PoseStackPush( ctx );
                ANIMJoint* outJoints = PoseFromStack( ctx, 0 );

                BlendJointsLinear( outJoints, leftJoints, rightJoints, cmdList->branch->alpha, ctx->numJoints );
				break;
			}
		}
		++cmdList;
	}
}

