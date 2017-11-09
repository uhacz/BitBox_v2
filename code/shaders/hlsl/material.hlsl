<pass name="base" vertex="vs_base" pixel="ps_base">
<hwstate 
    depth_test="1"
    depth_write="1"
/>
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

#define BSLOT( n ) b##n
#define TSLOT( n ) t##n

// --- tansform buffer
#include "transform_instance_data.h"
shared cbuffer _InstanceOffset : register( BSLOT(TRANSFORM_INSTANCE_DATA_SLOT) )
{
	TransformInstanceData _instance_data;
};
Buffer<float4> _instance_world   : register( TSLOT( TRANSFORM_INSTANCE_WORLD_SLOT ) );
Buffer<float3> _instance_worldIT : register( TSLOT( TRANSFORM_INSTANCE_WORLD_IT_SLOT ) );

void LoadWorld( out float4 row0, out float4 row1, out float4 row2, uint instanceID )
{
	uint row0Index = (_instance_data.offset + instanceID) * 3;
	row0 = _instance_world[row0Index];
	row1 = _instance_world[row0Index + 1];
	row2 = _instance_world[row0Index + 2];
}

void LoadWorldIT( out float3 row0IT, out float3 row1IT, out float3 row2IT, uint instanceID )
{
	uint row0Index = (_instance_data.offset + instanceID) * 3;
	row0IT = _instance_worldIT[row0Index];
	row1IT = _instance_worldIT[row0Index + 1];
	row2IT = _instance_worldIT[row0Index + 2];
}

float3 TransformPosition( in float4 row0, in float4 row1, in float4 row2, in float4 localPos )
{
	return float3(dot( row0, localPos ), dot( row1, localPos ), dot( row2, localPos ));
}
float3 TransformNormal( in float3 row0IT, in float3 row1IT, in float3 row2IT, in float3 normal )
{
	return float3(dot( row0IT, normal ), dot( row1IT, normal ), dot( row2IT, normal ));
}

// --- frame
#include "material_frame_data.h"
shared cbuffer _Frame : register( BSLOT( MATERIAL_FRAME_DATA_SLOT ) )
{
	MaterialFrameData _fdata;
};

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

OUT_PS ps_base( IN_PS input )
{
	OUT_PS output;
	output.rgba = float4(1, 0, 0, 1);
	return output;
}