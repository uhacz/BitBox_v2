#include "anim.h"
#include <foundation\math\vmath.h>
#include <foundation\common.h>

void BlendJointsLinear( ANIMJoint* outJoints, const ANIMJoint* leftJoints, const ANIMJoint* rightJoints, float blendFactor, uint32_t numJoints )
{
	uint32_t i = 0;

	do 
	{
        const quat_t& q0 = leftJoints[i].rotation;
        const quat_t& q1 = rightJoints[i].rotation;
        const quat_t q = slerp( blendFactor, q0, q1 );

		outJoints[i].rotation = q;

	} while ( ++i < numJoints );

	i = 0;
	do 
	{
        const vec4_t& t0 = leftJoints[i].position;
        const vec4_t& t1 = rightJoints[i].position;
        const vec4_t t = lerp( blendFactor, t0, t1 );

		outJoints[i].position = t;

	} while ( ++i < numJoints );

	i = 0;
	do 
	{
		const vec4_t& s0 = leftJoints[i].scale;
		const vec4_t& s1 = rightJoints[i].scale;
        const vec4_t s = lerp( blendFactor, s0, s1 );

		outJoints[i].scale = s;

	} while ( ++i < numJoints );
}

