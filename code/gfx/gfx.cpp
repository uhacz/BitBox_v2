#include "gfx.h"
#include "gfx_camera.h"

#include <rdi_backend/rdi_backend.h>
#include <rdix/rdix.h>

#include <foundation/string_util.h>
#include <foundation/id_table.h>
#include <foundation/id_array.h>
#include <foundation/array.h>
#include <util/bbox.h>
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

		uint32_t Index( TId id )
		{
			SYS_ASSERT( IsAlive( id ) );
			id_t iid = { id.i };
			return iid.index;
		}
		
		bool IsAlive( TId id )
		{
			id_t iid = { id.i };
			return id_table::has( _id, iid );
		}

		TId CreateId()
		{
			_idlock.lock();
			id_t id = id_table::create( _id );
			_idlock.unlock();

			return { id.hash };
		}

        TId InvalidateId( TId id )
        {
            id_t iid = { id.i };
            _idlock.lock();
            iid = id_table::invalidate( _id, iid );
            _idlock.unlock();

            return { iid.hash };
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

    struct DeadID
    {
        uintptr_t type = 0;
        uint32_t id = 0;
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
			AABB local_aabb[MAX_MESHES] = {};
		}_mesh;

		struct
		{
			static const uint8_t MAX_CAMERAS = 16;
			IDTable<MAX_CAMERAS, GFXCameraID> idtable;
			GFXCameraParams params[MAX_CAMERAS] = {};
			GFXCameraMatrices matrices[MAX_CAMERAS] = {};
		}_camera;

        std::mutex _to_destroy_lock;
        array_t<DeadID> _to_destroy;
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
	static inline void DestroyObject( T* container, typename T::IdType* id, uintptr_t type )
	{
        container->InvalidateId( *id );
        gfx_internal::DeadID deadid;
        deadid.type = type;
        deadid.id = id->i;

        gfx_internal::g_lookup._to_destroy_lock.lock();
        array::push_back( gfx_internal::g_lookup._to_destroy, deadid );
        gfx_internal::g_lookup._to_destroy_lock.unlock();

		id[0] = makeInvalidHandle<T::IdType>();
	}

	static inline bool	   IsAlive( GFXMeshID id )      { return gfx_internal::g_lookup._mesh.idtable.IsAlive( id ); }
	static inline uint32_t Index  ( GFXMeshID id )      { return gfx_internal::g_lookup._mesh.idtable.Index( id ); }
	static inline bool	   IsAlive( GFXMaterialID id )	{ return gfx_internal::g_lookup._material.idtable.IsAlive( id ); }
	static inline uint32_t Index  ( GFXMaterialID id )	{ return gfx_internal::g_lookup._material.idtable.Index( id ); }
	static inline bool	   IsAlive( GFXCameraID id )    { return gfx_internal::g_lookup._camera.idtable.IsAlive( id ); }
	static inline uint32_t Index  ( GFXCameraID id )    { return gfx_internal::g_lookup._camera.idtable.Index( id ); }
}

GFXMeshID     GFX::CreateMesh    ( const char* name ) {	return { CreateObject( &gfx_internal::g_lookup._mesh.idtable, name ) }; }
GFXCameraID	  GFX::CreateCamera  ( const char* name ) {	return { CreateObject( &gfx_internal::g_lookup._camera.idtable, name ) }; }
GFXMaterialID GFX::CreateMaterial( const char* name ) { return { CreateObject( &gfx_internal::g_lookup._material.idtable, name ) }; }

void GFX::DestroyCamera  ( GFXCameraID* id )   { DestroyObject( &gfx_internal::g_lookup._camera.idtable, id, (uintptr_t)GFX::DestroyCamera ); }
void GFX::DestroyMesh    ( GFXMeshID* id )     { DestroyObject( &gfx_internal::g_lookup._mesh.idtable, id, (uintptr_t)GFX::DestroyMesh ); }
void GFX::DestroyMaterial( GFXMaterialID* id ) { DestroyObject( &gfx_internal::g_lookup._material.idtable, id, (uintptr_t)GFX::DestroyMaterial ); }

GFXMeshInstanceID GFX::Add( GFXMeshID idmesh, GFXMaterialID idmat, uint32_t ninstances, uint8_t rendermask )
{
    if( !IsAlive( idmesh ) || !IsAlive( idmat ) || !ninstances )
        return { 0 };

	_mesh.idlock.lock();
	id_t id = id_array::create( _mesh.id_alloc );
	_mesh.idlock.unlock();

    const uint32_t index = id_array::index( _mesh.id_alloc, id );
    
    _mesh.matrices[index].count = ninstances;
    if( ninstances == 1 )
        _mesh.matrices[index].data = &_mesh._single_world_matrix[index];
    else
        _mesh.matrices[index].data = (mat44_t*)BX_MALLOC( _mesh._matrix_allocator, ninstances * sizeof( mat44_t ), 16 );

    const uint32_t mesh_index = Index( idmesh );
    _mesh.local_aabb[index] = gfx_internal::g_lookup._mesh.local_aabb[mesh_index];

    _mesh.render_mask[index] = rendermask;
	_mesh.mesh_id[index] = idmesh;
    _mesh.material_id[index] = idmat;
    _mesh.self_id[index] = { id.hash };

    return { id.hash };
}

void GFX::Remove( GFXMeshInstanceID id )
{
    const id_t iid = { id.i };
    
    _mesh.idlock.lock();
    if( id_array::has( _mesh.id_alloc, iid ) )
    {
        uint32_t index = id_array::index( _mesh.id_alloc, iid );
        _mesh.self_id[index].i = id_array::invalidate( _mesh.id_alloc, iid ).hash;
        array::push_back( _mesh._todestroy, uint16_t( index ) );
    }
    _mesh.idlock.unlock();
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

	{
		_mesh._matrix_allocator = _allocator;
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

