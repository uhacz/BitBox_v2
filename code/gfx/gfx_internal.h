#pragma once

#include <foundation/thread/mutex.h>
#include <foundation/string_util.h>
#include <foundation/id_table.h>
#include <foundation/id_array.h>
#include <foundation/array.h>
#include <foundation/static_array.h>
#include <foundation/container_soa.h>
#include <foundation/id_allocator_dense.h>
#include <util/bbox.h>

#include <rdi_backend/rdi_backend.h>
#include <rdix/rdix.h>
#include <rdix/rdix_command_buffer.h>

#include "gfx_type.h"
#include "gfx_camera.h"
#include "gfx_shadow.h"
#include "foundation/type_compound.h"
#include <atomic>



static constexpr uint32_t GFX_MAX_SCENES = 4;
static constexpr uint32_t GFX_MAX_MESH_INSTANCES = 1024 * 64;
static constexpr uint32_t GFX_MAX_MATERIALS = SHADER_MAX_MATERIALS;
static constexpr uint32_t GFX_MAX_CAMERAS = SHADER_MAX_CAMERAS;
static constexpr uint32_t GFX_SKINNING_BUFFER_SIZE = BIT_MEGA_BYTE( 4 );
static constexpr uint32_t GFX_DEFAULT_SKINNING_PIN = UINT32_MAX;

namespace GFXEFramebuffer
{
    enum E : uint8_t
    {
        COLOR = 0,
        COLOR_SWAP,
        SHADOW,
        DEPTH,

        _COUNT_,
    };
}//

namespace GFXEMaterialFlag
{
    enum E : uint8_t
    {
        PIPELINE_BASE = BIT_OFFSET( 0 ),
        PIPELINE_FULL = BIT_OFFSET( 1 ),
    };
}//


using IDArray = array_t<id_t>;
using ResourceIDArray = array_t<RSMResourceID>;

struct GFXCameraContainer
{
    id_array_t<GFX_MAX_CAMERAS> id_alloc;
    mutex_t id_lock;

    mutex_t to_remove_lock;
    IDArray to_remove;

    GFXCameraParams params[GFX_MAX_CAMERAS] = {};
    GFXCameraMatrices matrices[GFX_MAX_CAMERAS] = {};
    string_t names[GFX_MAX_CAMERAS] = {};

    BXIAllocator* _names_allocator = nullptr;

    bool IsAlive( id_t id ) const { return id_array::has( id_alloc, id );  }
    uint32_t DataIndex( id_t id ) const { return id_array::index( id_alloc, id ); }
    uint32_t Size()
    {
        scope_mutex_t guard( id_lock );
        return id_array::size( id_alloc );
    }
};


struct GFXMaterialContainer
{
    struct
    {
        RDIXPipeline* base = nullptr;
        RDIXPipeline* base_with_skybox = nullptr;
        RDIXPipeline* full = nullptr;
        RDIXPipeline* skybox = nullptr;
        RDIXPipeline* shadow_depth = nullptr;
    } pipeline;


    RDIConstantBuffer lighting_data_gpu = {};

    mutex_t                   lock;
    id_table_t<GFX_MAX_MATERIALS> idtable;
    gfx_shader::Material      data    [GFX_MAX_MATERIALS] = {};
    GFXMaterialTexture        textures[GFX_MAX_MATERIALS] = {};
    RDIXResourceBinding*      binding [GFX_MAX_MATERIALS] = {};
    string_t                  name    [GFX_MAX_MATERIALS] = {};
    uint8_t                   flags   [GFX_MAX_MATERIALS] = {};
    id_t                      idself  [GFX_MAX_MATERIALS] = {};

    mutex_t to_remove_lock;
    IDArray to_remove;

    mutex_t to_refresh_lock;
    IDArray to_refresh;

    //mutex_t resource_to_release_lock;
    //ResourceIDArray resource_to_release;

    bool IsAlive( id_t id ) const { return id_table::has( idtable, id ); }
    uint32_t DataIndex( id_t id ) const { return id.index; }
};

struct GFXMeshSkinningData
{
    RDIXRenderSource* rsource = nullptr;
    uint32_t pin_index = GFX_DEFAULT_SKINNING_PIN;
    uint32_t num_elements = 0;
};

implement this
struct GFXSkinningData
{
    RDIBufferRO gpu_buffer[2];
    uint32_t mapped_gpu_buffer = 0;

    float4_t* cpu_buffer = nullptr;
    uint32_t cpu_buffer_capacity = 0;
    std::atomic<uint32_t> cpu_buffer_offset;

    void Initialize( RDIDevice* dev, uint32_t capacity );
    void Uninitialize();

