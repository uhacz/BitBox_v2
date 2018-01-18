#include "gfx.h"
#include "gfx_camera.h"

#include <rdi_backend/rdi_backend.h>
#include <rdix/rdix.h>
#include <rdix/rdix_command_buffer.h>

#include <foundation/string_util.h>
#include <foundation/id_table.h>
#include <foundation/id_array.h>
#include <foundation/array.h>
#include <foundation/dense_container.h>
#include <foundation/container_soa.h>
#include <foundation/id_allocator_dense.h>
#include <foundation/thread/mutex.h>
#include <util/bbox.h>
#include <mutex>

namespace gfx_shader
{
#include <shaders/hlsl/material_frame_data.h>

}//

static constexpr uint32_t MAX_SCENES = 4;
static constexpr uint32_t MAX_MESHES = 128;
static constexpr uint32_t MAX_MESH_INSTANCES = 1024;
static constexpr uint32_t MAX_MATERIALS = 64;

struct GFXFrameContext
{
    RDICommandQueue* cmdq = nullptr;

    bool Valid() const { return cmdq != nullptr; }
};

//////////////////////////////////////////////////////////////////////////
struct GFXUtilsData
{
    BXIAllocator* allocator = nullptr;
    struct
    {
        RDIXPipeline* copy_rgba = nullptr;
    }pipeline;
};
// ---
void GFXUtils::StartUp( RDIDevice* dev, BXIFilesystem* filesystem, BXIAllocator* allocator )
{
    data->allocator = allocator;
    RDIXShaderFile* shader_file = LoadShaderFile( "shader/hlsl/bin/texture_util.shader", filesystem, allocator );

    RDIXPipelineDesc pipeline_desc = {};
    pipeline_desc.Shader( shader_file, "copy_rgba" );
    data->pipeline.copy_rgba = CreatePipeline( dev, pipeline_desc, allocator );


    UnloadShaderFile( &shader_file, allocator );
}

void GFXUtils::ShutDown( RDIDevice* dev )
{
    DestroyPipeline( dev, &data->pipeline.copy_rgba, data->allocator );

    data->allocator = nullptr;
}

void GFXUtils::CopyTexture( RDICommandQueue * cmdq, RDITextureRW * output, const RDITextureRW & input, float aspect )
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

    RDIXResourceBinding* resources = ResourceBinding( data->pipeline.copy_rgba );
    SetResourceRO( resources, "texture0", &input );
    if( output )
        ChangeRenderTargets( cmdq, output, 1, RDITextureDepth(), false );
    else
        ChangeToMainFramebuffer( cmdq );

    SetViewport( cmdq, RDIViewport::Create( viewport ) );
    BindPipeline( cmdq, data->pipeline.copy_rgba, true );
    Draw( cmdq, 6, 0 );
}

//////////////////////////////////////////////////////////////////////////
struct GFXMaterialContainer
{
    RDIXPipeline* base = nullptr;
    RDIConstantBuffer frame_data_gpu = {};

    mutex_t                   lock;
    id_table_t<MAX_MATERIALS> idtable;
    gfx_shader::Material      data[MAX_MATERIALS] = {};
    RDIConstantBuffer         data_gpu[MAX_MATERIALS] = {};
    RDIXResourceBinding*      binding[MAX_MATERIALS] = {};
    string_t                  name[MAX_MATERIALS] = {};
    id_t                      idself[MAX_MATERIALS] = {};

    mutex_t to_remove_lock;
    array_t<id_t> to_remove;

    bool IsAlive( id_t id ) const { return id_table::has( idtable, id ); }
};

struct GFXMeshContainer
{
    enum EMeshFlag : uint8_t
    {
        FROM_FILE = BIT_OFFSET( 0 ),
    };

    mutex_t lock;
    id_table_t<MAX_MESHES> idtable;
    RDIXRenderSource* rsource[MAX_MESHES] = {};
    uint8_t flags[MAX_MESHES] = {};

    mutex_t to_remove_lock;
    array_t<id_t> to_remove;

    bool IsAlive( id_t id ) const { return id_table::has( idtable, id ); }
};

struct GFXSceneContainer
{
    struct MeshData
    {
        mat44_t* world_matrix;
        uint32_t* transform_buffer_instance_index;
        GFXMeshID* idmesh;
        GFXMaterialID* idmat;
    };
    using MeshIDAllocator = id_allocator_dense_t;

    mutex_t lock;
    id_table_t<MAX_SCENES> idtable;
    
