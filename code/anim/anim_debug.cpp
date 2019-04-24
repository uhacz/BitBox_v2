#include "anim_debug.h"
#include "anim.h"
#include <rdix/rdix_debug_draw.h>
#include "foundation/math/vmath.h"


namespace anim_debug
{

    void DrawPose( const ANIMSkel* skel, array_span_t<const ANIMJoint> joints_ms, color32_t color, f32 scale )
    {
        const int16_t* parent_indices = ParentIndices( skel );
        const uint32_t num_joints = skel->numJoints;

        for( uint32_t i = 0; i < num_joints; ++i )
        {
            const int16_t parent_index = parent_indices[i];
            if( parent_index == -1 )
                continue;

            const ANIMJoint& parent = joints_ms[parent_index];
            const ANIMJoint& joint = joints_ms[i];

            RDIXDebug::AddLine( parent.position.xyz(), joint.position.xyz(), RDIXDebugParams( color ) );
            RDIXDebug::AddAxes( mat44_t( parent.rotation, parent.position ), RDIXDebugParams().Scale( scale ) );
        }
    }

}//