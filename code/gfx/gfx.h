#pragma once

#include "dll_interface.h"
#include <foundation/type.h>
#include <foundation/math/vmath.h>
#include <foundation/containers.h>
#include <util/bbox.h>

#include <rdi_backend/rdi_backend_type.h>
#include <resource_manager/resource_manager.h>

#include <mutex>

#include "gfx_shader_interop.h"
#include <rtti/rtti.h>

namespace gfx_shader
{
#include <shaders/hlsl/samplers.h>
#include <shaders/hlsl/material_data.h>
#include <shaders/hlsl/material_frame_data.h>
#include <shaders/hlsl/transform_instance_data.h>
}//

struct BXIFilesystem;
struct BXIAllocator;

struct RDIDevice;
struct RDICommandQueue;
struct RDITextureRW;

struct RDIXPipeline;
struct RDIXResourceBinding;
struct RDIXRenderTarget;
struct RDIXRenderSource;
struct RDIXCommandBuffer;

struct GFXCameraParams;
struct GFXCameraMatrices;
struct GFXFrameContext;
struct GFXSystem;

struct GFXSceneID        { uint32_t i; };
struct GFXCameraID       { uint32_t i; };
struct GFXMeshID         { uint32_t i; };
struct GFXMaterialID     { uint32_t i; };
struct GFXMeshInstanceID { uint64_t i; };

struct GFXDesc
{
	uint16_t framebuffer_width = 1920;
	uint16_t framebuffer_height = 1080;
};

namespace GFXERenderMask
{
    enum E : uint8_t
    {
        COLOR = BIT_OFFSET( 0 ),
        SHADOW = BIT_OFFSET( 1 ),
        
        COLOR_SHADOW = COLOR | SHADOW,
    };
}//

struct GFXMaterialTexture
{
    enum E : uint8_t
    {
        BASE_COLOR = 0,
        NORMAL,
        ROUGHNESS,
        METALNESS,
        _COUNT_,
    };
    RSMResourceID id[_COUNT_] = {};
};

struct GFXMaterialDesc
{
    gfx_shader::Material data = {};
    GFXMaterialTexture textures = {};
};

struct GFXSkyParams
{
    vec3_t sun_dir = normalize( vec3_t( 5.f, -5.f, 0.f ) );
    float sun_intensity = 1.f;
    float sky_intensity = 0.1f;
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

struct GFXUtilsData;
struct GFXUtils
{
    void StartUp( RDIDevice* dev, BXIFilesystem* filesystem, BXIAllocator* allocator );
    void ShutDown( RDIDevice* dev );
    
    void CopyTexture( RDICommandQueue* cmdq, RDITextureRW* output, const RDITextureRW& input, float aspect );

    GFXUtilsData* data = nullptr;
};

struct GFX_EXPORT GFX
{
    static GFX* StartUp( RDIDevice* dev, RSM* rsm, const GFXDesc& desc, BXIFilesystem* filesystem, BXIAllocator* allocator );
    static void ShutDown( GFX**gfx, RSM* rsm );

    RDIXRenderTarget* Framebuffer();
    RDIXPipeline*     MaterialBase();
    RDIDevice*        Device();

    // --- material system
    GFXMaterialID        CreateMaterial( const char* name, const GFXMaterialDesc& desc );
    void                 DestroyMaterial( GFXMaterialID idmat );
    void                 SetMaterialData( GFXMaterialID idmat, const gfx_shader::Material& data );
    GFXMaterialID        FindMaterial( const char* name );
    bool                 IsMaterialAlive( GFXMaterialID idmat );
    RDIXResourceBinding* MaterialBinding( GFXMaterialID idmat );

    // --- scenes
    GFXSceneID CreateScene( const GFXSceneDesc& desc );
    void       DestroyScene( GFXSceneID idscene );

    GFXMeshInstanceID AddMeshToScene( GFXSceneID idscene, const GFXMeshInstanceDesc& desc, const mat44_t& pose );
    void              RemoveMeshFromScene( GFXMeshInstanceID idmeshi );
    RSMResourceID     Mesh( GFXMeshInstanceID idmeshi );
    void              SetWorldPose( GFXMeshInstanceID idmeshi, const mat44_t& pose );

    // --- sky
    void EnableSky( GFXSceneID idscene, bool value );
    bool SetSkyTextureDDS( GFXSceneID idscene, const void* data, uint32_t size );
    void SetSkyParams( GFXSceneID idscene, const GFXSkyParams& params );
    const GFXSkyParams& SkyParams( GFXSceneID idscene ) const;

    // --- frame
    GFXFrameContext* BeginFrame( RDICommandQueue* cmdq, RSM* rsm );
    void             EndFrame( GFXFrameContext* fctx );
    void             RasterizeFramebuffer( RDICommandQueue* cmdq, uint32_t texture_index, float aspect );

    void SetCamera( const GFXCameraParams& camerap, const GFXCameraMatrices& cameram );
    void BindMaterialFrame( GFXFrameContext* fctx );
    void GenerateCommandBuffer( GFXFrameContext* fctx, GFXSceneID idscene, const GFXCameraParams& camerap, const GFXCameraMatrices& cameram );
    void SubmitCommandBuffer( GFXFrameContext* fctx, GFXSceneID idscene );

    void PostProcess( GFXFrameContext* fctx, const GFXCameraParams& camerap, const GFXCameraMatrices& cameram );

    // --- private data
    GFXSystem* gfx;
    GFXUtils* utils;
};

