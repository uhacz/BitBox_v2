#include "rdix.h"

#include <filesystem/filesystem_plugin.h>
#include <foundation/memory/memory.h>
#include <foundation/hash.h>
#include <foundation/common.h>
#include <foundation/buffer.h>

#include <rdi_backend/rdi_backend.h>

#include <algorithm>

// --- Shader
RDIXShaderFile* LoadShaderFile( const char* name, BXIFilesystem* filesystem, BXIAllocator* allocator )
{
	BXFileWaitResult load_result = filesystem->LoadFileSync( filesystem, name, BXIFilesystem::FILE_MODE_BIN, allocator );
	
	RDIXShaderFile* sfile = CreateShaderFile( load_result.file.pointer, load_result.file.size );

	filesystem->CloseFile( load_result.handle, false );

	return sfile;
}

RDIXShaderFile* CreateShaderFile( const void * data, uint32_t dataSize )
{
	RDIXShaderFile* sfile = (RDIXShaderFile*)data;
	if( sfile->tag != tag32_t( "SF01" ) )
	{
		SYS_LOG_ERROR( "ShaderFile: wrong file tag!" );
		return nullptr;
	}

	if( sfile->version != RDIXShaderFile::VERSION )
	{
		SYS_LOG_ERROR( "ShaderFile: wrong version!" );
		return nullptr;
	}
		
	return sfile;
}

void UnloadShaderFile( RDIXShaderFile** shaderfile, BXIAllocator* allocator )
{
	BX_FREE0( allocator, shaderfile[0] );
}

uint32_t GenerateShaderFileHashedName( const char * name, uint32_t version )
{
	return murmur3_hash32( name, (uint32_t)strlen( name ), version );
}

uint32_t FindShaderPass( RDIXShaderFile* shaderfile, const char* passname )
{
	const uint32_t hashed_name = GenerateShaderFileHashedName( passname, shaderfile->version );
	for( uint32_t i = 0; i < shaderfile->num_passes; ++i )
	{
		if( hashed_name == shaderfile->passes[i].hashed_name )
			return i;
	}
	return UINT32_MAX;
}


//--- Pipeline
struct RDIXPipeline
{
	RDIShaderPass pass;
	RDIHardwareState hardware_state;
	RDIInputLayout input_layout;
	RDIXResourceBinding* resources = nullptr;
	RDIETopology::Enum topology = RDIETopology::TRIANGLES;
};
RDIXPipeline* CreatePipeline( RDIDevice* dev, const RDIXPipelineDesc& desc, BXIAllocator* allocator )
{
	RDIXPipeline* impl = (RDIXPipeline*)BX_NEW( allocator, RDIXPipeline );

	const uint32_t pass_index = FindShaderPass( desc.shader_file, desc.shader_pass_name );
	SYS_ASSERT( pass_index != UINT32_MAX );

	const RDIXShaderFile::Pass& pass = desc.shader_file->passes[pass_index];
	RDIShaderPassCreateInfo pass_create_info = {};
	pass_create_info.vertex_bytecode = TYPE_OFFSET_GET_POINTER( void, desc.shader_file->passes[pass_index].offset_bytecode_vertex );
	pass_create_info.vertex_bytecode_size = pass.size_bytecode_vertex;
	pass_create_info.pixel_bytecode = TYPE_OFFSET_GET_POINTER( void, desc.shader_file->passes[pass_index].offset_bytecode_pixel );
	pass_create_info.pixel_bytecode_size = pass.size_bytecode_pixel;
	pass_create_info.reflection = nullptr;
	impl->pass = CreateShaderPass( dev, pass_create_info );
	impl->pass.vertex_input_mask = pass.vertex_layout.InputMask();

	impl->input_layout = CreateInputLayout( dev, pass.vertex_layout, impl->pass );
	const RDIHardwareStateDesc* hw_state_desc = (desc.hw_state_desc_override) ? desc.hw_state_desc_override : &pass.hw_state_desc;
	impl->hardware_state = CreateHardwareState( dev, *hw_state_desc );

	if( pass.offset_resource_descriptor )
	{
		void* src_resource_desc_ptr = TYPE_OFFSET_GET_POINTER( void, pass.offset_resource_descriptor );
		void* resource_desc_memory = BX_MALLOC( allocator, pass.size_resource_descriptor, 16 );
		memcpy( resource_desc_memory, src_resource_desc_ptr, pass.size_resource_descriptor );
		impl->resources = (RDIXResourceBinding*)resource_desc_memory;
	}

	return impl;
}
void DestroyPipeline( RDIDevice* dev, RDIXPipeline** pipeline, BXIAllocator* allocator )
{
	if( !pipeline[0] )
		return;

	RDIXPipeline* pipe = pipeline[0];
	BX_FREE0( allocator, pipe->resources );
	Destroy( &pipe->hardware_state );
	Destroy( &pipe->input_layout );
	Destroy( &pipe->pass );
}
void BindPipeline( RDICommandQueue* cmdq, RDIXPipeline* pipeline, bool bindResources )
{
	SetShaderPass( cmdq, pipeline->pass );
	SetInputLayout( cmdq, pipeline->input_layout );
	SetHardwareState( cmdq, pipeline->hardware_state );
	if( pipeline->resources && bindResources )
	{
		BindResources( cmdq, pipeline->resources );
	}

	SetTopology( cmdq, pipeline->topology );
}
RDIXResourceBinding* ResourceBinding( const RDIXPipeline* p )
{
	return p->resources;
}

