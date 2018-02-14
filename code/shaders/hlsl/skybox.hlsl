<pass name = "skybox" vertex = "vs_fullscreenquad" pixel = "ps_skybox">
<hwstate
    depth_test = "0"
    depth_write = "0" />
    </pass>
#~header

#define SHADER_IMPLEMENTATION

#include "common.h"
#include "fullscreenquad.hlsl"
#include "material_frame_data.h"

#define IN_PS out_VS_screenquad

float4 ps_skybox( IN_PS input ) : SV_Target0
{
    return float4(1, 0, 0, 0);
}
