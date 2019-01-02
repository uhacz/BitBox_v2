#include "components.h"
#include "gfx\gfx.h"
#include "rdix\rdix_type.h"
#include "foundation\hashed_string.h"
#include "anim\anim_struct.h"
#include <foundation\array.h>
#include <algorithm>

void TOOLMeshComponent::Initialize( GFX* gfx, GFXSceneID idscene, const GFXMeshInstanceDesc& mesh_desc, const mat44_t& pose )
{
    id_mesh = gfx->AddMeshToScene( idscene, mesh_desc, pose );
}

void TOOLMeshComponent::Uninitialize( GFX* gfx )
{
    gfx->RemoveMeshFromScene( id_mesh );
}

void TOOLSkinningComponent::Initialize( const RDIXMeshFile* mesh_file, const ANIMSkel* skel, GFXMeshInstanceID mesh )
{
    const uint32_t num_mesh_bones = mesh_file->num_bones;

    const mat44_t* mesh_bone_offsets = TYPE_OFFSET_GET_POINTER( const mat44_t, mesh_file->offset_bones );
    const hashed_string_t* mesh_bones_names = TYPE_OFFSET_GET_POINTER( const hashed_string_t, mesh_file->offset_bones_names );
    const hashed_string_t* skel_bones_names = TYPE_OFFSET_GET_POINTER( const hashed_string_t, skel->offsetJointNames );

    array::reserve( bone_offsets, num_mesh_bones );
    array::reserve( mapping, num_mesh_bones );
    
    for( uint32_t ibone = 0; ibone < num_mesh_bones; ++ibone )
        array::push_back( bone_offsets, mesh_bone_offsets[ibone] );

    for( uint32_t imesh_bone = 0; imesh_bone < num_mesh_bones; ++imesh_bone )
    {
        const hashed_string_t bone_name = mesh_bones_names[imesh_bone];
        uint32_t found = array::find( skel_bones_names, skel->numJoints, bone_name );
        
        SKINBoneMapping m;
        m.skin_idx = (uint16_t)imesh_bone;
        m.anim_idx = (uint16_t)found;
        array::push_back( mapping, m );
    }

    std::sort( mapping.begin(), mapping.end(), []( const SKINBoneMapping& a, const SKINBoneMapping& b )
    {
        const uint32_t hash_a = (uint32_t)a.anim_idx << 16 | (uint32_t)a.skin_idx;
        const uint32_t hash_b = (uint32_t)b.anim_idx << 16 | (uint32_t)b.skin_idx;
        return hash_a < hash_b;
    } );
}

void TOOLSkinningComponent::Uninitialize()
{

}
