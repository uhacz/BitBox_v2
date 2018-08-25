#pragma once

#include "anim_joint_transform.h"
#include "anim_struct.h"
#include "anim_common.h"

ANIMContext* ContextInit( const ANIMSkel& skel, BXIAllocator* allocator );
void ContextDeinit( ANIMContext** ctx );

void EvaluateBlendTree( ANIMContext* ctx, const uint16_t root_index , const ANIMBlendBranch* blend_branches, uint32_t num_branches, const ANIMBlendLeaf* blend_leaves, uint32_t num_leaves );
void EvaluateCommandList( ANIMContext* ctx );
void BlendJointsLinear( ANIMJoint* out_joints, const ANIMJoint* left_joints, const ANIMJoint* right_joints, float blend_factor, uint32_t num_joints);

void EvaluateClip( ANIMJoint* out_joints, const ANIMClip* anim, float eval_time, uint32_t beginJoint = UINT32_MAX, uint32_t endJoint = UINT32_MAX );
void EvaluateClip( ANIMJoint* out_joints, const ANIMClip* anim, uint32_t frame_integer, float frame_fraction, uint32_t beginJoint = UINT32_MAX, uint32_t endJoint = UINT32_MAX );

void EvaluateClipIndexed( ANIMJoint* out_joints, const ANIMClip* anim, float eval_time, const int16_t* indices, uint32_t numIndices );
void EvaluateClipIndexed( ANIMJoint* out_joints, const ANIMClip* anim, uint32_t frame_integer, float frame_fraction, const int16_t* indices, uint32_t numIndices );

void LocalJointsToWorldMatrices4x4( mat44_t* out_matrices, const ANIMJoint* in_joints, const uint16_t* parent_indices, uint32_t count, const ANIMJoint& root_joint );
void LocalJointsToWorldJoints( ANIMJoint* out_joints, const ANIMJoint* in_joints, const uint16_t* parent_indices, uint32_t count, const ANIMJoint& root_joint );


struct RSM;
namespace anim_ext {

   ANIMSkel* LoadSkelFromFile( RSM* resourceManager, const char* relativePath );
   ANIMClip* LoadAnimFromFile( RSM* resourceManager, const char* relativePath );

   void UnloadSkelFromFile( RSM* resourceManager, ANIMSkel** skel );
   void UnloadAnimFromFile( RSM* resourceManager, ANIMClip** clip );

   void LocalJointsToWorldJoints( ANIMJoint* outJoints, const ANIMJoint* inJoints, const ANIMSkel* skel, const ANIMJoint& rootJoint );
   void LocalJointsToWorldMatrices( mat44_t* outMatrices, const ANIMJoint* inJoints, const ANIMSkel* skel, const ANIMJoint& rootJoint );

   void ProcessBlendTree( ANIMContext* ctx, const uint16_t root_index, const ANIMBlendBranch* blend_branches, unsigned int num_branches, const ANIMBlendLeaf* blend_leaves, unsigned int num_leaves );
}