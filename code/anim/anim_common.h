#pragma once

#include "anim_struct.h"
#include <foundation/debug.h>
#include <foundation/hash.h>
#include <foundation/tag.h>
#include <foundation/hash.h>
#include <foundation/string_util.h>

static const uint32_t ANIM_SKEL_TAG = tag32_t( "SK01" );
static const uint32_t ANIM_CLIP_TAG = tag32_t( "CL01" );

inline int GetJointByHash( const ANIMSkel* skeleton, uint32_t joint_hash )
{
	SYS_ASSERT( skeleton != 0 );
	SYS_ASSERT( skeleton->tag == ANIM_SKEL_TAG );

	const uint32_t* joint_hash_array = TYPE_OFFSET_GET_POINTER( const uint32_t, skeleton->offsetJointNames );
	for( int i = 0; i < (int)skeleton->numJoints; ++i )
	{
		if( joint_hash == joint_hash_array[i] )
			return i;
	}
	return -1;
}

inline uint32_t GenerateJointNameHash( const char* joint_name )
{
    const uint32_t hash_seed = ANIM_SKEL_TAG;
    return murmur3_hash32( joint_name, string::length( joint_name ), hash_seed );
}

inline int GetJointByName( const ANIMSkel* skeleton, const char* joint_name )
{
	SYS_ASSERT( joint_name != 0 );
    const uint32_t joint_hash = GenerateJointNameHash( joint_name );
	return GetJointByHash( skeleton, joint_hash );
}