//---

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
struct RDIXResourceBinding
{
	struct Binding
	{
		uint8_t binding_type = RDIXResourceSlot::_COUNT_;
		uint8_t slot = 0;
		uint16_t stage_mask = 0;
		uint32_t data_offset = 0;
	};

	const uint32_t* HashedNames() const { return (uint32_t*)(this + 1); }
	const Binding* Bindings() const { return (Binding*)(HashedNames() + count); }
	uint8_t* Data() { return (uint8_t*)(Bindings() + count); }
	
	uint32_t count = 0;
};
namespace
{
	static const uint32_t _resource_size[RDIXResourceSlot::_COUNT_] =
	{
		sizeof( RDIResourceRO ),     //eRESOURCE_TYPE_READ_ONLY,
		sizeof( RDIResourceRW ),     //eRESOURCE_TYPE_READ_WRITE,
		sizeof( RDIConstantBuffer ), //eRESOURCE_TYPE_UNIFORM,
		sizeof( RDISampler ),        //eRESOURCE_TYPE_SAMPLER,
	};
}///
RDIXResourceBinding* CreateResourceBinding( const RDIXResourceLayout& layout, BXIAllocator* allocator )
{
	RDIXResourceBindingMemoryRequirments mem_req = CalculateResourceBindingMemoryRequirments( layout );

	const uint32_t mem_size = mem_req.Total();
	uint8_t* mem = (uint8_t*)BX_MALLOC( allocator, mem_size, 16 );
	memset( mem, 0x00, mem_size );

	RDIXResourceBinding* impl = (RDIXResourceBinding*)mem;
	impl->count = layout.count;
	uint32_t* hashed_names = (uint32_t*)impl->HashedNames();
	RDIXResourceBinding::Binding* bindings = (RDIXResourceBinding::Binding*)impl->Bindings();

	SYS_ASSERT( (uintptr_t)(bindings + impl->count) == (uintptr_t)((uint8_t*)mem + mem_req.descriptor_size) );

	for( uint32_t i = 0; i < impl->count; ++i )
	{
		const RDIXResourceSlot& src_bind = layout.slots[i];
		RDIXResourceBinding::Binding& dst_bind = bindings[i];
		dst_bind.binding_type = src_bind.type;
		dst_bind.slot = src_bind.slot;
		dst_bind.stage_mask = src_bind.stage_mask;
		dst_bind.data_offset = GenerateResourceHashedName( src_bind.name );;
	}

	struct BindingCmp
	{
		inline bool operator()( RDIXResourceBinding::Binding a, RDIXResourceBinding::Binding b )
		{
			const uint32_t keya = uint32_t( a.binding_type ) << 24 | uint32_t( a.slot ) << 16 | a.stage_mask;
			const uint32_t keyb = uint32_t( b.binding_type ) << 24 | uint32_t( b.slot ) << 16 | b.stage_mask;
			return keya < keyb;
		}
	};
	std::sort( bindings, bindings + impl->count, BindingCmp() );

	uint32_t data_offset = 0;
	for( uint32_t i = 0; i < impl->count; ++i )
	{
		hashed_names[i] = bindings[i].data_offset;
		bindings[i].data_offset = data_offset;
		data_offset += _resource_size[bindings[i].binding_type];
	}

	SYS_ASSERT( data_offset == mem_req.data_size );
	SYS_ASSERT( (uintptr_t)(impl->Data() + data_offset) == (uintptr_t)((uint8_t*)mem + mem_size) );

#ifdef _DEBUG
	for( uint32_t i = 0; i < impl->count; ++i )
	{
		for( uint32_t j = i + 1; j < impl->count; ++j )
		{
			SYS_ASSERT( hashed_names[i] != hashed_names[j] );
		}
	}
#endif

	return impl;
}

