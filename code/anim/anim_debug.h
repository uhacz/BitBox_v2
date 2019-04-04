#pragma once

#include <util/color.h>
#include <foundation/containers.h>

struct ANIMSkel;
struct ANIMJoint;

namespace anim_debug
{
    void DrawPose( const ANIMSkel* skel, array_span_t<const ANIMJoint> joints_ms, color32_t color, f32 scale );
}//
