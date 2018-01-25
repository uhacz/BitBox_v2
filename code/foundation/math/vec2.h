#pragma once

inline float length_sqr( const vec2_t& v )
{
    return v.x * v.x + v.y * v.y;
}

inline float length( const vec2_t& v )
{
    return ::sqrtf( length_sqr(v) );
}

inline float dot( const vec2_t& a, const vec2_t& b )
{
    return a.x * b.x + a.y * b.y;
}

inline float normalized( vec2_t* v )
{
    const float m = length_sqr(*v);
    v[0] = ( m > 0.0f ) ? *v * recip_sqrt( m ) : vec2_t( 0.f );
    return m;
}

inline vec2_t normalize( const vec2_t& v )
{
    const float m = length_sqr(v);
    return ( m > 0.f ) ? v * recip_sqrt( m ) : vec2_t( 0.f );
}

inline vec2_t mul_per_elem( const vec2_t& a, const vec2_t& b )
{
    return vec2_t( a.x * b.x, a.y * b.y );
}

inline vec2_t min_per_elem( const vec2_t& a, const vec2_t& b )
{
    return vec2_t( min_of_2( a.x, b.x ), min_of_2( a.y, b.y ) );
}

inline float min_elem( const vec2_t& v )
{
    return min_of_2( v.x, v.y );
}

inline vec2_t max_per_elem( const vec2_t& a, const vec2_t& b )
{
    return vec2_t( max_of_2( a.x, b.x ), max_of_2( a.y, b.y ) );
}

inline float max_elem( const vec2_t& v )
{
    return max_of_2( v.x, v.y );
}

 inline vec2_t operator*( float f, const vec2_t& v )
{
    return vec2_t( f * v.x, f * v.y );
}
