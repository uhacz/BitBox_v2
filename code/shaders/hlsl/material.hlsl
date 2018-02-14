<pass name="base" vertex="vs_base" pixel="ps_base">
<hwstate 
    depth_test="1"
    depth_write="1"
/>
</pass>
<pass name = "base_with_skybox" vertex = "vs_base" pixel = "ps_base">
<hwstate
    depth_test = "1"
    depth_write = "1"
    />
<define USE_SKYBOX="1"/>
</pass>
#~header

struct IN_VS
{
	uint   instanceID : SV_InstanceID;
	float3 pos	  	  : POSITION;
	float3 nrm		  : NORMAL;
	float2 uv0		  : TEXCOORD0;
};

struct IN_PS
{
	float4 pos_hs	: SV_Position;
	float3 pos_ws	: POSITION;
	float3 nrm_ws	: NORMAL;
	float2 uv0		: TEXCOORD;
};

struct OUT_PS
{
	float4 rgba : SV_Target0;
};


#define SHADER_IMPLEMENTATION

#include "common.h"

// --- tansform buffer
#include "transform_instance_data.h"
// --- frame
#include "material_frame_data.h"
// --- material
#include "material_data.h"
// --- samplers
#include "samplers.h"

IN_PS vs_base( IN_VS input )
{
	IN_PS output;

	// transform matrices rows
	float4 world_0, world_1, world_2;
	float3 world_it_0, world_it_1, world_it_2;
	
	LoadWorld( world_0, world_1, world_2, input.instanceID );
	LoadWorldIT( world_it_0, world_it_1, world_it_2, input.instanceID );

	float4 pos_ls = float4(input.pos, 1.0);
	float3 nrm_ls = input.nrm;

	float3 pos_ws = TransformPosition( world_0, world_1, world_2, pos_ls );
	float3 nrm_ws = TransformNormal( world_it_0, world_it_1, world_it_2, nrm_ls );

	float4 pos_hs = mul( _fdata.camera_view_proj, float4(pos_ws, 1.0) );
	
	output.pos_hs = pos_hs;
	output.pos_ws = pos_ws;
	output.nrm_ws = nrm_ws;
	output.uv0 = input.uv0;
	return output;
}

#define PI 3.1415926
#define PI_RCP 0.31830988618379067154

float WrappedNdotL( in float3 N, in float3 L, in float w )
{
	return saturate( (dot( N, L ) + w) / ((1 + w) * (1 + w)) );
}

// ================================================================================================
// Calculates the Fresnel factor using Schlick's approximation
// ================================================================================================
float3 Fresnel( in float3 specAlbedo, in float3 h, in float3 l )
{
	float lDotH = saturate( dot( l, h ) );
	float3 fresnel = specAlbedo + (1.0f - specAlbedo) * pow( (1.0f - lDotH), 5.0f );

	// Disable specular entirely if the albedo is set to 0.0
	fresnel *= dot( specAlbedo, 1.0f ) > 0.0f;

	return fresnel;
}

// ===============================================================================================
// Helper for computing the GGX visibility term
// ===============================================================================================
float GGX_V1( in float m2, in float nDotX )
{
	return 1.0f / (nDotX + sqrt( m2 + (1 - m2) * nDotX * nDotX ));
}

// ===============================================================================================
// Computes the specular term using a GGX microfacet distribution, with a matching
// geometry factor and visibility term. Based on "Microfacet Models for Refraction Through
// Rough Surfaces" [Walter 07]. m is roughness, n is the surface normal, h is the half vector,
// l is the direction to the light source, and specAlbedo is the RGB specular albedo
// ===============================================================================================
float GGX_Specular( in float m, in float3 n, in float3 h, in float3 v, in float3 l )
{
	float nDotH = saturate( dot( n, h ) );
	//float nDotL = WrappedNdotL( n, l, w );
    float nDotL = saturate( dot( n, l ) );
    float nDotV = saturate( dot( n, v ) );

	float nDotH2 = nDotH * nDotH;
	float m2 = m * m;

	// Calculate the distribution term
	float d = m2 / (PI * pow( nDotH * nDotH * (m2 - 1) + 1, 2.0f ));

	// Calculate the matching visibility term
	float v1i = GGX_V1( m2, nDotL );
	float v1o = GGX_V1( m2, nDotV );
	float vis = v1i * v1o;

	return d * vis;
}

struct LitData
{
    float3 diffuse;
    float3 specular;
};

LitData CalcDirectionalLighting( in float3 N, in float3 L, in float3 lightColor, in float3 diffuseAlbedo, in float3 specularAlbedo, in float roughness, in float3 posWS )
{
    LitData lit_data = (LitData)0;

	float3 lighting = 0.0f;
	//float nDotL = WrappedNdotL( N, L, w );
	float nDotL = saturate( dot( N, L) );
	if( nDotL > 0.0f )
	{
		float3 V = normalize( _fdata.camera_eye.xyz - posWS );
		float3 H = normalize( V + L );
		float specular = GGX_Specular( roughness, N, H, V, L );
		float3 fresnel = Fresnel( specularAlbedo, H, L );

        lit_data.diffuse = max( 0, diffuseAlbedo * PI_RCP * nDotL * lightColor );
        lit_data.specular = max( 0, specular * fresnel * nDotL * lightColor );
	}

    return lit_data;
}

OUT_PS ps_base( IN_PS input )
{
	const float3 light_pos = float3(-5.f, 5.f, 0.f);
	const float3 light_color = float3(1, 1, 1);
	const float3 L = normalize( light_pos - input.pos_ws );
	const float3 N = normalize( input.nrm_ws );
	
	float w = 0.1f;
    LitData lit_data = CalcDirectionalLighting( N, L, light_color, _material.diffuse_albedo, _material.specular_albedo, _material.roughness, input.pos_ws );
    
    float3 color = lit_data.diffuse + lit_data.specular;

#if USE_SKYBOX
    float3 V = normalize( _fdata.camera_eye.xyz - input.pos_ws );
    const float3 R = 2.0*dot(N,V)*N - V;
    float a2 = _material.roughness * _material.roughness;
    float specPower = (1.0 / a2 - 1.0) * 2.0;

    float MIPlevel = log2( _ldata.environment_map_width * sqrt( 3 ) ) - 0.5 * log2( specPower + 1 );
    float3 diffuse_env = _skybox.SampleLevel( _samp_bilinear, N, _ldata.environment_map_max_mip ).rgb;
    float3 specular_env = _skybox.SampleLevel( _samp_bilinear, R, MIPlevel ).rgb;

    float3 color_from_sky_diff = (lit_data.diffuse * diffuse_env);
    float3 color_from_sky_spec = (specular_env * Fresnel( _material.specular_albedo, N, V ));
    
    float3 color_from_sky = (color_from_sky_diff + color_from_sky_spec) * _ldata.sky_intensity * PI_RCP;
    color += color_from_sky;
#else
    float alpha = WrappedNdotL( N, L, 1.1f );
    float3 indirect = 0.1f * _material.diffuse_albedo * (alpha)* PI_RCP;

    color = max( color, indirect );
    color += indirect;
#endif
    
	OUT_PS output;
	output.rgba = float4(color, 1);
	return output;
}