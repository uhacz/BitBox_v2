#include "gfx.h"
#include "gfx_camera.h"

#include <rdi_backend/rdi_backend.h>
#include <rdix/rdix.h>

#include <foundation/string_util.h>
#include <foundation/id_table.h>
#include <foundation/id_array.h>
#include <foundation/array.h>
#include <foundation/dense_container.h>
#include <foundation/thread/mutex.h>
#include <util/bbox.h>
#include <mutex>

#include "gfx_shader_interop.h"

namespace gfx_shader
{
#include <shaders/hlsl/material_data.h>
#include <shaders/hlsl/material_frame_data.h>
}//

struct GFXFrameContext
{
    RDICommandQueue* cmdq = nullptr;

    bool Valid() const { return cmdq != nullptr; }
};

struct GFXSystem
{
    static constexpr uint32_t MAX_MESHES = 1024;
    static constexpr uint32_t MAX_MATERIALS = 64;

    BXIAllocator* _allocator = nullptr;
    RDIDevice*	  _rdidev = nullptr;

    RDIXRenderTarget* _framebuffer = nullptr;
    uint32_t _sync_interval = 0;

    struct Materials
    {
        RDIXPipeline* base = nullptr;
        RDIConstantBuffer frame_data_gpu = {};

        mutex_t                   lock;
        id_table_t<MAX_MATERIALS> idtable;
        gfx_shader::Material      data[MAX_MATERIALS] = {};
        RDIConstantBuffer         data_gpu[MAX_MATERIALS] = {};
        RDIXResourceBinding*      binding[MAX_MATERIALS] = {};

        // material map
        struct  
        {
            id_t     id[MAX_MATERIALS] = {};
            string_t name[MAX_MATERIALS] = {};
            uint32_t count = 0;
        } lookup;


        mutex_t to_remove_lock;
        array_t<id_t> to_remove;

    }_material;

    struct Meshes
    {
        enum EMeshFlag : uint8_t
        {
            FROM_FILE = BIT_OFFSET(0),
        };

        mutex_t lock;
        id_table_t<MAX_MESHES> idtable;
        RDIXRenderSource* rsource[MAX_MESHES] = {};
        uint8_t flags[MAX_MESHES] = {};

        mutex_t to_remove_lock;
        array_t<id_t> to_remove;
    } _mesh;

    gfx_shader::ShaderSamplers _samplers;
};

static GFXSystem* g_gfx = nullptr;
static GFXFrameContext* g_framectx = nullptr;

namespace gfx_internal
{
    void RemovePendingObjects()
    {
        {// -- meshes
            GFXSystem::Meshes& mesh = g_gfx->_mesh;
            mesh.to_remove_lock.lock();
            while( !array::empty( mesh.to_remove ) )
            {
                id_t id = array::back( mesh.to_remove );
                array::pop_back( mesh.to_remove );
                if( mesh.flags[id.index] & GFXSystem::Meshes::FROM_FILE )
                {
                    SYS_NOT_IMPLEMENTED;
                }
                mesh.rsource[id.index] = nullptr;
                mesh.flags[id.index] = 0;
                id_table::destroy( mesh.idtable, id );
            }
            mesh.to_remove_lock.unlock();
        }
    }

    bool IsMaterialAlive( id_t id )
    {
        return id_table::has( g_gfx->_material.idtable, id );
    }
    id_t FindMaterial( const char* name )
    {
        const GFXSystem::Materials& materials = g_gfx->_material;
        for( uint32_t i = 0; i < materials.lookup.count; ++i )
        {
            id_t id = materials.lookup.id[i];
            if( string::equal( materials.lookup.name[i].c_str(), name ) )
                return id;
        }
        return makeInvalidHandle<id_t>();
    }

    RDIXResourceBinding* GetMaterialBinding( id_t id )
    {
        if( !IsMaterialAlive( id ) )
            return nullptr;

        return g_gfx->_material.binding[id.index];
    }
}

