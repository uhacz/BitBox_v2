#pragma once

inline mat44_t::mat44_t( const quat_t& q )
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

inline mat44_t::mat44_t( const xform_t& xf )
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

inline mat44_t mat44_t::translation( const vec3_t& v )
{
    return mat44_t( vec4_t::ax(), vec4_t::ay(), vec4_t::az(), vec4_t( v, 1.0f ) );
}
inline mat44_t mat44_t::rotationx( float rad )
{
    const float s = ::sinf( rad );
    const float c = ::cosf( rad );
    return mat44_t(
        vec4_t::ax(),
        vec4_t( 0.0f, c, s, 0.0f ),
        vec4_t( 0.0f, -s, c, 0.0f ),
        vec4_t::aw()
    );
}
inline mat44_t mat44_t::rotationy( float rad )
{
    float s, c;
    s = ::sinf( rad );
    c = ::cosf( rad );
    return mat44_t(
        vec4_t( c, 0.0f, -s, 0.0f ),
        vec4_t::ay(),
        vec4_t( s, 0.0f, c, 0.0f ),
        vec4_t::aw()
    );
}
inline mat44_t mat44_t::rotationz( float rad )
{
    const float s = sinf( rad );
    const float c = cosf( rad );
    return mat44_t(
        vec4_t( c, s, 0.0f, 0.0f ),
        vec4_t( -s, c, 0.0f, 0.0f ),
        vec4_t::az(),
        vec4_t::aw()
    );
}
inline mat44_t mat44_t::rotationzyx( const vec3_t& radxyz )
{
    const float sX = sinf( radxyz.x );
    const float cX = cosf( radxyz.x );
    const float sY = sinf( radxyz.y );
    const float cY = cosf( radxyz.y );
    const float sZ = sinf( radxyz.z );
    const float cZ = cosf( radxyz.z );
    const float tmp0 = (cZ * sY);
    const float tmp1 = (sZ * sY);
    return mat44_t(
        vec4_t( (cZ * cY), (sZ * cY), -sY, 0.0f ),
        vec4_t( ((tmp0 * sX) - (sZ * cX)), ((tmp1 * sX) + (cZ * cX)), (cY * sX), 0.0f ),
        vec4_t( ((tmp0 * cX) + (sZ * sX)), ((tmp1 * cX) - (cZ * sX)), (cY * cX), 0.0f ),
        vec4_t::aw()
    );
}
inline mat44_t mat44_t::rotation( float rad, const vec3_t& axis )
{
    const float s = sinf( rad );
    const float c = cosf( rad );
    const float x = axis.x;
    const float y = axis.y;
    const float z = axis.z;
    const float xy = (x * y);
    const float yz = (y * z);
    const float zx = (z * x);
    const float oneMinusC = (1.0f - c);
    return mat44_t(
        vec4_t( (((x * x) * oneMinusC) + c), ((xy * oneMinusC) + (z * s)), ((zx * oneMinusC) - (y * s)), 0.0f ),
        vec4_t( ((xy * oneMinusC) - (z * s)), (((y * y) * oneMinusC) + c), ((yz * oneMinusC) + (x * s)), 0.0f ),
        vec4_t( ((zx * oneMinusC) + (y * s)), ((yz * oneMinusC) - (x * s)), (((z * z) * oneMinusC) + c), 0.0f ),
        vec4_t::aw()
    );
}
inline mat44_t mat44_t::scale( const vec3_t& v )
{
    return mat44_t(
        vec4_t( v.x, 0.0f, 0.0f, 0.0f ),
        vec4_t( 0.0f, v.y, 0.0f, 0.0f ),
        vec4_t( 0.0f, 0.0f, v.z, 0.0f ),
        vec4_t::aw()
    );
}

inline const mat44_t append_scale( const mat44_t& mat, const vec3_t& scale )
{
    return mat44_t(
        (mat.c0 * scale.x),
        (mat.c1 * scale.y),
        (mat.c2 * scale.z),
        mat.c3
    );
}

inline const mat44_t prependScale( const vec3_t& scale, const mat44_t& mat )
{
    const vec4_t scale4( scale, 1.0f );
    return mat44_t(
        mul_per_elem( mat.c0, scale4 ),
        mul_per_elem( mat.c1, scale4 ),
        mul_per_elem( mat.c2, scale4 ),
        mul_per_elem( mat.c3, scale4 )
    );
}