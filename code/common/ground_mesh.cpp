#include "ground_mesh.h"

#include <gfx\gfx.h>
#include <resource_manager\resource_manager.h>

void CreateGroundMesh( CMNGroundMesh* mesh, GFX* gfx, GFXSceneID scene_id, RSM* rsm, const vec3_t& scale /*= vec3_t( 100.f, 0.5f, 100.f )*/, const mat44_t& pose /*= mat44_t::identity() */ )
{
    GFXMeshInstanceDesc desc = {};
    desc.idmaterial = gfx->FindMaterial( "red" );
    desc.idmesh_resource = rsm->Find( "box" );
    const mat44_t final_pose = append_scale( pose, scale );

    mesh->_mesh_id = gfx->AddMeshToScene( scene_id, desc, final_pose );
    mesh->_world_pose = final_pose;
}

void DestroyGroundMesh( CMNGroundMesh* mesh, GFX* gfx )
{
    gfx->RemoveMeshFromScene( mesh->_mesh_id );
}
