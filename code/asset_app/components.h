#pragma once

#include <gfx/gfx_type.h>
#include <resource_manager/resource_manager.h>
#include <foundation/math/vmath_type.h>

struct RDIXRenderSource;
struct TOOLMeshComponent
{
    GFXMeshInstanceID id_mesh;

    void Initialize( GFX* gfx, GFXSceneID idscene, const GFXMeshInstanceDesc& mesh_desc, const mat44_t& pose );
    void Uninitialize( GFX* gfx );
};

struct TOOLAnimComponent
{
    RSMResourceID id_skel_resource;
    RSMResourceID id_clip_resource;
};
