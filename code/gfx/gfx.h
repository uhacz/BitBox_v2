#pragma once

#include <foundation/type.h>
#include <foundation/math/vmath.h>
#include <foundation/containers.h>
#include <util/bbox.h>

#include <rdi_backend/rdi_backend_type.h>

#include <mutex>

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
struct RDIXCommandBuffer;

struct GFXCameraID   { uint32_t i; };
struct GFXMeshID     { uint32_t i; };
struct GFXMaterialID { uint32_t i; };
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

struct GFXSystem;
GFXSystem* GFXStartUp( const GFXDesc* desc, BXIAllocator* allocator );
void GFXShutDown( GFXSystem** gfx );





class GFX
{
public:
	static GFXCameraID	 CreateCamera( const char* name );
	static GFXMeshID	 CreateMesh( const char* name );
	static GFXMaterialID CreateMaterial( const char* name );

	static void DestroyCamera( GFXCameraID* id );
	static void DestroyMesh( GFXMeshID* id );
	static void DestroyMaterial( GFXMaterialID* id );
	
	GFXMeshInstanceID Add( GFXMeshID idmesh, GFXMaterialID idmat, uint32_t ninstances, uint8_t rendermask = GFXERenderMask::COLOR_SHADOW );
	void Remove( GFXMeshInstanceID id );

	void SetRenderMask( GFXMeshInstanceID id, uint8_t mask );
	void SetAABB( GFXMeshInstanceID id );
	void SetWorldMatrix( GFXMeshInstanceID instanceid, const mat44_t* matrices, uint32_t count, uint32_t startindex = 0 );

	void GenerateCommandBuffer( RDIXCommandBuffer* cmdbuffer, GFXCameraID cameraid );

public:
    GFX() {}
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
	RDIDevice*	  _rdidev = nullptr;

	RDIXRenderTarget* _framebuffer = nullptr;
	
	struct
	{
		RDIXPipeline* base = nullptr;
	}_material;
	
	struct MeshMatrix
	{
		mat44_t* data = nullptr;
		uint32_t count = 0;
	};

	struct Mesh
	{
		static constexpr uint32_t MAX_MESH_INSTANCES = 1024;
		enum EStreams : uint32_t
        {
            SINGLE_WORLD_MATRIX = 0,
            MATRICES,
            LOCAL_AABB,
            RENDER_MASK,
            MESH_ID,
            MATERIAL_ID,
            SELF_ID,
        };

        dense_container_t<MAX_MESH_INSTANCES>* container;
       
        id_array_t<MAX_MESH_INSTANCES> id_alloc;
        std::mutex idlock;
        //MeshContainer container;

		mat44_t			_single_world_matrix[MAX_MESH_INSTANCES] = {};
		MeshMatrix		matrices	        [MAX_MESH_INSTANCES] = {};
		AABB			local_aabb          [MAX_MESH_INSTANCES] = {};
		uint8_t			render_mask         [MAX_MESH_INSTANCES] = {};
        GFXMeshID	    mesh_id             [MAX_MESH_INSTANCES] = {};
        GFXMaterialID   material_id         [MAX_MESH_INSTANCES] = {};
        GFXMeshInstanceID self_id           [MAX_MESH_INSTANCES] = {};

		BXIAllocator* _matrix_allocator = nullptr;
        
        array_t<uint16_t> _todestroy;
	} _mesh;

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