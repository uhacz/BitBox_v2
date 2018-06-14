#pragma once

#include "dll_interface.h"
#include "gfx_type.h"
#include "gfx_camera.h"
#include <resource_manager/resource_manager.h>

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

    // --- camera system
    GFXCameraID         CreateCamera( const char* name, const GFXCameraParams& params, const mat44_t& world = mat44_t::identity() );
    void                DestroyCamera( GFXCameraID idcam );
    GFXCameraID         FindCamera( const char* name ) const;
    bool                IsCameraAlive( GFXCameraID idcam );
    const GFXCameraParams&   CameraParams( GFXCameraID idcam ) const;
    const GFXCameraMatrices& CameraMatrices( GFXCameraID idcam ) const;
    void                SetCameraParams( GFXCameraID idcam, const GFXCameraParams& params );
    void                SetCameraWorld( GFXCameraID idcam, const mat44_t& world );
    void                ComputeCamera( GFXCameraID idcam, GFXCameraMatrices* output = nullptr );

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
    void             RasterizeFramebuffer( RDICommandQueue* cmdq, uint32_t texture_index, GFXCameraID idcamera );

    void GenerateCommandBuffer( GFXFrameContext* fctx, GFXSceneID idscene, GFXCameraID idcamera );
    void SubmitCommandBuffer( GFXFrameContext* fctx, GFXSceneID idscene );

    void PostProcess( GFXFrameContext* fctx, GFXCameraID idcamera );

    // --- private data
    GFXSystem* gfx;
    GFXUtils* utils;
};