void GFXStartUp( RDIDevice* dev, const GFXDesc& desc, BXIFilesystem* filesystem, BXIAllocator* allocator )
{
    GFXSystem* gfx = BX_NEW( allocator, GFXSystem );
    gfx->_allocator = allocator;
    gfx->_rdidev = dev;

    {// --- framebuffer
        RDIXRenderTargetDesc render_target_desc( desc.framebuffer_width, desc.framebuffer_height );
        render_target_desc.Texture( RDIFormat::Float4() );
        render_target_desc.Depth( RDIEType::DEPTH32F );
        gfx->_framebuffer = CreateRenderTarget( dev, render_target_desc, allocator );
    }

    {// --- material
        GFXSystem::Materials& materials = g_gfx->_material;

        RDIXShaderFile* shader_file = LoadShaderFile( "shader/hlsl/bin/material.shader", filesystem, allocator );

        RDIXPipelineDesc pipeline_desc = {};
        pipeline_desc.Shader( shader_file, "base" );
        materials.base = CreatePipeline( dev, pipeline_desc, allocator );

        UnloadShaderFile( &shader_file, allocator );

        materials.frame_data_gpu = CreateConstantBuffer( dev, sizeof( gfx_shader::MaterialFrameData ) );
    }

    {// --- samplers
        RDISamplerDesc desc = {};

        desc.Filter( RDIESamplerFilter::NEAREST );
        gfx->_samplers._point = CreateSampler( dev, desc );

        desc.Filter( RDIESamplerFilter::LINEAR );
        gfx->_samplers._linear = CreateSampler( dev, desc );

        desc.Filter( RDIESamplerFilter::BILINEAR_ANISO );
        gfx->_samplers._bilinear = CreateSampler( dev, desc );

        desc.Filter( RDIESamplerFilter::TRILINEAR_ANISO );
        gfx->_samplers._trilinear = CreateSampler( dev, desc );
    }

    g_gfx = gfx;
    g_framectx = BX_NEW( allocator, GFXFrameContext );
}


void GFXShutDown()
{
    if( !g_gfx )
        return;

    GFXSystem* gfx = g_gfx;
    RDIDevice* dev = gfx->_rdidev;
    BXIAllocator* allocator = gfx->_allocator;

    SYS_ASSERT( g_framectx->Valid() == false );
    BX_DELETE0( allocator, g_framectx );

    g_gfx = nullptr;

    {// --- samplers
        ::Destroy( &gfx->_samplers._trilinear );
        ::Destroy( &gfx->_samplers._bilinear );
        ::Destroy( &gfx->_samplers._linear );
        ::Destroy( &gfx->_samplers._point );
    }

    {// --- material
        ::Destroy( &g_gfx->_material.frame_data_gpu );
        DestroyPipeline( dev, &gfx->_material.base, allocator );
    }

    DestroyRenderTarget( dev, &gfx->_framebuffer,  allocator );
    
    BX_DELETE( allocator, gfx );
}

GFXMeshID CreateMesh( const GFXMeshDesc& desc )
{
    g_gfx->_mesh.lock.lock();
    id_t id = id_table::create( g_gfx->_mesh.idtable );
    g_gfx->_mesh.lock.unlock();

    SYS_ASSERT( desc.filename || desc.rsouce );

    if( desc.filename )
    {
        SYS_NOT_IMPLEMENTED;
    }
    else
    {
        g_gfx->_mesh.rsource[id.index] = desc.rsouce;
    }

    return { id.hash };
}

void DestroyMesh( GFXMeshID idmesh )
{
    id_t id = { idmesh.i };
    if( !id_table::has( g_gfx->_mesh.idtable, id ) )
        return;

    g_gfx->_mesh.lock.lock();
    id = id_table::invalidate( g_gfx->_mesh.idtable, id );
    g_gfx->_mesh.lock.unlock();

    g_gfx->_mesh.to_remove_lock.lock();
    array::push_back( g_gfx->_mesh.to_remove, id );
    g_gfx->_mesh.to_remove_lock.unlock();
}

