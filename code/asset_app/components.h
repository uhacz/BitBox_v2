#pragma once

#include <gfx/gfx_type.h>
#include <resource_manager/resource_manager.h>
#include <foundation/hashed_string.h>
#include <foundation/math/vmath_type.h>
#include <entity/entity_system.h>


struct RDIXRenderSource;
struct TOOLMeshComponent
{
    GFXMeshInstanceID id_mesh;

    void Initialize( GFX* gfx, GFXSceneID idscene, const GFXMeshInstanceDesc& mesh_desc, const mat44_t& pose );
    void Uninitialize( GFX* gfx );
};


struct SKINBoneMapping
{
    uint16_t skin_idx;
    uint16_t anim_idx;
};

struct RDIXMeshFile;
struct ANIMSkel;

struct TOOLMeshDescComponent
{
    ECS_NON_POD_COMPONENT( TOOLMeshDescComponent );

    array_t<hashed_string_t> bones_names;
    array_t<mat44_t>         bones_offsets;
};

struct TOOLAnimDescComponent
{
    ECS_NON_POD_COMPONENT( TOOLAnimDescComponent );

    array_t<hashed_string_t> bones_names;
};

struct TOOLSkinningComponent
{
    ECS_NON_POD_COMPONENT( TOOLSkinningComponent );

    GFXMeshInstanceID id_mesh;
    array_t<mat44_t> bone_offsets;
    array_t<mat44_t> skinning_matrices;
    array_t<SKINBoneMapping> mapping;
    uint32_t num_valid_mappings = 0;

    void Initialize( const RDIXMeshFile* mesh_file, const ANIMSkel* skel, GFXMeshInstanceID mesh );
    void Uninitialize();

    void ComputeSkinningMatrices( const array_span_t<const mat44_t> anim_matrices );
};

bool InitializeSkinningComponent( ECSComponentID skinning_id, ECS* ecs );
void InitializeSkinningComponent( TOOLSkinningComponent* output, const TOOLMeshDescComponent* mesh_desc, const TOOLAnimDescComponent* anim_desc, GFXMeshInstanceID mesh );


