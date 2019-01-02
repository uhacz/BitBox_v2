#pragma once

#include <gfx/gfx_type.h>
#include <resource_manager/resource_manager.h>
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

struct TOOLSkinningComponent
{
    ECS_NON_POD_COMPONENT( TOOLSkinningComponent );

    GFXMeshInstanceID id_mesh;
    array_t<mat44_t> bone_offsets;
    array_t<SKINBoneMapping> mapping;

    void Initialize( const RDIXMeshFile* mesh_file, const ANIMSkel* skel, GFXMeshInstanceID mesh );
    void Uninitialize();
};

struct TOOLAnimComponent
{
    RSMResourceID id_skel_resource;
    RSMResourceID id_clip_resource;
};
