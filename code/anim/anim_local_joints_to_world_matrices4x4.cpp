#include "anim.h"
#include <foundation/debug.h>
#include <foundation/math/vec4.h>
#include <foundation/math/mat44.h>

void LocalJointsToWorldMatrices4x4( mat44_t* out_matrices, const ANIMJoint* in_joints, const uint16_t* parent_indices, uint32_t count, const ANIMJoint& root_joint )
{
    mat44_t root = mat44_t( root_joint.rotation, root_joint.position );
    root.c0 *= root_joint.scale.x;
    root.c1 *= root_joint.scale.y;
    root.c2 *= root_joint.scale.z;


    mat44_t* out_transform = (mat44_t*)out_matrices;

    for( unsigned i = 0; i < count; ++i )
    {
        const uint32_t parent_idx = parent_indices[i];
        const bool is_root = parent_idx == 0xffff;

        const mat44_t& parent = ( is_root ) ? root : out_transform[parent_idx];
        
        const ANIMJoint& local_joint = in_joints[i];
        vec4_t scale_compensate = ( is_root ) ? root_joint.scale : in_joints[parent_idx].scale;
        scale_compensate = recip_per_elem( scale_compensate );

        // Compute Output Matrix
        // worldMatrix = worldParent * localTranslate * localScaleCompensate * localRotation * localScale

        const vec4_t local_translation = local_joint.position;

        mat44_t world(
            parent.c0 * scale_compensate.x,
            parent.c1 * scale_compensate.y,
            parent.c2 * scale_compensate.z,
            ( parent * local_translation )
            );

        mat44_t local_rotation_scale( local_joint.rotation );
        local_rotation_scale.c0 *= local_joint.scale.x;
        local_rotation_scale.c1 *= local_joint.scale.y;
        local_rotation_scale.c2 *= local_joint.scale.z;

        world *= local_rotation_scale;
        out_transform[i] = mat44_t( world.upper3x3(), world.translation() );

        //Transform3 local( local_joint.rotation, local_joint.position );
        //local.setCol0( local.getCol0() * local_joint.scale.getX() );
        //local.setCol1( local.getCol1() * local_joint.scale.getY() );
        //local.setCol2( local.getCol2() * local_joint.scale.getZ() );

        //out_transform[i] = parent * local;
    }
}
