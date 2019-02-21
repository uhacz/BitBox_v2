#pragma once

#include <foundation/debug.h>
#include <foundation/tag.h>
#include <foundation/hashed_string.h>
#include "anim_struct.h"

inline int GetJointByHash( const ANIMSkel* skeleton, hashed_string_t joint_hash )
{
	SYS_ASSERT( skeleton != 0 );

	const hashed_string_t* joint_hash_array = TYPE_OFFSET_GET_POINTER( const hashed_string_t, skeleton->offsetJointNames );
	for( int i = 0; i < (int)skeleton->numJoints; ++i )
	{
		if( joint_hash == joint_hash_array[i] )
			return i;
	}
	return -1;
}

inline int GetJointByName( const ANIMSkel* skeleton, const char* joint_name )
{
	SYS_ASSERT( joint_name != 0 );
    const hashed_string_t joint_hash = hashed_string( joint_name );
	return GetJointByHash( skeleton, joint_hash );
}

