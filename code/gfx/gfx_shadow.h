#pragma once

#include "gfx_forward_decl.h"
#include "gfx_camera.h"
#include "rdi_backend\rdi_backend_type.h"

struct RDIXCommandBuffer;
struct GFXShadowData
{
    GFXCameraID id_camera{ 0 };
    RDITextureRW shadow_map = {};
    RDIXCommandBuffer* cmd_buffer = nullptr;

    struct Setup
    {
        uint16_t width = 2048;
        uint32_t height = 2048;
    };
    Setup setup;
};
void GenerateCommandBuffer( GFXShadowData* sdata, GFXSystem* gfx, GFXSceneID idscene, const vec3_t& light_dir );