void DestroyResourceBinding( RDIXResourceBinding** binding, BXIAllocator* allocator )
{
	BX_FREE0( allocator, binding[0] );
}

namespace
{
	static inline uint32_t _FindBinding( RDIXResourceBinding* binding, const char* name )
	{
		const uint32_t hashed_name = GenerateResourceHashedName( name );
		const uint32_t* resource_hashed_names = binding->HashedNames();
		for( uint32_t i = 0; i < binding->count; ++i )
		{
			if( hashed_name == resource_hashed_names[i] )
				return i;
		}
		return UINT32_MAX;
	}

	template< RDIXResourceSlot::EType B, typename T >
	void _SetResource1( RDIXResourceBinding* impl, uint32_t index, const T* resourcePtr )
	{
		SYS_ASSERT( index < impl->count );
		const RDIXResourceBinding::Binding& binding = impl->Bindings()[index];
		SYS_ASSERT( binding.binding_type == B );

		T* resource_data = (T*)(impl->Data() + binding.data_offset);
		resource_data[0] = *resourcePtr;
	}
	template< RDIXResourceSlot::EType B, typename T >
	bool _SetResource2( RDIXResourceBinding* impl, const char* name, const T* resourcePtr )
	{
		uint32_t index = _FindBinding( impl, name );
		if( index == UINT32_MAX )
		{
			SYS_LOG_ERROR( "Resource '%s' not found in descriptor", name );
			return false;
		}

		_SetResource1<B>( impl, index, resourcePtr );
		return true;
	}
}


bool ClearResource( RDICommandQueue * cmdq, RDIXResourceBinding* binding, const char * name )
{
	uint32_t index = _FindBinding( binding, name );
	if( index == UINT32_MAX )
	{
		SYS_LOG_ERROR( "Resource '%s' not found in descriptor", name );
		return false;
	}

	const RDIXResourceBinding::Binding& b = binding->Bindings()[index];
	switch( b.binding_type )
	{
	case RDIXResourceSlot::READ_ONLY:
	{
		RDIResourceRO null_resource = {};
		SetResourcesRO( cmdq, &null_resource, b.slot, 1, b.stage_mask );
	}break;
	case RDIXResourceSlot::READ_WRITE:
	{
		RDIResourceRW null_resource = {};
		SetResourcesRW( cmdq, &null_resource, b.slot, 1, b.stage_mask );
	}break;
	case RDIXResourceSlot::UNIFORM:
	{
		RDIConstantBuffer null_resource = {};
		SetCbuffers( cmdq, &null_resource, b.slot, 1, b.stage_mask );
	}break;
	case RDIXResourceSlot::SAMPLER:
	{
		RDISampler null_resource = {};
		SetSamplers( cmdq, &null_resource, b.slot, 1, b.stage_mask );
	}break;
	default:
		SYS_NOT_IMPLEMENTED;
		break;
	}
	return true;
}

void BindResources( RDICommandQueue* cmdq, RDIXResourceBinding* rbind )
{
	const RDIXResourceBinding::Binding* bindings = rbind->Bindings();
	uint8_t* data = rbind->Data();
	const uint32_t n = rbind->count;

	for( uint32_t i = 0; i < n; ++i )
	{
		const RDIXResourceBinding::Binding b = bindings[i];
		uint8_t* resource_data = data + b.data_offset;
		switch( b.binding_type )
		{
		case RDIXResourceSlot::READ_ONLY:
		{
			RDIResourceRO* r = (RDIResourceRO*)resource_data;
			SetResourcesRO( cmdq, r, b.slot, 1, b.stage_mask );
		}break;
		case RDIXResourceSlot::READ_WRITE:
		{
			RDIResourceRW* r = (RDIResourceRW*)resource_data;
			SetResourcesRW( cmdq, r, b.slot, 1, b.stage_mask );
		}break;
		case RDIXResourceSlot::UNIFORM:
		{
			RDIConstantBuffer* r = (RDIConstantBuffer*)resource_data;
			SetCbuffers( cmdq, r, b.slot, 1, b.stage_mask );
		}break;
		case RDIXResourceSlot::SAMPLER:
		{
			RDISampler* r = (RDISampler*)resource_data;
			SetSamplers( cmdq, r, b.slot, 1, b.stage_mask );
		}break;
		default:
			SYS_NOT_IMPLEMENTED;
			break;
		}
	}
}

