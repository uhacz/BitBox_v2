#pragma once

struct vec2_t
{
	union
	{
		float xy[2];
		float x, y;
	};

	vec2_t(                    ) {}
	vec2_t( float s            ) : x( s ), y( s ) {}
	vec2_t( float x_, float y_ ) : x( x_ ), y( y_ ) {}
	vec2_t( const vec2_t& v    ) : x( v.x ), y( v.y ) {}
};


struct vec3_t
{
	union
	{
		float xyz[3];
		float x, y, z;
	};

	vec3_t(                              ) {}
	vec3_t( float s                      ) : x( s ), y( s ), z( s ) {}
	vec3_t( float x_, float y_, float z_ ) : x( x_ ), y( y_ ), z( z_ ) {}
	vec3_t( const vec2_t& v, float z_    ) : x( v.x ), y( v.y ), z( z_ ) {}
	vec3_t( const vec3_t& v              ) : x( v.x ), y( v.y ), z( v.z ) {}
};


struct vec4_t
{
	union
	{
		float xyzw[4];
		float x, y, z, w;
	};

	vec4_t(                                        ) {}
	vec4_t( float s                                ) : x( s ), y( s ), z( s ), w( s ) {}
	vec4_t( float x_, float y_, float z_, float w_ ) : x( x_ ), y( y_ ), z( z_ ), w( w_ ) {}
	vec4_t( const vec3_t& v, float w_              ) : x( v.x ), y( v.y ), z( v.z ), w( w_ ) {}
	vec4_t( const vec4_t& v                        ) : x( v.x ), y( v.y ), z( v.z ), w( v.w ) {}
};

struct mat33_t;
struct quat_t
{
	union
	{
		float xyzw[4];
		float x, y, z, w;
	};

	quat_t(                                        ) {}
	quat_t( float s                                ) : x( s ), y( s ), z( s ), w( s ) {}
	quat_t( float x_, float y_, float z_, float w_ ) : x( x_ ), y( y_ ), z( z_ ), w( w_ ) {}
	quat_t( const vec3_t& v, float w_              ) : x( v.x ), y( v.y ), z( v.z ), w( w_ ) {}
	quat_t( const vec4_t& v                        ) : x( v.x ), y( v.y ), z( v.z ), w( v.w ) {}
	quat_t( const quat_t& q                        ) : x( q.x ), y( q.y ), z( q.z ), w( q.w ) {}
	quat_t( const mat33_t& m );

	static quat_t identity() { return quat_t( 0.f, 0.f, 0.f, 1.f ); }
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
	xform_t( const mat33_t& m );
	xform_t( const mat44_t& m );
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
};