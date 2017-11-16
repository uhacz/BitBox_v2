#pragma once

inline mat44_t::mat44_t( const quat_t& rot, const vec3_t& pos )
{
	*this = mat44_t( mat33_t( rot ), pos );
}

inline mat44_t transpose( const mat44_t& m )
{
    return mat44_t(
        vec4_t( m.c0.x, m.c1.x, m.c2.x, m.c3.x ),
        vec4_t( m.c0.y, m.c1.y, m.c2.y, m.c3.y ),
        vec4_t( m.c0.z, m.c1.z, m.c2.z, m.c3.z ),
        vec4_t( m.c0.w, m.c1.w, m.c2.w, m.c3.w ) );
}

mat44_t inverse( const mat44_t& m );

inline vec3_t mul_as_point( const mat44_t& m, const vec3_t& other )
{
    return ( m * vec4_t( other, 1.f ) ).xyz();
}

inline vec4_t rotate( const mat44_t& m, const vec4_t& other )
{
    return m.c0 * other.x + m.c1 * other.y + m.c2 * other.z;
}


inline vec3_t rotate( const mat44_t& m, const vec3_t& other )
{
    return rotate( m, vec4_t( other, 1.0f ) ).xyz();
}

