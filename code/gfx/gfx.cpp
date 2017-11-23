#include "gfx.h"
#include "gfx_camera.h"

#include <rdi_backend/rdi_backend.h>
#include <rdix/rdix.h>

#include <foundation/string_util.h>
#include <foundation/id_table.h>
#include <mutex>

// ---
struct GFXMeshLookup
{
	static const uint32_t MAX_COUNT = 8*1024;
	
	std::mutex _idlock;
	id_table_t<MAX_COUNT> _id;

	RDIXRenderSource* _rsource[MAX_COUNT] = {};
	
	char* _name[MAX_COUNT] = {};
	BXIAllocator* _name_allocator = nullptr;

	id_t CreateId()
	{
		_idlock.lock();
		id_t id = id_table::create( _id );
		_idlock.unlock();

		return id;
	}
	void DestroyId( id_t id )
	{
		_idlock.lock();
		id_table::destroy( _id, id );
		_idlock.unlock();
	}

	void SetName( id_t id, const char* name )
	{
		SYS_ASSERT( id_table::has( _id, id ) );
		_name[id.index] = string::duplicate( _name[id.index], name, _name_allocator );
	}

};
static GFXMeshLookup g_mesh_lookup = {};


GFXMeshID GFX::CreateMesh( const char* name )
{
	id_t id = g_mesh_lookup.CreateId();
	g_mesh_lookup.SetName( id, name );

	return { id.id };
}

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

