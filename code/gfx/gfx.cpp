#include "gfx.h"
#include "gfx_camera.h"

#include <rdi_backend/rdi_backend.h>
#include <rdix/rdix.h>

#include <foundation/string_util.h>
#include <foundation/id_table.h>
#include <mutex>

#include "gfx_shader_interop.h"

namespace gfx_shader
{
#include <shaders/hlsl/material_data.h>
}//

// ---
namespace gfx_internal
{
	template< uint32_t MAX, typename TId>
	struct IDTable
	{
		typedef TId IdType;

		std::mutex _idlock;
		id_table_t<MAX> _id;

		char* _name[MAX] = {};
		BXIAllocator* _name_allocator = nullptr;

		TId CreateId()
		{
			_idlock.lock();
			id_t id = id_table::create( _id );
			_idlock.unlock();

			return { id.hash };
		}
		void DestroyId( TId id )
		{
			id_t iid = { id.i };
			_idlock.lock();
			id_table::destroy( _id, iid );
			_idlock.unlock();
		}

		void SetName( TId id, const char* name )
		{
			id_t iid = { id.i };
			SYS_ASSERT( id_table::has( _id, iid ) );
			if( !name )
			{
				string::free_and_null( &_name[iid.index], _name_allocator );
			}
			else
			{
				_name[iid.index] = string::duplicate( _name[iid.index], name, _name_allocator );
			}
		}

	};

	struct GFXLookup
	{
		struct
		{
			static constexpr uint8_t MAX_MATERIALS = 64;
			IDTable< MAX_MATERIALS, GFXMaterialID > idtable;
			gfx_shader::Material data[MAX_MATERIALS] = {};
			RDIXResourceBinding* resources[MAX_MATERIALS] = {};
		}_material;

		struct
		{
			static const uint16_t MAX_MESHES = 1024;
			IDTable< MAX_MESHES, GFXMeshID > idtable;
			RDIXRenderSource* source[MAX_MESHES] = {};
		}_mesh;

		struct
		{
			static const uint8_t MAX_CAMERAS = 16;
			IDTable<MAX_CAMERAS, GFXCameraID> idtable;
			GFXCameraParams params[MAX_CAMERAS] = {};
			GFXCameraMatrices matrices[MAX_CAMERAS] = {};
		}_camera;
	};
	static GFXLookup g_lookup = {};
}



namespace
{
	template< typename T >
	static inline typename T::IdType CreateObject( T* container, const char* name )
	{
		T::IdType id = container->CreateId();
		container->SetName( id, name );

		return id;
	}

	template< typename T >
	static inline void DestroyObject( T* container, typename T::IdType* id )
	{
		container->SetName( *id, nullptr );
		container->DestroyId( *id );
		id[0] = makeInvalidHandle<T::IdType>();
	}
}

GFXMeshID     GFX::CreateMesh    ( const char* name ) {	return { CreateObject( &gfx_internal::g_lookup._mesh.idtable, name ) }; }
GFXCameraID	  GFX::CreateCamera  ( const char* name ) {	return { CreateObject( &gfx_internal::g_lookup._camera.idtable, name ) }; }
GFXMaterialID GFX::CreateMaterial( const char* name ) { return { CreateObject( &gfx_internal::g_lookup._material.idtable, name ) }; }

void GFX::Destroy( GFXCameraID* id ) {}
void GFX::Destroy( GFXMeshID* id ) {}
void GFX::Destroy( GFXMaterialID* id ) {}

