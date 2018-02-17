<pass name = "shadow_ss" vertex = "vs_fullscreenquad" pixel = "ps_shadow_ss">
<hwstate
    depth_test = "0"
    depth_write = "0" />
    </pass>
#~header

#define SHADER_IMPLEMENTATION

#include "common.h"
#include "fullscreenquad.hlsl"
#include "shadow_data.h"
#include "samplers.h"

#define IN_PS out_VS_screenquad

Texture2D<float> depth_tex  : register(t0);

shared cbuffer _Frame : register(BSLOT( SHADOW_DATA_SLOT ))
{
    ShadowData _fdata;
};

float4 ps_shadow_ss( IN_PS input ) : SV_Target0
{
    int3   position_ss = int3((int2)(input.uv * _fdata.rtarget_size ), 0);
    float  depth_cs = depth_tex.Load( position_ss );
    float4 position_cs = float4(((float2(position_ss.xy) + 0.5) * _fdata.rtarget_size_rcp) * float2(2.0, -2.0) + float2(-1.0, 1.0), -depth_cs, 1.0);

    float4 position_ws = mul( _fdata.camera_view_proj_inv, position_cs );
    position_ws.xyz *= rcp( position_ws.w );

    return position_ws;
    return float4(depth_cs, 0, 0, 0);
}