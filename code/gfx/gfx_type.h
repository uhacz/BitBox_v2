#pragma once

#include <foundation/type.h>
#include <foundation/debug.h>
#include <foundation/tag.h>
#include <rdi_backend/rdi_backend_type.h>

// --- 
struct GFXShaderFile
{
	static const uint32_t VERSION = BX_UTIL_MAKE_VERSION( 1, 0, 0 );

	struct Pass
	{
		uint32_t hashed_name                     = 0;
		RDIHardwareStateDesc hw_state_desc = {};
		RDIVertexLayout vertex_layout      = {};
		uint32_t offset_bytecode_pixel           = 0;
		uint32_t offset_bytecode_vertex          = 0;
		uint32_t offset_resource_descriptor      = 0;
		uint32_t size_bytecode_pixel             = 0;
		uint32_t size_bytecode_vertex            = 0;
		uint32_t size_resource_descriptor        = 0;
	};

	uint32_t tag        = tag32_t( "SF01" );
	uint32_t version    = VERSION;
	uint32_t num_passes = 0;
	Pass passes[1];
};

// --- 
struct GFXPipelineDesc
{
	GFXShaderFile* shader_file	 = nullptr;
	const char* shader_pass_name = nullptr;
	RDIHardwareStateDesc* hw_state_desc_override = nullptr;
	RDIETopology::Enum topology = RDIETopology::TRIANGLES;

	GFXPipelineDesc& Shader( GFXShaderFile* f, const char* pass_name )
	{
		shader_file = f;
		shader_pass_name = pass_name;
		return *this;
	}
	GFXPipelineDesc& HardwareState( RDIHardwareStateDesc* hwdesc ) { hw_state_desc_override = hwdesc; return *this; }
	GFXPipelineDesc& Topology( RDIETopology::Enum t ) { topology = t; return *this; }
};

// ---
struct GFXResourceSlot
{
	enum EType : uint8_t
	{
		READ_ONLY,
		READ_WRITE,
		UNIFORM,
		SAMPLER,
		_COUNT_,
	};

	const char* name = nullptr;
	EType type = EType::_COUNT_;
	uint8_t slot = 0;
	uint16_t stage_mask = 0;

	GFXResourceSlot() {}
	GFXResourceSlot( const char* n, EType t, uint8_t sm, uint8_t sl, uint8_t cnt )
		: name( n ), type( t ), slot( sl ), stage_mask( sm ) {}

	GFXResourceSlot( const char* n, EType t ) { name = n;  type = t; }
	GFXResourceSlot& StageMask( uint8_t sm ) { stage_mask = sm; return *this; }
	GFXResourceSlot& Slot( uint8_t sl ) { slot = sl; return *this; }
};

// --- 
struct GFXResourceLayout
{
	const GFXResourceSlot* slots = nullptr;
	uint32_t num_bindings = 0;

	GFXResourceLayout() {}
	GFXResourceLayout( const GFXResourceSlot* b, uint32_t n )
		: slots( b ), num_bindings( n ) {}
};

// --- 
struct GFXResourceBindingMemoryRequirments
{
	uint32_t descriptor_size = 0;
	uint32_t data_size = 0;
	uint32_t Total() const { return descriptor_size + data_size; }
};

// --- 
struct GFXRenderTargetDesc
{
	uint16_t num_color_textures = 0;
	uint16_t mips               = 1;
	uint16_t width              = 0;
	uint16_t height             = 0;

	RDIFormat color_texture_formats[cRENDER_MAX_RENDER_TARGETS] = {};
	RDIEType::Enum depth_texture_type = RDIEType::UNKNOWN;

	GFXRenderTargetDesc& Size( uint16_t w, uint16_t h, uint16_t m = 1 )
	{
		width = w; height = h; mips = m;
		return *this;
	}
	GFXRenderTargetDesc& Texture( RDIFormat format )
	{
		SYS_ASSERT( num_color_textures < cRENDER_MAX_RENDER_TARGETS );
		color_texture_formats[num_color_textures++] = format;
		return *this;
	}
	GFXRenderTargetDesc& Depth( RDIEType::Enum dataType )
	{ 
		depth_texture_type = dataType; return *this; 
	}
};

// ---
struct GFXRenderSourceRange
{
	uint32_t begin = 0;
	uint32_t count = 0;
};

struct GFXRenderSourceDesc
{
	uint32_t num_vertices = 0;
	uint32_t num_indices = 0;
	uint32_t num_draw_ranges = 0;
	RDIVertexLayout vertex_layout = {};

	RDIIndexBuffer shared_index_buffer = {};
	RDIEType::Enum index_type = RDIEType::UNKNOWN;
	const void* index_data = nullptr;

	const void* vertex_data[cRENDER_MAX_VERTEX_BUFFERS] = {};

	const GFXRenderSourceRange* draw_ranges = nullptr;

	GFXRenderSourceDesc& Count( uint32_t nVertices, uint32_t nIndices = 0 )
	{
		num_vertices = nVertices;
		num_indices = nIndices;
		return *this;
	}

	GFXRenderSourceDesc& VertexBuffer( RDIVertexBufferDesc vbDesc, const void* initialData )
	{
		SYS_ASSERT( vertex_layout.count < cRENDER_MAX_VERTEX_BUFFERS );
		uint32_t index = vertex_layout.count++;
		vertex_layout.descs[index] = vbDesc;
		vertex_data[index] = initialData;
		return *this;
	}
	GFXRenderSourceDesc& IndexBuffer( RDIEType::Enum dt, const void* initialData )
	{
		SYS_ASSERT( shared_index_buffer.id == 0 );
		index_type = dt;
		index_data = initialData;
		return *this;
	}
	GFXRenderSourceDesc& SharedIndexBuffer( RDIIndexBuffer ibuffer )
	{
		SYS_ASSERT( index_type == RDIEType::UNKNOWN );
		SYS_ASSERT( index_data == nullptr );
		shared_index_buffer = ibuffer;
		return *this;
	}
};

struct GFXPipeline;
struct GFXResourceBinding;
struct GFXRenderSource;
struct GFXRenderTarget;