    mutex_t to_remove_lock;
    array_t<id_t> to_remove;

    RDIXTransformBuffer* transform_buffer[MAX_SCENES] = {};
    RDIXCommandBuffer*   command_buffer[MAX_SCENES] = {};

    MeshData*        mesh_data   [MAX_SCENES] = {};
    MeshIDAllocator* mesh_idalloc[MAX_SCENES] = {};
    mutex_t          mesh_lock   [MAX_SCENES] = {};

    bool IsSceneAlive( id_t id ) const { return id_table::has( idtable, id ); }
    bool IsMeshAlive( id_t idscene, id_t id ) const
    {
        return ( IsSceneAlive( idscene ) && id_allocator::has( mesh_idalloc[idscene.index], id ) );
    }
};

struct GFXSystem
{
    BXIAllocator* _allocator = nullptr;
    RDIDevice*	  _rdidev = nullptr;

    RDIXRenderTarget* _framebuffer = nullptr;
    uint32_t _sync_interval = 0;

    GFXMaterialContainer _material;
    GFXMeshContainer     _mesh;
    GFXSceneContainer    _scene;
    GFXSceneID           _scene_lookup[MAX_SCENES][MAX_MESH_INSTANCES] = {};

    GFXMaterialID _fallback_idmaterial;
    GFXMeshID     _fallback_idmesh;

    gfx_shader::ShaderSamplers _samplers;

    GFXFrameContext _frame_ctx = {};
};

namespace gfx_internal
{
    void RemovePendingObjects( GFXSystem* gfx )
    {
        {// -- scenes
            GFXSceneContainer& sc = gfx->_scene;
            std::lock_guard<mutex_t> lock_guard( sc.to_remove_lock );
            while( !array::empty( sc.to_remove ) )
            {
                id_t id = array::back( sc.to_remove );
                array::pop_back( sc.to_remove );

                DestroyCommandBuffer( &sc.command_buffer[id.index], gfx->_allocator );
                DestroyTransformBuffer( gfx->_rdidev, &sc.transform_buffer[id.index], gfx->_allocator );
                container_soa::destroy( &sc.mesh_data[id.index] );
                id_allocator::destroy( &sc.mesh_idalloc[id.index] );

                {
                    std::lock_guard<mutex_t> lck( sc.lock );
                    id_table::destroy( sc.idtable, id );
                }
            }
        }

        {// -- meshes
            GFXMeshContainer& mesh = gfx->_mesh;
            std::lock_guard<mutex_t> lock_guard( mesh.to_remove_lock );
            while( !array::empty( mesh.to_remove ) )
            {
                id_t id = array::back( mesh.to_remove );
                array::pop_back( mesh.to_remove );
                if( mesh.flags[id.index] & GFXMeshContainer::FROM_FILE )
                {
                    SYS_NOT_IMPLEMENTED;
                }
                mesh.rsource[id.index] = nullptr;
                mesh.flags[id.index] = 0;


                {
                    std::lock_guard<mutex_t> lck( mesh.lock );
                    id_table::destroy( mesh.idtable, id );
                }
            }
        }

        {// --- materials
            GFXMaterialContainer& mc = gfx->_material;
            BXIAllocator* allocator = gfx->_allocator;

            std::lock_guard<mutex_t> lock_guard( mc.to_remove_lock );
            while ( !array::empty( mc.to_remove ) )
            {
                id_t id = array::back( mc.to_remove );
                array::pop_back( mc.to_remove );

                string::free( &mc.name[id.index] );
                mc.idself[id.index] = makeInvalidHandle<id_t>();

                DestroyResourceBinding( &mc.binding[id.index], allocator );
                Destroy( &mc.data_gpu[id.index] );

                {
                    std::lock_guard<mutex_t> lck( mc.lock );
                    id_table::destroy( mc.idtable, id );
                }
            }
        }
    }

    bool IsMaterialAlive( GFXSystem* gfx, id_t id )
    {
        return id_table::has( gfx->_material.idtable, id );
    }
    id_t FindMaterial( GFXSystem* gfx, const char* name )
    {
        const GFXMaterialContainer& materials = gfx->_material;
        for( uint32_t i = 0; i < MAX_MATERIALS; ++i )
        {
            if( string::equal( materials.name[i].c_str(), name ) )
            {
                if( id_table::has( materials.idtable, materials.idself[i] ) )
                    return materials.idself[i];
            }
        }
        return makeInvalidHandle<id_t>();
    }

