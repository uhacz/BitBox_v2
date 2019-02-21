#pragma once

#include <gfx/gfx_type.h>
#include <resource_manager/resource_manager.h>
#include <foundation/hashed_string.h>
#include <foundation/math/vmath_type.h>
#include <entity/entity_system.h>

#include <functional>


struct RDIXRenderSource;
struct TOOLMeshComponent
{
    GFXMeshInstanceID id_mesh;

    bool Initialize( GFX* gfx, GFXSceneID idscene, const GFXMeshInstanceDesc& mesh_desc, const mat44_t& pose );
    void Uninitialize( GFX* gfx );

    static void SetWorldPose( ECSRawComponent* _this, GFX* gfx, const mat44_t& pose );
};


struct SKINBoneMapping
{
    uint16_t skin_idx;
    uint16_t anim_idx;
};

struct RDIXMeshFile;
struct ANIMSkel;

struct TOOLTransformComponent
{
    mat44_t pose;

    bool Initialize( const vec3_t& pos );
    bool Initialize( const quat_t& rot );
    bool Initialize( const mat44_t& mtx );
    bool Initialize( const xform_t& xform );
};

struct TOOLTransformMeshComponent
{
    ECSComponentID source;
    ECSComponentID destination;

    bool Initialize( ECS* ecs, ECSComponentID src, ECSComponentID dst );
};

struct TOOLTransformSystem
{
    ECSComponentID Link( ECS* ecs, ECSComponentID src, ECSComponentID dst );
    void Tick( ECS* ecs, GFX* gfx );

    void* _dummy = nullptr;
};


struct TOOLMeshDescComponent
{
    ECS_NON_POD_COMPONENT( TOOLMeshDescComponent );

    GFXMeshInstanceID        id_mesh;
    array_t<hashed_string_t> bones_names;
    array_t<mat44_t>         bones_offsets;

    bool Initialize( ECS* ecs, ECSComponentID id_mesh_comp, const RDIXMeshFile* mesh_file );
};

struct TOOLAnimDescComponent
{
    ECS_NON_POD_COMPONENT( TOOLAnimDescComponent );

    array_t<hashed_string_t> bones_names;

    bool Initialize( const ANIMSkel* skel );
};

struct TOOLSkinningComponent
{
    ECS_NON_POD_COMPONENT( TOOLSkinningComponent );

    GFXMeshInstanceID        id_mesh;
    array_t<mat44_t>         bone_offsets;
    array_t<SKINBoneMapping> mapping;
    uint32_t                 num_valid_mappings = 0;

    bool Initialize( const RDIXMeshFile* mesh_file, const ANIMSkel* skel, GFXMeshInstanceID mesh );
    bool Initialize( ECS* ecs, ECSComponentID id_mesh_desc );
    bool Initialize( const TOOLMeshDescComponent* mesh_desc, const TOOLAnimDescComponent* anim_desc );

    void ComputeSkinningMatrices( array_span_t<mat44_t> output_span, const array_span_t<const mat44_t> anim_matrices );
};

struct CMNEngine;
struct TOOLContext;

void           UnloadTOOLMeshComponent( CMNEngine* e, ECSComponentID id_mesh_comp );
ECSComponentID LoadTOOLMeshComponent( CMNEngine* e, ECSComponentID id_mesh_comp, const TOOLContext& ctx, const RDIXMeshFile* mesh_file, BXIAllocator* allocator );