uint32_t FindResource( RDIXResourceBinding* binding, const char* name )
{
	return _FindBinding( binding, name );
}

void SetResourceROByIndex    ( RDIXResourceBinding* rbind, uint32_t index, const RDIResourceRO* resource )     { _SetResource1<RDIXResourceSlot::READ_ONLY>( rbind, index, resource );}
void SetResourceRWByIndex    ( RDIXResourceBinding* rbind, uint32_t index, const RDIResourceRW* resource )     { _SetResource1<RDIXResourceSlot::READ_WRITE>( rbind, index, resource );}
void SetConstantBufferByIndex( RDIXResourceBinding* rbind, uint32_t index, const RDIConstantBuffer* cbuffer )  { _SetResource1<RDIXResourceSlot::UNIFORM>( rbind, index, cbuffer );}
void SetSamplerByIndex       ( RDIXResourceBinding* rbind, uint32_t index, const RDISampler* sampler )         { _SetResource1<RDIXResourceSlot::SAMPLER>( rbind, index, sampler );}
bool SetResourceRO           ( RDIXResourceBinding* rbind, const char* name, const RDIResourceRO* resource )   { return _SetResource2<RDIXResourceSlot::READ_ONLY>( rbind, name, resource );}
bool SetResourceRW           ( RDIXResourceBinding* rbind, const char* name, const RDIResourceRW* resource )   { return _SetResource2<RDIXResourceSlot::READ_WRITE>( rbind, name, resource );}
bool SetConstantBuffer       ( RDIXResourceBinding* rbind, const char* name, const RDIConstantBuffer* cbuffer ){ return _SetResource2<RDIXResourceSlot::UNIFORM>( rbind, name, cbuffer );}
bool SetSampler              ( RDIXResourceBinding* rbind, const char* name, const RDISampler* sampler )       { return _SetResource2<RDIXResourceSlot::SAMPLER>( rbind, name, sampler );}

uint32_t GenerateResourceHashedName( const char * name )
{
	return murmur3_hash32( name, (uint32_t)strlen( name ), tag32_t( "RDES" ) );
}

RDIXResourceBindingMemoryRequirments CalculateResourceBindingMemoryRequirments( const RDIXResourceLayout & layout )
{
	RDIXResourceBindingMemoryRequirments requirments = {};

	requirments.descriptor_size = sizeof( RDIXResourceBinding );
	requirments.descriptor_size += layout.count * sizeof( uint32_t ); // hashed names
	requirments.descriptor_size += layout.count * sizeof( RDIXResourceBinding::Binding );

	for( uint32_t i = 0; i < layout.count; ++i )
	{
		const RDIXResourceSlot slot = layout.slots[i];
		requirments.data_size += _resource_size[slot.type];
	}
	return requirments;
}




//---
struct RDIXRenderTarget
{
	RDITextureRW color_textures[cRDI_MAX_RENDER_TARGETS] = {};
	RDITextureDepth depth_texture;
	uint32_t num_color_textures = 0;
};
RDIXRenderTarget* CreateRenderTarget( RDIDevice* dev, const RDIXRenderTargetDesc& desc, BXIAllocator* allocator )
{
	RDIXRenderTarget* impl = BX_NEW( allocator, RDIXRenderTarget );
	for( uint32_t i = 0; i < desc.num_color_textures; ++i )
	{
		impl->color_textures[i] = CreateTexture2D( dev, desc.width, desc.height, desc.mips, desc.color_texture_formats[i], RDIEBind::RENDER_TARGET | RDIEBind::SHADER_RESOURCE, 0, nullptr );
	}
	impl->num_color_textures = desc.num_color_textures;

	if( desc.depth_texture_type != RDIEType::UNKNOWN )
	{
		impl->depth_texture = CreateTexture2Ddepth( dev, desc.width, desc.height, desc.mips, desc.depth_texture_type );
	}

	return impl;
}
void DestroyRenderTarget( RDIDevice* dev, RDIXRenderTarget** renderTarget, BXIAllocator* allocator )
{
	if( renderTarget[0] == nullptr )
		return;

	RDIXRenderTarget* impl = renderTarget[0];

	if( impl->depth_texture.id )
	{
		Destroy( &impl->depth_texture );
	}

	for( uint32_t i = 0; i < impl->num_color_textures; ++i )
	{
		Destroy( &impl->color_textures[i] );
	}

	BX_DELETE0( allocator, renderTarget[0] );
}

