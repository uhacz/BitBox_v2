#include "anim.h"
#include <foundation/debug.h>
#include <foundation/math/vmath.h>

void LocalJointsToWorldJoints( ANIMJoint* outJoints, const ANIMJoint* inJoints, const uint16_t* parentIndices, uint32_t count, const ANIMJoint& rootJoint )
{
	SYS_ASSERT( outJoints != inJoints );
	SYS_ASSERT( count > 0 );

	uint32_t i = 0;
	do 
	{
		const uint16_t parentIdx = parentIndices[i];

		ANIMJoint world;
		const ANIMJoint& local = inJoints[i];
		const ANIMJoint& worldParent = ( parentIdx != 0xFFFF ) ? outJoints[parentIdx] : rootJoint;

		world.rotation = worldParent.rotation * local.rotation;
		world.rotation = normalize( world.rotation );

		world.scale = mul_per_elem( local.scale, worldParent.scale );

		const vec3_t ts = mul_per_elem( local.position, worldParent.scale ).xyz();
		const vec4_t q = worldParent.rotation.to_vec4();

		const float w = q.w;

        const vec3_t qxyz = q.xyz();
		const vec3_t c = cross( qxyz, ts ) + ts * w;
		const vec3_t qts = ts + cross( qxyz, c ) * 2.f;

		world.position = worldParent.position + vec4_t( qts, 0.f );

		outJoints[i] = world;

	} while ( ++i < count );
}
