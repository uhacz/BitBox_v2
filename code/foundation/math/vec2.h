#pragma once

#include "math_type.h"

inline float dot( const vec2_t& a, const vec2_t& b )
{
	return a.x*b.x + a.y*b.y;
}
