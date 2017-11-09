#ifndef MATERIAL_FRAME_DATA
#define MATERIAL_FRAME_DATA

#define MATERIAL_FRAME_DATA_SLOT 1

struct MaterialFrameData
{
	float4x4 camera_world;
	float4x4 camera_view;
	float4x4 camera_view_proj;
};




#endif