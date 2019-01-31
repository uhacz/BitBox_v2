#pragma once

//#include <rtti/rtti.h>
#include <foundation/math/vmath_type.h>
#include <rdi_backend/rdi_backend_type.h>
//#include "dll_interface.h"

namespace gfx_shader
{
    using float2 = vec2_t;
    using float3 = vec3_t;
    using float4 = vec4_t;
    using float3x3 = mat33_t;
    using float4x4 = mat44_t;
    using uint = uint32_t;

#include <shaders/hlsl/samplers.h>
#include <shaders/hlsl/material_data.h>
#include <shaders/hlsl/frame_data.h>
#include <shaders/hlsl/transform_instance_data.h>
}//

