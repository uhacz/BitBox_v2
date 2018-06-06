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

#include <mutex>




static constexpr uint32_t GFX_MAX_SCENES = 4;
static constexpr uint32_t GFX_MAX_MESH_INSTANCES = 1024 * 64;
static constexpr uint32_t GFX_MAX_MATERIALS = 64;

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

struct GFXMaterialContainer
{
    struct
    {
        RDIXPipeline* base = nullptr;
        RDIXPipeline* base_with_skybox = nullptr;
        RDIXPipeline* full = nullptr;
        RDIXPipeline* skybox = nullptr;
    } pipeline;


    RDIConstantBuffer frame_data_gpu = {};
    RDIConstantBuffer lighting_data_gpu = {};

    mutex_t                   lock;
    id_table_t<GFX_MAX_MATERIALS> idtable;
    gfx_shader::Material      data[GFX_MAX_MATERIALS] = {};
    GFXMaterialTexture        textures[GFX_MAX_MATERIALS] = {};
    RDIConstantBuffer         data_gpu[GFX_MAX_MATERIALS] = {};
    RDIXResourceBinding*      binding[GFX_MAX_MATERIALS] = {};
    string_t                  name[GFX_MAX_MATERIALS] = {};
    uint8_t                   flags[GFX_MAX_MATERIALS] = {};
    id_t                      idself[GFX_MAX_MATERIALS] = {};

    mutex_t to_remove_lock;
    IDArray to_remove;

    mutex_t to_refresh_lock;
    IDArray to_refresh;

    bool IsAlive( id_t id ) const { return id_table::has( idtable, id ); }
};



struct GFXSceneContainer
{
    struct MeshData
    {
        mat44_t* world_matrix;
        uint32_t* transform_buffer_instance_index;
        RSMResourceID* idmesh_resource;
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

    RDIXTransformBuffer* transform_buffer[GFX_MAX_SCENES] = {};
    RDIXCommandBuffer*   command_buffer[GFX_MAX_SCENES] = {};

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
    RSM* rsm = nullptr;

    bool Valid() const { return cmdq != nullptr; }
};


using IDArray = array_t<id_t>;
struct GFXSystem
{
    BXIAllocator* _allocator = nullptr;
    RDIDevice*	  _rdidev = nullptr;

    RDIXRenderTarget* _framebuffer = nullptr;
    uint32_t _sync_interval = 0;

    GFXMaterialContainer _material;
    GFXSceneContainer    _scene;
    GFXSceneID           _scene_lookup[GFX_MAX_SCENES][GFX_MAX_MESH_INSTANCES] = {};
    GFXPostProcess       _postprocess;

    static constexpr uint32_t NUM_DEFAULT_MESHES = 2;
    RSMResourceID     _default_meshes[NUM_DEFAULT_MESHES];
    RDIXRenderSource* _fallback_mesh = nullptr;


    static constexpr uint32_t NUM_DEFAULT_MATERIALS = 5;
    GFXMaterialID _default_materials[NUM_DEFAULT_MATERIALS];
    GFXMaterialID _fallback_idmaterial;

    gfx_shader::ShaderSamplers _samplers;

    GFXFrameContext _frame_ctx = {};
};
