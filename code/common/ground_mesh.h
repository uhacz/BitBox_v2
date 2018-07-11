#pragma once
#include <foundation\math\vmath_type.h>
#include <gfx\gfx_forward_decl.h>

struct GFX;
struct RSM;
struct CMNGroundMesh
{
    mat44_t _world_pose = mat44_t::identity();
    GFXMeshInstanceID _mesh_id = { 0 };
};

void CreateGroundMesh( CMNGroundMesh* mesh, GFX* gfx, GFXSceneID scene_id, RSM* rsm, const vec3_t& scale = vec3_t( 100.f, 0.5f, 100.f ), const mat44_t& pose = mat44_t::identity() );
void DestroyGroundMesh( CMNGroundMesh* mesh, GFX* gfx );