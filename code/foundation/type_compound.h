#pragma once

#include "type.h"

union float2_t
{
	float32_t xy[2];
	struct { float32_t x, y; };

	float2_t() {}
	float2_t( float32_t vx, float32_t vy )
		: x( vx ), y( vy ) {}
};

union float3_t
{
	float32_t xyz[3];
	struct { float32_t x, y, z; };

	float3_t() {}
	float3_t( float32_t vxyz ) : x( vxyz ), y( vxyz ), z( vxyz ) {}
	float3_t( float32_t vx, float32_t vy, float32_t vz ) : x( vx ), y( vy ), z( vz ) {}
};
inline float3_t operator * ( const float3_t& a, float b ) { return float3_t( a.x * b, a.y * b, a.y * b ); }

union float4_t
{
	float32_t xyzw[4];
	struct { float32_t x, y, z, w; };

	float4_t() {}
	float4_t( float32_t vx, float32_t vy, float32_t vz, float32_t vw ) : x( vx ), y( vy ), z( vz ), w( vw ) {}
	float4_t( const float3_t xyz, float32_t vw ) : x( xyz.x ), y( xyz.y ), z( xyz.z ), w( vw ) {}
};

union int32_tx2
{
	struct
	{
		int32_t x, y;
	};
	int32_t xyz[2];

	int32_tx2() : x( 0 ), y( 0 ) {}
	int32_tx2( int32_t all ) : x( all ), y( all ) {}
	int32_tx2( int32_t a, int32_t b ) : x( a ), y( b ) {}
};
union int32_tx3
{
	struct
	{
		int32_t x, y, z;
	};
	int32_t xyz[3];

	int32_tx3() : x( 0 ), y( 0 ), z( 0 ) {}
	int32_tx3( int32_t all ) : x( all ), y( all ), z( all ) {}
	int32_tx3( int32_t a, int32_t b, int32_t c ) : x( a ), y( b ), z( c ) {}
};