void ClearRenderTarget( RDICommandQueue* cmdq, RDIXRenderTarget* rtarget, float r, float g, float b, float a, float d )
{
	float rgbad[5] = { r, g, b, a, d };
	int clear_depth = (d >= 0.f) ? 1 : 0;
	ClearBuffers( cmdq, rtarget->color_textures, rtarget->num_color_textures, rtarget->depth_texture, rgbad, 1, clear_depth );
}

void ClearRenderTargetDepth( RDICommandQueue* cmdq, RDIXRenderTarget* rtarget, float d )
{
	float rgbad[5] = { 0.f, 0.f, 0.f, 0.f, d };
	ClearBuffers( cmdq, nullptr, 0, rtarget->depth_texture, rgbad, 0, 1 );
}

void BindRenderTarget( RDICommandQueue * cmdq, RDIXRenderTarget * renderTarget, const std::initializer_list<uint8_t>& colorTextureIndices, bool useDepth )
{
	SYS_ASSERT( (uint32_t)colorTextureIndices.size() < cRDI_MAX_RENDER_TARGETS );

	RDITextureRW color[cRDI_MAX_RENDER_TARGETS] = {};
	uint8_t color_array_index = 0;
	for( uint8_t i : colorTextureIndices )
	{
		SYS_ASSERT( i < renderTarget->num_color_textures );
		color[color_array_index++] = renderTarget->color_textures[i];
	}

	RDITextureDepth depth = {};
	if( useDepth )
	{
		depth = renderTarget->depth_texture;
	}

	ChangeRenderTargets( cmdq, color, (uint32_t)colorTextureIndices.size(), depth );
}

void BindRenderTarget( RDICommandQueue* cmdq, RDIXRenderTarget* renderTarget )
{
	ChangeRenderTargets( cmdq, renderTarget->color_textures, renderTarget->num_color_textures, renderTarget->depth_texture );
}

RDITextureRW Texture( RDIXRenderTarget * rtarget, uint32_t index )
{
	SYS_ASSERT( index < rtarget->num_color_textures );
	return rtarget->color_textures[index];
}

RDITextureDepth TextureDepth( RDIXRenderTarget * rtarget )
{
	return rtarget->depth_texture;
}


// --- RenderSource
struct RDIXRenderSource
{
	uint16_t num_vertex_buffers = 0;
	uint8_t num_draw_ranges = 0;
	uint8_t has_shared_index_buffer = 0;
	RDIIndexBuffer index_buffer;
	RDIVertexBuffer* vertex_buffers = nullptr;
	RDIXRenderSourceRange* draw_ranges = nullptr;
};
RDIXRenderSource* CreateRenderSource( RDIDevice* dev, const RDIXRenderSourceDesc& desc, BXIAllocator* allocator )
{
	const uint32_t num_streams = desc.vertex_layout.count;
	const uint32_t num_draw_ranges = max_of_2( 1u, desc.num_draw_ranges );

	uint32_t mem_size = sizeof( RDIXRenderSource );
	mem_size += (num_streams) * sizeof( RDIVertexBuffer );
	mem_size += num_draw_ranges * sizeof( RDIXRenderSourceRange );

	void* mem = BX_MALLOC( allocator, mem_size, ALIGNOF( RDIXRenderSource ) );
	memset( mem, 0x00, mem_size );

	BufferChunker chunker( mem, mem_size );
	RDIXRenderSource* impl = chunker.Add< RDIXRenderSource >();
	impl->vertex_buffers   = chunker.Add< RDIVertexBuffer >( num_streams );
	impl->draw_ranges      = chunker.Add< RDIXRenderSourceRange >( num_draw_ranges );
	chunker.Check();

	impl->num_vertex_buffers = num_streams;
	impl->num_draw_ranges = num_draw_ranges;

	for( uint32_t i = 0; i < num_streams; ++i )
	{
		const void* data = desc.vertex_data[i];
		impl->vertex_buffers[i] = CreateVertexBuffer( dev, desc.vertex_layout.descs[i], desc.num_vertices, data );
	}

	RDIXRenderSourceRange& default_range = impl->draw_ranges[0];
	if( desc.index_type != RDIEType::UNKNOWN )
	{
		impl->index_buffer = CreateIndexBuffer( dev, desc.index_type, desc.num_indices, desc.index_data );
		default_range.count = desc.num_indices;
		impl->has_shared_index_buffer = 0;
	}
	else if( desc.shared_index_buffer.id )
	{
		impl->index_buffer = desc.shared_index_buffer;
		impl->has_shared_index_buffer = 1;
		default_range.count = desc.num_indices;
	}
	else
	{
		default_range.count = desc.num_vertices;
	}

	for( uint32_t i = 1; i < num_draw_ranges; ++i )
	{
		impl->draw_ranges[i] = desc.draw_ranges[i - 1];
	}

	return impl;
}

