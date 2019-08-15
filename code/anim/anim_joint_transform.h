#pragma once

#include <foundation/type.h>
#include <foundation/math/vmath_type.h>

struct BIT_ALIGNMENT_16 ANIMJoint
{
	quat_t rotation;
	vec4_t position;
    vec4_t scale;

    static ANIMJoint identity()
    {
        ANIMJoint joint = { quat_t::identity(), vec4_t( 0.f ), vec4_t( 1.f ) };
        return joint;
    }
};

inline ANIMJoint toAnimJoint_noScale( const mat44_t& RT )
{
    ANIMJoint joint;
	joint.position = RT.c3;
	joint.rotation = quat_t( RT.upper3x3() );
	joint.scale = vec4_t( 1.f );
	return joint;
}

inline ANIMJoint toAnimJoint_noScale( const xform_t& RT )
{
    ANIMJoint joint;
    joint.position = vec4_t( RT.pos, 1.f );
    joint.rotation = RT.rot;
    joint.scale = vec4_t( 1.f );
    return joint;
}

inline mat44_t toMatrix4( const ANIMJoint& j )
{
    mat44_t m4x4( j.rotation, j.position.xyz() );
    m4x4.c0 *= j.scale.x;
	m4x4.c1 *= j.scale.y;
	m4x4.c2 *= j.scale.z;
	return m4x4;
}
