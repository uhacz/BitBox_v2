#pragma once

mat44_t::mat44_t( const quat_t& q )
{
    const float x = q.x;
    const float y = q.y;
    const float z = q.z;
    const float w = q.w;

    const float x2 = x + x;
    const float y2 = y + y;
    const float z2 = z + z;

    const float xx = x2 * x;
    const float yy = y2 * y;
    const float zz = z2 * z;

    const float xy = x2 * y;
    const float xz = x2 * z;
    const float xw = x2 * w;

    const float yz = y2 * z;
    const float yw = y2 * w;
    const float zw = z2 * w;

    c0 = vec4_t( 1.0f - yy - zz, xy + zw, xz - yw, 0.0f );
    c1 = vec4_t( xy - zw, 1.0f - xx - zz, yz + xw, 0.0f );
    c2 = vec4_t( xz + yw, yz - xw, 1.0f - xx - yy, 0.0f );
    c3 = vec4_t( 0.0f, 0.0f, 0.0f, 1.0f );
}

mat44_t::mat44_t( const xform_t& xf )
{
    const float x = xf.rot.x;
    const float y = xf.rot.y;
    const float z = xf.rot.z;
    const float w = xf.rot.w;

    const float x2 = x + x;
    const float y2 = y + y;
    const float z2 = z + z;

    const float xx = x2 * x;
    const float yy = y2 * y;
    const float zz = z2 * z;

    const float xy = x2 * y;
    const float xz = x2 * z;
    const float xw = x2 * w;

    const float yz = y2 * z;
    const float yw = y2 * w;
    const float zw = z2 * w;

    c0 = vec4_t( 1.0f - yy - zz, xy + zw, xz - yw, 0.0f );
    c1 = vec4_t( xy - zw, 1.0f - xx - zz, yz + xw, 0.0f );
    c2 = vec4_t( xz + yw, yz - xw, 1.0f - xx - yy, 0.0f );
    c3 = vec4_t( xf.pos, 1.0f );
}

inline mat44_t transpose( const mat44_t& m )
{
    return mat44_t(
        vec4_t( m.c0.x, m.c1.x, m.c2.x, m.c3.x ),
        vec4_t( m.c0.y, m.c1.y, m.c2.y, m.c3.y ),
        vec4_t( m.c0.z, m.c1.z, m.c2.z, m.c3.z ),
        vec4_t( m.c0.w, m.c1.w, m.c2.w, m.c3.w ) );
}

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

