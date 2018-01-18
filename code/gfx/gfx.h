#pragma once

#include "dll_interface.h"
#include <foundation/type.h>
#include <foundation/math/vmath.h>
#include <foundation/containers.h>
#include <util/bbox.h>

#include <rdi_backend/rdi_backend_type.h>

#include <mutex>

#include "gfx_shader_interop.h"
namespace gfx_shader
{
#include <shaders/hlsl/samplers.h>
#include <shaders/hlsl/material_data.h>
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
struct GFXMeshInstanceID { uint32_t i; };

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

struct GFXMeshDesc
{
    const char* filename = nullptr;
    RDIXRenderSource* rsouce = nullptr;
};

struct GFXMaterialDesc
{
    const char* filename = nullptr;
    gfx_shader::Material data = {};
};

struct GFXSceneDesc
{
    const char* name = "scene";
    uint32_t max_renderables = 1024;
};

using GFXDrawCallback = void( RDICommandQueue* cmdq, void* userdata );
struct GFXMeshInstanceDesc
{
    GFXMeshID idmesh = { 0 };
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
    static GFX* Allocate( BXIAllocator* allocator );
    static void Free( GFX** gfx, BXIAllocator* allocator );

    void StartUp( RDIDevice* dev, const GFXDesc& desc, BXIFilesystem* filesystem, BXIAllocator* allocator );
    void ShutDown();

    RDIXRenderTarget* Framebuffer();
    RDIXPipeline*     MaterialBase();

    // --- mesh management
    GFXMeshID CreateMesh( const GFXMeshDesc& desc );
    void      DestroyMesh( GFXMeshID idmesh );

    // --- material system
    GFXMaterialID        CreateMaterial( const char* name, const GFXMaterialDesc& desc );
    void                 DestroyMaterial( GFXMaterialID idmat );
    GFXMaterialID        FindMaterial( const char* name );
    bool                 IsMaterialAlive( GFXMaterialID idmat );
    RDIXResourceBinding* MaterialBinding( GFXMaterialID idmat );

    // --- scenes
    GFXSceneID CreateScene( const GFXSceneDesc& desc );
    void       DestroyScene( GFXSceneID idscene );

    GFXMeshInstanceID AddMeshToScene( GFXSceneID idscene, const GFXMeshInstanceDesc& desc, const mat44_t& pose );
    void              RemoveMeshFromScene( GFXMeshInstanceID idmeshi );

    // --- frame
    GFXFrameContext* BeginFrame( RDICommandQueue* cmdq );
    void             EndFrame( GFXFrameContext* fctx );
    void             RasterizeFramebuffer( RDICommandQueue* cmdq, uint32_t texture_index, float aspect );

    void SetCamera( const GFXCameraParams& camerap, const GFXCameraMatrices& cameram );
    void BindMaterialFrame( GFXFrameContext* fctx );
    void DrawScene( GFXFrameContext* fctx, GFXSceneID idscene, const GFXCameraParams& camerap, const GFXCameraMatrices& cameram );

    // --- private data
    GFXSystem* gfx;
    GFXUtils* utils;
};

