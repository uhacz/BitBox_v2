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
	float4 pos	  	  : POSITION;
};

struct IN_PS
{
	float4 h_pos	: SV_Position;
};

struct OUT_PS
{
	float4 rgba : SV_Target0;
};

IN_PS vs_base( IN_VS input )
{
	IN_PS output;
	output.h_pos = input.pos;
	return output;
}

OUT_PS ps_base( IN_PS input )
{
	OUT_PS output;
	output.rgba = float4(1, 0, 0, 1);
	return output;
}