GFXMaterialID CreateMaterial( const GFXMaterialDesc& desc )
{

}

void DestroyMaterial( GFXMaterialID idmat )
{

}



GFXFrameContext* BeginFrame( RDICommandQueue* cmdq )
{
    gfx_internal::RemovePendingObjects();

    SYS_ASSERT( !g_framectx->Valid() );
    g_framectx->cmdq = cmdq;
    return g_framectx;
}

void EndFrame( GFXFrameContext** fctx )
{
    ::Swap( g_framectx->cmdq, g_gfx->_sync_interval );

    g_framectx->cmdq = nullptr;
    fctx[0] = nullptr;
}



#if 0
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
	static GFXLookup* g_lookup = {};
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

        gfx_internal::g_lookup->_to_destroy_lock.lock();
        array::push_back( gfx_internal::g_lookup->_to_destroy, deadid );
        gfx_internal::g_lookup->_to_destroy_lock.unlock();

		id[0] = makeInvalidHandle<T::IdType>();
	}

	static inline bool	   IsAlive( GFXMeshID id )      { return gfx_internal::g_lookup->_mesh.idtable.IsAlive( id ); }
	static inline uint32_t Index  ( GFXMeshID id )      { return gfx_internal::g_lookup->_mesh.idtable.Index( id ); }
	static inline bool	   IsAlive( GFXMaterialID id )	{ return gfx_internal::g_lookup->_material.idtable.IsAlive( id ); }
	static inline uint32_t Index  ( GFXMaterialID id )	{ return gfx_internal::g_lookup->_material.idtable.Index( id ); }
	static inline bool	   IsAlive( GFXCameraID id )    { return gfx_internal::g_lookup->_camera.idtable.IsAlive( id ); }
	static inline uint32_t Index  ( GFXCameraID id )    { return gfx_internal::g_lookup->_camera.idtable.Index( id ); }
}

GFXMeshID     GFX::CreateMesh    ( const char* name ) {	return { CreateObject( &gfx_internal::g_lookup->_mesh.idtable, name ) }; }
GFXCameraID	  GFX::CreateCamera  ( const char* name ) {	return { CreateObject( &gfx_internal::g_lookup->_camera.idtable, name ) }; }
GFXMaterialID GFX::CreateMaterial( const char* name ) { return { CreateObject( &gfx_internal::g_lookup->_material.idtable, name ) }; }

void GFX::DestroyCamera  ( GFXCameraID* id )   { DestroyObject( &gfx_internal::g_lookup->_camera.idtable, id, (uintptr_t)GFX::DestroyCamera ); }
void GFX::DestroyMesh    ( GFXMeshID* id )     { DestroyObject( &gfx_internal::g_lookup->_mesh.idtable, id, (uintptr_t)GFX::DestroyMesh ); }
void GFX::DestroyMaterial( GFXMaterialID* id ) { DestroyObject( &gfx_internal::g_lookup->_material.idtable, id, (uintptr_t)GFX::DestroyMaterial ); }

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
    _mesh.local_aabb[index] = gfx_internal::g_lookup->_mesh.local_aabb[mesh_index];

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

        dense_container_desc_t desc = {};
        desc.add_stream< mat44_t >( "SINGLE_WORLD_MATRIX" );
        desc.add_stream< MeshMatrix >( "MATRICES" );
        desc.add_stream< AABB >( "LOCAL_AABB" );
        desc.add_stream< uint8_t >( "RENDER_MASK" );
        desc.add_stream< GFXMeshID >( "MESH_ID" );
        desc.add_stream< GFXMaterialID >( "MATERIAL_ID" );
        desc.add_stream< GFXMeshInstanceID >( "SELF_ID" );

        _mesh.container = dense_container::create<Mesh::MAX_MESH_INSTANCES>( desc, allocator );
	}
}

void GFX::ShutDown()
{
    {
        dense_container::destroy( &_mesh.container );
    }
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

#endif














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
