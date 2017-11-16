#ifndef MATERIAL_FRAME_DATA
#define MATERIAL_FRAME_DATA

#define MATERIAL_FRAME_DATA_SLOT 1
#define MATERIAL_DATA_SLOT 2

struct MaterialFrameData
{
	float4x4 camera_world;
	float4x4 camera_view;
	float4x4 camera_view_proj;
	float4   camera_eye;
	float4   camera_dir;
};

#ifdef SHADER_IMPLEMENTATION
shared cbuffer _Frame : register(BSLOT( MATERIAL_FRAME_DATA_SLOT ))
{
	MaterialFrameData _fdata;
};
#endif


#endif