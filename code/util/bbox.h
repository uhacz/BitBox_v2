#pragma once

#include <foundation/math/vmath.h>

struct AABB
{
    vec3_t min, max;

	AABB( const vec3_t& a, const vec3_t& b )
        : min(a), max(b) {}

	AABB()
        : min(-0.5f), max(0.5f) {}

    static inline AABB Prepare()
    {
        return AABB( vec3_t( FLT_MAX ), vec3_t( -FLT_MAX ) );
    }

    inline vec3_t Size( const AABB& bbox )
    {
        return max - min;
    }

    inline vec3_t Center( const AABB& bbox )
    {
        return min + ( max - min ) * 0.5f;
    }

    static inline AABB Extend( const AABB& bbox, const vec3_t& point )
    {
        return AABB( min_per_elem( bbox.min, point ), max_per_elem( bbox.max, point ) );
    }

    static inline AABB Merge( const AABB& a, const AABB& b )
    {
        return AABB( min_per_elem( a.min, b.min ), max_per_elem( a.max, b.max ) );
    }

	static inline AABB Transform( const mat44_t& matrix, const AABB& bbox )
	{
		const vec4_t xa = matrix.c0 * bbox.min.x;
		const vec4_t xb = matrix.c0 * bbox.max.x;
		const vec4_t ya = matrix.c1 * bbox.min.y;
		const vec4_t yb = matrix.c1 * bbox.max.y;
		const vec4_t za = matrix.c2 * bbox.min.z;
		const vec4_t zb = matrix.c2 * bbox.max.z;

		AABB result;
		result.min = (min_per_elem( xa, xb ) + min_per_elem( ya, yb ) + min_per_elem( za, zb )).xyz() + matrix.translation();
		result.max = (max_per_elem( xa, xb ) + max_per_elem( ya, yb ) + max_per_elem( za, zb )).xyz() + matrix.translation();

		return result;
	}

	//static inline vec_float4 pointInAABBf4( const vec_float4 bboxMin, const vec_float4 bboxMax, const vec_float4 point )
	//{
	//	const vec_float4 a = vec_cmplt( bboxMin, point );
	//	const vec_float4 b = vec_cmpgt( bboxMax, point );
	//	const vec_float4 a_n_b = vec_and( a, b );
	//	return vec_and( vec_splat( a_n_b, 0 ), vec_and( vec_splat( a_n_b, 1 ), vec_splat( a_n_b, 2 ) ) );
	//}

	//static inline bool isPointInside( const bxAABB& aabb, const Vector3& point )
	//{
	//	return boolInVec( floatInVec( pointInAABBf4( aabb.min.get128(), aabb.max.get128(), point.get128() ) ) ).getAsBool();
	//}

	//static inline Vector3 closestPointOnABB( const Vector3& bboxMin, const Vector3& bboxMax, const Vector3& point )
	//{
	//	return minPerElem( maxPerElem( point, bboxMin ), bboxMax );
	//}

};

