#pragma once

struct vec2_t
{
	union
	{
		float xy[2];
        struct{ float x, y; };
	};

	vec2_t(                    ) {}
	vec2_t( float s            ) : x( s ), y( s ) {}
	vec2_t( float x_, float y_ ) : x( x_ ), y( y_ ) {}
	vec2_t( const vec2_t& v    ) : x( v.x ), y( v.y ) {}

    inline vec2_t operator-()                  const { return vec2_t( -x, -y ); }
    inline vec2_t operator+( const vec2_t& v ) const { return vec2_t( x + v.x, y + v.y ); }
    inline vec2_t operator-( const vec2_t& v ) const { return vec2_t( x - v.x, y - v.y ); }
    inline vec2_t operator*( float f )         const { return vec2_t( x * f, y * f ); }
    inline vec2_t operator/( float f )         const { f = 1.0f / f; return vec2_t( x * f, y * f ); }

    inline vec2_t& operator+=( const vec2_t& v ) { x += v.x; y += v.y; return *this; }
    inline vec2_t& operator-=( const vec2_t& v ) { x -= v.x; y -= v.y; return *this; }
    inline vec2_t& operator*=( float f ) { x *= f; y *= f; return *this; }
    inline vec2_t& operator/=( float f ) { f = 1.0f / f; x *= f; y *= f; return *this; }

};


struct vec3_t
{
    union
    {
        float xyz[3];
        struct{ float x, y, z; };
    };

    vec3_t() {}
    vec3_t( float s ) : x( s ), y( s ), z( s ) {}
    vec3_t( float x_, float y_, float z_ ) : x( x_ ), y( y_ ), z( z_ ) {}
    vec3_t( const vec2_t& v, float z_ ) : x( v.x ), y( v.y ), z( z_ ) {}
    vec3_t( const vec3_t& v ) : x( v.x ), y( v.y ), z( v.z ) {}

    inline vec3_t  operator- ()                  const { return vec3_t( -x, -y, -z ); }
    inline vec3_t  operator+ ( const vec3_t& v ) const { return vec3_t( x + v.x, y + v.y, z + v.z ); }
    inline vec3_t  operator- ( const vec3_t& v ) const { return vec3_t( x - v.x, y - v.y, z - v.z ); }
    inline vec3_t  operator* ( float f )         const { return vec3_t( x * f, y * f, z * f ); }
    inline vec3_t  operator/ ( float f )         const { f = 1.0f / f; return vec3_t( x * f, y * f, z * f ); }

    inline vec3_t& operator+=( const vec3_t& v ) { x += v.x; y += v.y; z += v.z; return *this; }
    inline vec3_t& operator-=( const vec3_t& v ) { x -= v.x; y -= v.y; z -= v.z; return *this; }
    inline vec3_t& operator*=( float f ) { x *= f; y *= f; z *= f; return *this; }
    inline vec3_t& operator/=( float f ) { f = 1.0f / f; x *= f; y *= f; z *= f; return *this; }

    static vec3_t ax() { return vec3_t( 1.f, 0.f, 0.f ); }
    static vec3_t ay() { return vec3_t( 0.f, 1.f, 0.f ); }
    static vec3_t az() { return vec3_t( 0.f, 0.f, 1.f ); }
};


struct vec4_t
{
	union
	{
		float xyzw[4];
        struct{ float x, y, z, w; };
	};

	vec4_t(                                        ) {}
	vec4_t( float s                                ) : x( s ), y( s ), z( s ), w( s ) {}
	vec4_t( float x_, float y_, float z_, float w_ ) : x( x_ ), y( y_ ), z( z_ ), w( w_ ) {}
	vec4_t( const vec3_t& v, float w_              ) : x( v.x ), y( v.y ), z( v.z ), w( w_ ) {}
	vec4_t( const vec4_t& v                        ) : x( v.x ), y( v.y ), z( v.z ), w( v.w ) {}

    inline vec4_t operator-()                  const { return vec4_t( -x, -y, -z, -w ); }
    inline vec4_t operator+( const vec4_t& v ) const { return vec4_t( x + v.x, y + v.y, z + v.z, w + v.w ); }
    inline vec4_t operator-( const vec4_t& v ) const { return vec4_t( x - v.x, y - v.y, z - v.z, w - v.w ); }
    inline vec4_t operator*( float f )         const { return vec4_t( x * f, y * f, z * f, w * f ); }
    inline vec4_t operator/( float f )         const { f = 1.0f / f; return vec4_t( x * f, y * f, z * f, w * f ); }

    inline vec4_t& operator+=( const vec4_t& v ) { x += v.x; y += v.y; z += v.z; w += v.w; return *this; }
    inline vec4_t& operator-=( const vec4_t& v ) { x -= v.x; y -= v.y; z -= v.z; w -= v.w; return *this; }
    inline vec4_t& operator*=( float f ) { x *= f; y *= f; z *= f; w *= f; return *this; }
    inline vec4_t& operator/=( float f ) { f = 1.0f / f; x *= f; y *= f; z *= f; w *= f; return *this; }

    static vec4_t ax() { return vec4_t( 1.f, 0.f, 0.f, 0.f ); }
    static vec4_t ay() { return vec4_t( 0.f, 1.f, 0.f, 0.f ); }
    static vec4_t az() { return vec4_t( 0.f, 0.f, 1.f, 0.f ); }
    static vec4_t aw() { return vec4_t( 0.f, 0.f, 0.f, 1.f ); }
};

