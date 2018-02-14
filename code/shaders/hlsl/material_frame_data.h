#ifndef MATERIAL_FRAME_DATA
#define MATERIAL_FRAME_DATA

#define MATERIAL_FRAME_DATA_SLOT 1
#define MATERIAL_DATA_SLOT 2
#define LIGHTING_FRAME_DATA_SLOT 3
#define LIGHTING_SKY_CUBEMAP_SLOT 6

struct MaterialFrameData
{
	float4x4 camera_world;
	float4x4 camera_view;
	float4x4 camera_view_proj;
	float4   camera_eye;
	float4   camera_dir;

    float2 rtarget_size;
    float2 rtarget_size_rcp;
};

struct LightningFrameData
{
    float4 sun_L; // L means direction TO light
    float4 sun_color; 
    float sun_intensity;
    float sky_intensity;
    float environment_map_width;
    int environment_map_max_mip;
};


#ifdef SHADER_IMPLEMENTATION
shared cbuffer _Frame : register(BSLOT( MATERIAL_FRAME_DATA_SLOT ))
{
	MaterialFrameData _fdata;
};

shared cbuffer _Lighting : register(BSLOT( LIGHTING_FRAME_DATA_SLOT ))
{
    LightningFrameData _ldata;
};

#if USE_SKYBOX
textureCUBE<float3> _skybox : register(TSLOT( LIGHTING_SKY_CUBEMAP_SLOT ));
#endif


#endif


#endif