void DestroyRenderSource( RDIDevice* dev, RDIXRenderSource** rsource, BXIAllocator* allocator )
{
	if( !rsource[0] )
		return;

	RDIXRenderSource* impl = rsource[0];

	if( impl->has_shared_index_buffer == 0 )
	{
		Destroy( &impl->index_buffer );
	}
	for( uint32_t i = 0; i < impl->num_vertex_buffers; ++i )
	{
		Destroy( &impl->vertex_buffers[i] );
	}

	BX_FREE0( allocator, rsource[0] );
}

void BindRenderSource( RDICommandQueue* cmdq, RDIXRenderSource* renderSource )
{
	RDIVertexBuffer* vbuffers = nullptr;
	uint32_t num_vbuffers = 0;

	RDIIndexBuffer ibuffer = {};

	if( renderSource )
	{
		vbuffers = renderSource->vertex_buffers;
		num_vbuffers = renderSource->num_vertex_buffers;
		ibuffer = renderSource->index_buffer;
	}
	SetVertexBuffers( cmdq, vbuffers, 0, num_vbuffers );
	SetIndexBuffer( cmdq, ibuffer );
}
void SubmitRenderSource( RDICommandQueue* cmdq, RDIXRenderSource* renderSource, uint32_t rangeIndex )
{
	SYS_ASSERT( rangeIndex < renderSource->num_draw_ranges );
	RDIXRenderSourceRange range = renderSource->draw_ranges[rangeIndex];

	if( renderSource->index_buffer.id )
		DrawIndexed( cmdq, range.count, range.begin, 0 );
	else
		Draw( cmdq, range.count, range.begin );
}

void SubmitRenderSourceInstanced( RDICommandQueue* cmdq, RDIXRenderSource* renderSource, uint32_t numInstances, uint32_t rangeIndex )
{
	SYS_ASSERT( rangeIndex < renderSource->num_draw_ranges );
	RDIXRenderSourceRange range = renderSource->draw_ranges[rangeIndex];

	if( renderSource->index_buffer.id )
		DrawIndexedInstanced( cmdq, range.count, range.begin, numInstances, 0 );
	else
		DrawInstanced( cmdq, range.count, range.begin, numInstances );
}

uint32_t NumVertexBuffers( RDIXRenderSource* rsource ){ return rsource->num_vertex_buffers; }
uint32_t NumVertices     ( RDIXRenderSource* rsource ){ return rsource->vertex_buffers[0].numElements; }
uint32_t NumIndices      ( RDIXRenderSource* rsource ){ return rsource->index_buffer.numElements; }
uint32_t NumRanges       ( RDIXRenderSource* rsource ){ return rsource->num_draw_ranges; }
RDIVertexBuffer VertexBuffer( RDIXRenderSource* rsource, uint32_t index )
{
	SYS_ASSERT( index < rsource->num_vertex_buffers );
	return rsource->vertex_buffers[index];
}
RDIIndexBuffer IndexBuffer( RDIXRenderSource* rsource ){ return rsource->index_buffer; }
RDIXRenderSourceRange Range( RDIXRenderSource* rsource, uint32_t index )
{
	SYS_ASSERT( index < rsource->num_draw_ranges );
	return rsource->draw_ranges[index];
}