    void Swap();

    struct Pin
    {
        float4_t* address;
        uint32_t  index; // in float4_t unit
        uint32_t  num_elements;
    };
    Pin Allocate( uint32_t size_in_bytes );
};
struct GFXSceneContainer
{
    struct MeshData
    {
        mat44_t* world_matrix;
        gfx_shader::InstanceData* instance_data;
        RSMResourceID* idmesh_resource;
        GFXMeshSkinningData* skinning_data;
        GFXMaterialID* idmat;
        id_t* idinstance;
    };
       
    struct DeadMeshInstanceID
    {
        id_t idscene;
        id_t idinst;
    };
    struct SkyData
    {
        RDITextureRO sky_texture = {};
        GFXSkyParams params = {};
        uint32_t flag_enabled = 0;
    };

    using MeshIDAllocator = id_allocator_dense_t;

    mutex_t lock;
    id_table_t<GFX_MAX_SCENES> idtable;

    mutex_t to_remove_lock;
    IDArray to_remove;

    mutex_t                     mesh_to_remove_lock;
    array_t<DeadMeshInstanceID> mesh_to_remove;

    GFXMeshInstanceID mesh_to_skin[GFX_MAX_MESH_INSTANCES];
    std::atomic<uint32_t> num_meshes_to_skin{ 0 };
    

    RDIXTransformBuffer* transform_buffer[GFX_MAX_SCENES] = {};
    RDIXCommandBuffer*   command_buffer[GFX_MAX_SCENES] = {};
    GFXSkinningData      skinning_data[GFX_MAX_SCENES] = {};

    MeshData*        mesh_data[GFX_MAX_SCENES] = {};
    MeshIDAllocator* mesh_idalloc[GFX_MAX_SCENES] = {};
    mutex_t          mesh_lock[GFX_MAX_SCENES] = {};

    SkyData          sky_data[GFX_MAX_SCENES] = {};
    GFXShadowData    sun_shadow[GFX_MAX_SCENES] = {};

    bool IsSceneAlive( id_t id ) const { return id_table::has( idtable, id ); }
    bool IsMeshAlive( id_t idscene, id_t id ) const
    {
        const bool scene_ok = IsSceneAlive( idscene );
        const bool mesh_ok = id_allocator::has( mesh_idalloc[idscene.index], id );
        return (scene_ok && mesh_ok);
    }

    uint32_t SceneIndexSafe( id_t id )
    {
        return IsSceneAlive( id ) ? id.index : UINT32_MAX;
    }
    uint32_t MeshInstanceIndex( id_t idscene, id_t id )
    {
        SYS_ASSERT( IsMeshAlive( idscene, id ) );
        return id_allocator::dense_index( mesh_idalloc[idscene.index], id );
    }
};

struct GFXPostProcess
{
    struct
    {
        RDIXPipeline* pipeline_ss = nullptr;
        RDIXPipeline* pipeline_combine = nullptr;
        RDIConstantBuffer cbuffer_fdata;
    }shadow;
};

struct GFXFrameContext
{
    RDICommandQueue* cmdq = nullptr;
    bool Valid() const { return cmdq != nullptr; }
};


using IDArray = array_t<id_t>;
struct GFXSystem
{
    BXIAllocator* _allocator = nullptr;
    RDIDevice*	  _rdidev = nullptr;

    RDIXRenderTarget* _framebuffer = nullptr;
    uint32_t _sync_interval = 0;

    gfx_shader::ShaderSamplers _samplers;
    RDIConstantBuffer _gpu_camera_buffer;
    RDIConstantBuffer _gpu_frame_data_buffer;
    RDIConstantBuffer _gpu_material_data_buffer;

    static constexpr uint32_t NUM_DEFAULT_MESHES = 2;
    RSMResourceID     _default_meshes[NUM_DEFAULT_MESHES];
    RDIXRenderSource* _fallback_mesh = nullptr;

    static constexpr uint32_t NUM_DEFAULT_MATERIALS = 5;
    GFXMaterialID _default_materials[NUM_DEFAULT_MATERIALS];
    GFXMaterialID _fallback_idmaterial;

    GFXFrameContext _frame_ctx = {};

    GFXCameraContainer   _camera;
    GFXMaterialContainer _material;
    GFXSceneContainer    _scene;
    GFXPostProcess       _postprocess;

    uint32_t MaterialDataIndex( id_t idmat ) const
    {
        const id_t fallback_id = { _fallback_idmaterial.i };
        return _material.IsAlive( idmat ) ? _material.DataIndex( idmat ) : _material.DataIndex( fallback_id );
    }
};