void GFX::StartUp( const GFXDesc& desc, RDIDevice* dev, BXIFilesystem * filesystem, BXIAllocator * allocator )
{
	_allocator = allocator;
	_rdidev = dev;

	{// --- framebuffer
		RDIXRenderTargetDesc render_target_desc( desc.framebuffer_width, desc.framebuffer_height );
		render_target_desc.Texture( RDIFormat::Float4() );
		render_target_desc.Depth( RDIEType::DEPTH32F );
		_framebuffer = CreateRenderTarget( _rdidev, render_target_desc, allocator );
	}

	{// --- material
		RDIXShaderFile* shader_file = LoadShaderFile( "shader/hlsl/bin/material.shader", filesystem, allocator );

		RDIXPipelineDesc pipeline_desc = {};
		pipeline_desc.Shader( shader_file, "base" );
		_material.base = CreatePipeline( _rdidev, pipeline_desc, allocator );

		UnloadShaderFile( &shader_file, allocator );
	}

	{// --- samplers
		RDISamplerDesc desc = {};

		desc.Filter( RDIESamplerFilter::NEAREST );
		_samplers._point = CreateSampler( dev, desc );

		desc.Filter( RDIESamplerFilter::LINEAR );
		_samplers._linear = CreateSampler( dev, desc );

		desc.Filter( RDIESamplerFilter::BILINEAR_ANISO );
		_samplers._bilinear = CreateSampler( dev, desc );

		desc.Filter( RDIESamplerFilter::TRILINEAR_ANISO );
		_samplers._trilinear = CreateSampler( dev, desc );
	}
}

void GFX::ShutDown()
{
	{// --- samplers
		::Destroy( &_samplers._trilinear );
		::Destroy( &_samplers._bilinear );
		::Destroy( &_samplers._linear );
		::Destroy( &_samplers._point );
	}

	{// --- material
		DestroyPipeline( _rdidev, &_material.base, _allocator );
	}

	DestroyRenderTarget( _rdidev, &_framebuffer, _allocator );

	_rdidev = nullptr;
	_allocator = nullptr;
}

void GFX::BeginFrame( RDICommandQueue* cmdq )
{
	ClearState( cmdq );
	SetSamplers( cmdq, (RDISampler*)&_samplers._point, 0, 4, RDIEPipeline::ALL_STAGES_MASK );

}

void GFX::EndFrame( RDICommandQueue* cmdq )
{
	Swap( cmdq, _sync_interval );
}

void GFX::RasterizeFramebuffer( RDICommandQueue* cmdq, uint32_t texture_index, float aspect )
{
	GFXUtils::CopyTexture( cmdq, nullptr, Texture( _framebuffer, 0 ), aspect );
}















namespace GFXUtils
{
	struct Data
	{
		BXIAllocator* allocator = nullptr;
		struct
		{
			RDIXPipeline* copy_rgba = nullptr;
		}pipeline;
	};
	static Data g_utils = {};

	// ---
	void StartUp( RDIDevice* dev, BXIFilesystem* filesystem, BXIAllocator* allocator )
	{
		g_utils.allocator = allocator;
		RDIXShaderFile* shader_file = LoadShaderFile( "shader/hlsl/bin/texture_util.shader", filesystem, allocator );

		RDIXPipelineDesc pipeline_desc = {};
		pipeline_desc.Shader( shader_file, "copy_rgba" );
		g_utils.pipeline.copy_rgba = CreatePipeline( dev, pipeline_desc, allocator );

		UnloadShaderFile( &shader_file, allocator );
	}

	void ShutDown( RDIDevice* dev )
	{
		DestroyPipeline( dev, &g_utils.pipeline.copy_rgba, g_utils.allocator );

		g_utils.allocator = nullptr;
	}

	void CopyTexture( RDICommandQueue * cmdq, RDITextureRW * output, const RDITextureRW & input, float aspect )
	{
		const RDITextureInfo& src_info = input.info;
		int32_t viewport[4] = {};
		RDITextureInfo dst_info = {};

		if( output )
		{
			dst_info = input.info;
		}
		else
		{
			RDITextureRW back_buffer = GetBackBufferTexture( cmdq );
			dst_info = back_buffer.info;
		}

		ComputeViewport( viewport, aspect, dst_info.width, dst_info.height, src_info.width, src_info.height );

		RDIXResourceBinding* resources = ResourceBinding( g_utils.pipeline.copy_rgba );
		SetResourceRO( resources, "texture0", &input );
		if( output )
			ChangeRenderTargets( cmdq, output, 1, RDITextureDepth(), false );
		else
			ChangeToMainFramebuffer( cmdq );

		SetViewport( cmdq, RDIViewport::Create( viewport ) );
		BindPipeline( cmdq, g_utils.pipeline.copy_rgba, true );
		Draw( cmdq, 6, 0 );
	}
}