    RDIXResourceBinding* GetMaterialBinding( GFXSystem* gfx, id_t id )
    {
        if( !IsMaterialAlive( gfx, id ) )
            return nullptr;

        return gfx->_material.binding[id.index];
    }
}

GFX* GFX::Allocate( BXIAllocator* allocator )
{
    uint32_t mem_size = 0;
    mem_size += sizeof( GFX );
    mem_size += sizeof( GFXSystem );
    mem_size += sizeof( GFXUtils ) + sizeof( GFXUtilsData );

    void* memory = BX_MALLOC( allocator, mem_size, 16 );
    memset( memory, 0x00, mem_size );

    GFX* gfx_interface = (GFX*)memory;
    gfx_interface->gfx = new(gfx_interface + 1) GFXSystem();

    gfx_interface->utils = (GFXUtils*)(gfx_interface->gfx + 1);
    gfx_interface->utils->data = new(gfx_interface->utils + 1) GFXUtilsData();

    return gfx_interface;
}

void GFX::Free( GFX** gfxi, BXIAllocator* allocator )
{
    gfxi[0]->gfx->~GFXSystem();
    gfxi[0]->utils->data->~GFXUtilsData();
    BX_DELETE0( allocator, gfxi[0] );
}

void GFX::StartUp( RDIDevice* dev, const GFXDesc& desc, BXIFilesystem* filesystem, BXIAllocator* allocator )
{
    gfx->_allocator = allocator;
    gfx->_rdidev = dev;

    utils->StartUp( dev, filesystem, allocator );

    {// --- framebuffer
        RDIXRenderTargetDesc render_target_desc( desc.framebuffer_width, desc.framebuffer_height );
        render_target_desc.Texture( RDIFormat::Float4() );
        render_target_desc.Depth( RDIEType::DEPTH32F );
        gfx->_framebuffer = CreateRenderTarget( dev, render_target_desc, allocator );
    }

    {// --- material
        GFXMaterialContainer& materials = gfx->_material;

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
    
    { // fallback material
        GFXMaterialDesc material_desc;
        material_desc.data.diffuse_albedo = float3( 1.0, 0.0, 0.0 );
        material_desc.data.roughness = 0.9f;
        material_desc.data.specular_albedo = float3( 1.0, 0.0, 0.0 );
        material_desc.data.metal = 0.f;
        gfx->_fallback_idmaterial = CreateMaterial( "fallback", material_desc );
    }

    { // fallback mesh
        par_shapes_mesh* shape = par_shapes_create_parametric_sphere( 64, 64, FLT_EPSILON );
        
        GFXMeshDesc mesh_desc = {};
        mesh_desc.rsouce = CreateRenderSourceFromShape( dev, shape, allocator );
        gfx->_fallback_idmesh = CreateMesh( mesh_desc );

        par_shapes_free_mesh( shape );
    }
}


void GFX::ShutDown()
{
    DestroyMesh( gfx->_fallback_idmesh );
    DestroyMaterial( gfx->_fallback_idmaterial );
    gfx_internal::RemovePendingObjects( gfx );

    RDIDevice* dev = gfx->_rdidev;
    BXIAllocator* allocator = gfx->_allocator;

    SYS_ASSERT( gfx->_frame_ctx.Valid() == false );

    {// --- samplers
        ::Destroy( &gfx->_samplers._trilinear );
        ::Destroy( &gfx->_samplers._bilinear );
        ::Destroy( &gfx->_samplers._linear );
        ::Destroy( &gfx->_samplers._point );
    }

    {// --- material

        ::Destroy( &gfx->_material.frame_data_gpu );
        DestroyPipeline( dev, &gfx->_material.base, allocator );
    }

    DestroyRenderTarget( dev, &gfx->_framebuffer,  allocator );
   
    utils->ShutDown( dev );
}

RDIXRenderTarget* GFX::Framebuffer()
{
    return gfx->_framebuffer;
}

RDIXPipeline* GFX::MaterialBase()
{
    return gfx->_material.base;
}

GFXMeshID GFX::CreateMesh( const GFXMeshDesc& desc )
{
    gfx->_mesh.lock.lock();
    id_t id = id_table::create( gfx->_mesh.idtable );
    gfx->_mesh.lock.unlock();

    SYS_ASSERT( desc.filename || desc.rsouce );

    if( desc.filename )
    {
        SYS_NOT_IMPLEMENTED;
    }
    else
    {
        gfx->_mesh.rsource[id.index] = desc.rsouce;
    }

    return { id.hash };
}

void GFX::DestroyMesh( GFXMeshID idmesh )
{
    id_t id = { idmesh.i };
    if( !id_table::has( gfx->_mesh.idtable, id ) )
        return;

    gfx->_mesh.lock.lock();
    id = id_table::invalidate( gfx->_mesh.idtable, id );
    gfx->_mesh.lock.unlock();

    gfx->_mesh.to_remove_lock.lock();
    array::push_back( gfx->_mesh.to_remove, id );
    gfx->_mesh.to_remove_lock.unlock();
}

GFXMaterialID GFX::CreateMaterial( const char* name, const GFXMaterialDesc& desc )
{
    id_t id = gfx_internal::FindMaterial( gfx, name );
    if( gfx_internal::IsMaterialAlive( gfx, id ) )
        return {0};

    RDIDevice* dev = gfx->_rdidev;
    BXIAllocator* allocator = gfx->_allocator;

    GFXMaterialContainer& mc = gfx->_material;

    mc.lock.lock();
    id = id_table::create( mc.idtable );
    mc.lock.unlock();

    mc.data_gpu[id.index] = CreateConstantBuffer( dev, sizeof( gfx_shader::Material ) );

    RDIConstantBuffer* cbuffer = &mc.data_gpu[id.index];
    RDIXResourceBinding* binding = CloneResourceBinding( ResourceBinding( mc.base ), allocator );

    UpdateCBuffer( GetImmediateCommandQueue( dev ), *cbuffer, &desc.data );
    SetConstantBuffer( binding, "MaterialData", cbuffer );

    mc.binding[id.index] = binding;
    mc.data[id.index] = desc.data;
    string::create( &mc.name[id.index], name, allocator );
    mc.idself[id.index] = id;

    return { id.hash };
}

void GFX::DestroyMaterial( GFXMaterialID idmat )
{
    GFXMaterialContainer& mc = gfx->_material;

    id_t id = { idmat.i };
    if( !id_table::has( mc.idtable, id ) )
        return;

    mc.lock.lock();
    id = id_table::invalidate( mc.idtable, id );
    mc.lock.unlock();

    mc.to_remove_lock.lock();
    array::push_back( mc.to_remove, id );
    mc.to_remove_lock.unlock();
}

GFXMaterialID GFX::FindMaterial( const char* name )
{
    return { gfx_internal::FindMaterial( gfx, name ).hash };
}

bool GFX::IsMaterialAlive( GFXMaterialID idmat )
{
    return gfx->_material.IsAlive( { idmat.i } );
}

RDIXResourceBinding* GFX::MaterialBinding( GFXMaterialID idmat )
{
    const id_t id = { idmat.i };
    return (gfx->_material.IsAlive( id )) ? gfx->_material.binding[id.index] : nullptr;
}

GFXSceneID GFX::CreateScene( const GFXSceneDesc& desc )
{
    GFXSceneContainer& sc = gfx->_scene;

    gfx->_scene.lock.lock();
    id_t idscene = id_table::create( sc.idtable );
    gfx->_scene.lock.unlock();

    RDIXTransformBufferDesc transform_buffer_desc = {};
    transform_buffer_desc.capacity = desc.max_renderables;
    RDIXTransformBuffer* transform_buffer = CreateTransformBuffer( gfx->_rdidev, transform_buffer_desc, gfx->_allocator );
    RDIXCommandBuffer* command_buffer = CreateCommandBuffer( gfx->_allocator, desc.max_renderables * 4 + 10 );
    
    container_soa_desc_t cnt_desc;
    container_soa_add_stream( cnt_desc, GFXSceneContainer::MeshData, world_matrix );
    container_soa_add_stream( cnt_desc, GFXSceneContainer::MeshData, transform_buffer_instance_index );
    container_soa_add_stream( cnt_desc, GFXSceneContainer::MeshData, idmesh );
    container_soa_add_stream( cnt_desc, GFXSceneContainer::MeshData, idmat );

    GFXSceneContainer::MeshData* mesh_data = nullptr;
    container_soa::create( &mesh_data, cnt_desc, desc.max_renderables, gfx->_allocator );

    GFXSceneContainer::MeshIDAllocator* mesh_idalloc = nullptr;
    mesh_idalloc = id_allocator::create_dense( desc.max_renderables, gfx->_allocator );

    const uint32_t scene_index = idscene.index;
    
    sc.command_buffer[scene_index] = command_buffer;
    sc.transform_buffer[scene_index] = transform_buffer;
    sc.mesh_data[scene_index] = mesh_data;
    sc.mesh_idalloc[scene_index] = mesh_idalloc;

    return { idscene.hash };
}

void GFX::DestroyScene( GFXSceneID idscene )
{
    GFXSceneContainer& sc = gfx->_scene;
    
    id_t id = { idscene.i };
    if( !sc.IsSceneAlive( id ) )
        return;

    sc.lock.lock();
    id = id_table::invalidate( sc.idtable, id );
    sc.lock.unlock();

    sc.to_remove_lock.lock();
    array::push_back( sc.to_remove, id );
    sc.to_remove_lock.unlock();
}

GFXMeshInstanceID GFX::AddMeshToScene( GFXSceneID idscene, const GFXMeshInstanceDesc& desc, const mat44_t& pose )
{
    const id_t id = { idscene.i };
    GFXSceneContainer& sc = gfx->_scene;
    if( !sc.IsSceneAlive( id ) )
        return { 0 };

    const uint32_t index = id.index;

    sc.mesh_lock[index].lock();
    id_t idinst = id_allocator::alloc( sc.mesh_idalloc[index] );
    sc.mesh_lock[index].unlock();

    GFXMeshID idmesh = desc.idmesh;
    if( !gfx->_mesh.IsAlive( { idmesh.i } ) )
        idmesh = gfx->_fallback_idmesh;

    GFXMaterialID idmat = desc.idmaterial;
    if( !gfx->_material.IsAlive( { idmat.i } ) )
        idmat = gfx->_fallback_idmaterial;

    GFXSceneContainer::MeshData* data = sc.mesh_data[index];
    data->world_matrix[idinst.index] = pose;
    data->idmesh[idinst.index] = idmesh;
    data->idmat[idinst.index] = idmat;

    return { idinst.hash };
}

GFXFrameContext* GFX::BeginFrame( RDICommandQueue* cmdq )
{
    gfx_internal::RemovePendingObjects( gfx );

    ClearState( cmdq );
    SetSamplers( cmdq, (RDISampler*)&gfx->_samplers._point, 0, 4, RDIEPipeline::ALL_STAGES_MASK );

    SYS_ASSERT( !gfx->_frame_ctx.Valid() );
    gfx->_frame_ctx.cmdq = cmdq;
    return &gfx->_frame_ctx;
}

void GFX::EndFrame( GFXFrameContext* fctx )
{
    SYS_ASSERT( fctx == &gfx->_frame_ctx );

    ::Swap( fctx->cmdq, gfx->_sync_interval );
    fctx->cmdq = nullptr;
}
void GFX::RasterizeFramebuffer( RDICommandQueue* cmdq, uint32_t texture_index, float aspect )
{
    utils->CopyTexture( cmdq, nullptr, Texture( gfx->_framebuffer, 0 ), aspect );
}

void GFX::SetCamera( const GFXCameraParams& camerap, const GFXCameraMatrices& cameram )
{
    {
        gfx_shader::MaterialFrameData fdata;
        fdata.camera_world = cameram.world;
        fdata.camera_view = cameram.view;
        fdata.camera_view_proj = cameram.view_proj;
        fdata.camera_eye = vec4_t( cameram.eye(), 1.f );
        fdata.camera_dir = vec4_t( cameram.dir(), 0.f );
        UpdateCBuffer( GetImmediateCommandQueue( gfx->_rdidev ), gfx->_material.frame_data_gpu, &fdata );
    };
}

void GFX::BindMaterialFrame( GFXFrameContext* fctx )
{
    SetCbuffers( fctx->cmdq, &gfx->_material.frame_data_gpu, MATERIAL_FRAME_DATA_SLOT, 1, RDIEPipeline::ALL_STAGES_MASK );
}

void GFX::DrawScene( GFXFrameContext* fctx, GFXSceneID idscene, const GFXCameraParams& camerap, const GFXCameraMatrices& cameram )
{

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

//// --- plugin interface
//extern "C" {
//
//    PLUGIN_EXPORT void* BXLoad_gfx( BXIAllocator* allocator )
//    {
//        return GFXAllocate( allocator );
//    }
//
//    PLUGIN_EXPORT void BXUnload_gfx( void* plugin, BXIAllocator* allocator )
//    {
//        GFX* gfx = (GFX*)plugin;
//        gfx->ShutDown();
//        GFXFree( &gfx, allocator );
//    }
//}
