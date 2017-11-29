#pragma once

#include <foundation/type.h>
#include <foundation/math/vmath.h>

#include <rdi_backend/rdi_backend_type.h>

namespace gfx_shader
{
#include <shaders/hlsl/samplers.h>
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

struct GFXCameraID   { uint32_t i; };
struct GFXLightID    { uint32_t i; };
struct GFXMeshID     { uint32_t i; };
struct GFXMaterialID { uint32_t i; };
struct GFXMeshInstanceID { uint32_t i; };

struct GFXDesc
{
	uint16_t framebuffer_width = 1920;
	uint16_t framebuffer_height = 1080;
};

class GFX
{
public:
	static GFXCameraID	 CreateCamera( const char* name );
	static GFXLightID	 CreateLight( const char* name );
	static GFXMeshID	 CreateMesh( const char* name );
	static GFXMaterialID CreateMaterial( const char* name );

	static void Destroy( GFXCameraID* id );
	static void Destroy( GFXLightID* id );
	static void Destroy( GFXMeshID* id );
	static void Destroy( GFXMaterialID* id );
	
	GFXMeshInstanceID Add( GFXMeshID idmesh, GFXMaterialID idmat, uint32_t ninstances );
	void Remove( GFXMeshInstanceID id );

	void SetRenderMask( GFXMeshInstanceID id, uint8_t mask );
	void SetAABB( GFXMeshInstanceID id );


public:
	void StartUp( const GFXDesc& desc, RDIDevice* dev, BXIFilesystem* filesystem, BXIAllocator* allocator );
	void ShutDown();


	RDIXRenderTarget* Framebuffer() { return _framebuffer; }
	RDIXPipeline* MaterialBase() { return _material.base; }

	void BeginFrame( RDICommandQueue* cmdq );
	void EndFrame( RDICommandQueue* cmdq );
	void RasterizeFramebuffer( RDICommandQueue* cmdq, uint32_t texture_index, float aspect );

public:

private:
	BXIAllocator* _allocator = nullptr;
	RDIDevice* _rdidev = nullptr;

	RDIXRenderTarget* _framebuffer = nullptr;
	
	struct
	{
		RDIXPipeline* base = nullptr;
	}_material;
	
	gfx_shader::ShaderSamplers _samplers;

	uint32_t _sync_interval = 0;



};

// ---
namespace GFXUtils
{
	void StartUp( RDIDevice* dev, BXIFilesystem* filesystem, BXIAllocator* allocator );
	void ShutDown( RDIDevice* dev );

	void CopyTexture( RDICommandQueue* cmdq, RDITextureRW* output, const RDITextureRW& input, float aspect );
};