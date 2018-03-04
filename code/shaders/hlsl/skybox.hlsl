<pass name = "skybox" vertex = "vs_fullscreenquad" pixel = "ps_skybox">
<hwstate
    depth_test = "0"
    depth_write = "0" />
    <define USE_SKYBOX = "1" />
    </pass>
#~header

#define SHADER_IMPLEMENTATION

#include "common.h"
#include "fullscreenquad.hlsl"
#include "material_frame_data.h"
#include "samplers.h"

#define IN_PS out_VS_screenquad

float4 ps_skybox( IN_PS input ) : SV_Target0
{
    //int3   position_ss = int3((int2)(input.uv * _fdata.rtarget_size ), 0);
    //float4 position_cs = float4(((float2(position_ss.xy) + 0.5) * _fdata.rtarget_size_rcp ) * float2(2.0, -2.0) + float2(-1.0, 1.0), -1.0, 1.0);

    float2 texel_ext = _fdata.rtarget_size_rcp * 0.5;
    float4 position_cs = float4( (input.uv + texel_ext) * float2(2.0, -2.0) + float2(-1.0, 1.0), -1.0, 1.0);
    float4 position_ws = mul( _fdata.camera_view_proj_inv, position_cs );
    position_ws.xyz *= rcp( position_ws.w );

    float3 V = normalize( _fdata.camera_eye.xyz - position_ws.xyz );
    float3 skybox_color = _skybox.SampleLevel( _samp_bilinear, -V, 0 );
    
    return float4( skybox_color, 1.0 );
}