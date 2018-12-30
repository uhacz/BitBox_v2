#include "components.h"
#include "gfx\gfx.h"

void TOOLMeshComponent::Initialize( GFX* gfx, GFXSceneID idscene, const GFXMeshInstanceDesc& mesh_desc, const mat44_t& pose )
{
    id_mesh = gfx->AddMeshToScene( idscene, mesh_desc, pose );
}

void TOOLMeshComponent::Uninitialize( GFX* gfx )
{
    gfx->RemoveMeshFromScene( id_mesh );
}