struct mat33_t;
struct quat_t
{
	union
	{
		float xyzw[4];
        struct{ float x, y, z, w; };
	};

	quat_t(                                        ) {}
	quat_t( float s                                ) : x( s ), y( s ), z( s ), w( s ) {}
	quat_t( float x_, float y_, float z_, float w_ ) : x( x_ ), y( y_ ), z( z_ ), w( w_ ) {}
	quat_t( const vec3_t& v, float w_              ) : x( v.x ), y( v.y ), z( v.z ), w( w_ ) {}
	quat_t( const vec4_t& v                        ) : x( v.x ), y( v.y ), z( v.z ), w( v.w ) {}
	quat_t( const quat_t& q                        ) : x( q.x ), y( q.y ), z( q.z ), w( q.w ) {}
	quat_t( const mat33_t& m );
    quat_t( float radians, const vec3_t& axis );

	static quat_t identity() { return quat_t( 0.f, 0.f, 0.f, 1.f ); }

    inline quat_t operator*( const quat_t& q ) const
    {
        return quat_t( w * q.x + q.w * x + y * q.z - q.y * z, w * q.y + q.w * y + z * q.x - q.z * x,
                       w * q.z + q.w * z + x * q.y - q.x * y, w * q.w - x * q.x - y * q.y - z * q.z );
    }
    inline quat_t operator-() const { return quat_t( -x, -y, -z, -w ); }
    inline quat_t operator+( const quat_t& q ) const { return quat_t( x + q.x, y + q.y, z + q.z, w + q.w ); }
    inline quat_t operator-( const quat_t& q ) const { return quat_t( x - q.x, y - q.y, z - q.z, w - q.w ); }
    inline quat_t operator*( float r )         const { return quat_t( x * r, y * r, z * r, w * r ); }

    inline quat_t& operator*=( const quat_t& q )
    {
        const float tx = w * q.x + q.w * x + y * q.z - q.y * z;
        const float ty = w * q.y + q.w * y + z * q.x - q.z * x;
        const float tz = w * q.z + q.w * z + x * q.y - q.x * y;

        w = w * q.w - q.x * x - y * q.y - q.z * z;
        x = tx;
        y = ty;
        z = tz;

        return *this;
    }

    inline quat_t& operator+=( const quat_t& q ) { x += q.x; y += q.y; z += q.z; w += q.w; return *this; }
    inline quat_t& operator-=( const quat_t& q ) { x -= q.x; y -= q.y; z -= q.z; w -= q.w; return *this; }
    inline quat_t& operator*=( const float s ) { x *= s; y *= s; z *= s; w *= s; return *this; }
};


struct mat44_t;
struct xform_t
{
	quat_t rot;
	vec3_t pos;

	xform_t() {}
	xform_t( const quat_t& q ) : rot( q ), pos( 0.f ) {}
	xform_t( const vec3_t& p ) : rot( quat_t::identity() ), pos( p ) {}
	xform_t( const quat_t& q, const vec3_t& p ) : rot( q ), pos( p ) {}
    xform_t( const mat33_t& m ) : rot( m ), pos( 0.f ) {}
    xform_t( const mat44_t& m );

    static xform_t identity() { return xform_t( quat_t::identity(), vec3_t( 0.f ) ); }
};

struct mat33_t
{
	vec3_t c0;
	vec3_t c1;
	vec3_t c2;

	mat33_t() {}
	mat33_t( float s ) : c0( s ), c1( s ), c2( s ) {}
	mat33_t( const vec3_t& v ) : c0( v ), c1( v ), c2( v ) {}
	mat33_t( const vec3_t& v0, const vec3_t& v1, const vec3_t& v2 ) : c0( v0 ), c1( v1 ), c2( v2 ) {}
	mat33_t( const quat_t& q );
	mat33_t( const mat33_t& m ) : c0( m.c0 ), c1( m.c1 ), c2( m.c2 ) {}
	mat33_t( const mat44_t& m );

    static mat33_t identity() { return mat33_t( vec3_t::ax(), vec3_t::ay(), vec3_t::az() ); }
};

struct mat44_t
{
	vec4_t c0;
	vec4_t c1;
	vec4_t c2;
	vec4_t c3;

	mat44_t() {}
	mat44_t( float s ) : c0( s ), c1( s ), c2( s ), c3(s) {}
	mat44_t( const vec4_t& v ) : c0( v ), c1( v ), c2( v ), c3( v ) {}
	mat44_t( const vec4_t& v0, const vec4_t& v1, const vec4_t& v2, const vec4_t& v3 ) : c0( v0 ), c1( v1 ), c2( v2 ), c3(v3) {}
	mat44_t( const quat_t& q );
	mat44_t( const mat33_t& m ) : c0( vec4_t(m.c0, 0.f) ), c1( vec4_t( m.c1, 0.f ) ), c2( vec4_t( m.c2, 0.f ) ), c3( 0.f, 0.f, 0.f, 1.f ) {}
	mat44_t( const mat44_t& m ) : c0( m.c0 ), c1( m.c1 ), c2( m.c2 ), c3(m.c3 ) {}

    static mat44_t identity() { return mat44_t( vec4_t::ax(), vec4_t::ay(), vec4_t::az(), vec4_t::aw() ); }
    
};