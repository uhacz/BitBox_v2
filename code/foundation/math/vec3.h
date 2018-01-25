#pragma once

inline float length_sqr( const vec3_t& v )
{
    return v.x * v.x + v.y * v.y + v.z * v.z;
}

inline float length( const vec3_t& v )
{
    return ::sqrtf( length_sqr(v) );
}

inline float dot( const vec3_t& a, const vec3_t& b )
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline vec3_t cross( const vec3_t& a, const vec3_t& b )
{
    return vec3_t( a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x );
}

inline float normalized( vec3_t* v )
{
    const float m = length_sqr( *v );
    v[0] = ( m > 0.0f ) ? *v * recip_sqrt( m ) : vec3_t( 0.f );
    return m;
}

inline vec3_t normalize( const vec3_t& v, float tolerance = FLT_EPSILON )
{
    const float m = length_sqr( v );
    return ( m >= tolerance) ? v * recip_sqrt( m ) : vec3_t( 0.f );
}

inline vec3_t mul_per_elem( const vec3_t& a, const vec3_t& b )
{
    return vec3_t( b.x * a.x, b.y * a.y, b.z * a.z );
}

inline vec3_t min_per_elem( const vec3_t& a, const vec3_t& b )
{
    return vec3_t( min_of_2( a.x, b.x ), min_of_2( a.y, b.y ), min_of_2( a.z, b.z ) );
}

inline float min_elem( const vec3_t& v )
{
    return min_of_2( v.x, min_of_2( v.y, v.z ) );
}

inline vec3_t max_per_elem( const vec3_t& a, const vec3_t& b )
{
    return vec3_t( max_of_2( a.x, b.x ), max_of_2( a.y, b.y ), max_of_2( a.z, b.z ) );
}

inline float max_elem( const vec3_t& v )
{
    return max_of_2( v.x, max_of_2( v.y, v.z ) );
}

inline vec3_t abs_per_elem( const vec3_t& v )
{
    return vec3_t( ::fabsf( v.x ), ::fabsf( v.y ), ::fabsf( v.z ) );
}
