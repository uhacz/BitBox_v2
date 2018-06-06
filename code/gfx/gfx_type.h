#pragma once

#include "gfx_forward_decl.h"
#include "gfx_shader_interop.h"
#include <resource_manager/resource_manager.h>

struct GFXDesc
{
    uint16_t framebuffer_width = 1920;
    uint16_t framebuffer_height = 1080;
};

struct GFXMaterialTexture
{
    RSMResourceID id[GFXEMaterialTextureSlot::_COUNT_] = {};
};

struct GFXMaterialDesc
{
    gfx_shader::Material data = {};
    GFXMaterialTexture textures = {};
};

struct GFXSkyParams
{
    vec3_t sun_dir = vec3_t( 0.f, -1.f, 0.f );
    float sun_intensity = 1.f;
    float sky_intensity = 1.f;
};

struct GFXSceneDesc
{
    const char* name = "scene";
    uint32_t max_renderables = 1024;
};

using GFXDrawCallback = void( RDICommandQueue* cmdq, void* userdata );
struct GFXMeshInstanceDesc
{
    RSMResourceID idmesh_resource = { 0 };
    GFXMaterialID idmaterial = { 0 };

    GFXDrawCallback* callback = nullptr;
    GFXERenderMask::E rmask = GFXERenderMask::COLOR_SHADOW;
};

