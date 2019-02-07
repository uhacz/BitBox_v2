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

static uint32_t InitializeSkinningMapping( 
    array_t<SKINBoneMapping>& mapping, 
    array_span_t<const hashed_string_t> mesh_bones_names,
    array_span_t<const hashed_string_t> anim_bones_names )
{
    const uint32_t num_mesh_bones = mesh_bones_names.size();
    const uint32_t num_anim_bones = anim_bones_names.size();

    array::clear( mapping );
    array::reserve( mapping, num_mesh_bones );

    for( uint32_t imesh_bone = 0; imesh_bone < num_mesh_bones; ++imesh_bone )
    {
        const hashed_string_t bone_name = mesh_bones_names[imesh_bone];
        uint32_t found = array::find( anim_bones_names.begin(), num_anim_bones, bone_name );

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

    uint32_t num_valid_mappings = 0;
    for( ; num_valid_mappings < mapping.size; ++num_valid_mappings )
    {
        if( mapping[num_valid_mappings].anim_idx == UINT16_MAX )
            break;
    }
    return num_valid_mappings;
}

static void InitializeSkinningBones( array_t<mat44_t>& bone_offsets, array_span_t<const mat44_t> input_offsets )
{
    const uint32_t num_mesh_bones = input_offsets.size();

    array::clear( bone_offsets );
    array::reserve( bone_offsets, num_mesh_bones );

    for( uint32_t ibone = 0; ibone < num_mesh_bones; ++ibone )
    {
        array::push_back( bone_offsets, input_offsets[ibone] );
    }
}



void TOOLSkinningComponent::Initialize( const RDIXMeshFile* mesh_file, const ANIMSkel* skel, GFXMeshInstanceID mesh )
{
    const uint32_t num_mesh_bones = mesh_file->num_bones;
    if( !num_mesh_bones )
    {
        SYS_LOG_ERROR( "Trying to initialize skinning component with no bones in mesh" );
        return;
    }
    const mat44_t* mesh_bone_offsets = TYPE_OFFSET_GET_POINTER( const mat44_t, mesh_file->offset_bones );
    const hashed_string_t* mesh_bones_names = TYPE_OFFSET_GET_POINTER( const hashed_string_t, mesh_file->offset_bones_names );
    const hashed_string_t* skel_bones_names = TYPE_OFFSET_GET_POINTER( const hashed_string_t, skel->offsetJointNames );

    InitializeSkinningBones( bone_offsets, to_array_span( mesh_bone_offsets, num_mesh_bones ) );
    num_valid_mappings = InitializeSkinningMapping( mapping, to_array_span( mesh_bones_names, num_mesh_bones ), to_array_span( skel_bones_names, skel->numJoints ) );
}

bool TOOLSkinningComponent::Initialize( ECS* ecs, ECSComponentID id_mesh_desc, GFXMeshInstanceID mesh )
{
    const ECSEntityID entity = ecs->Owner( this );

    ECSComponentID anim_desc_id = Lookup<TOOLAnimDescComponent>( ecs, entity );

    if( anim_desc_id == ECSComponentID::Null() || id_mesh_desc == ECSComponentID::Null() )
    {
        return false;    
    }

    const TOOLAnimDescComponent* anim_desc = Component<TOOLAnimDescComponent>( ecs, anim_desc_id );
    const TOOLMeshDescComponent* mesh_desc = Component<TOOLMeshDescComponent>( ecs, id_mesh_desc );

    Initialize( mesh_desc, anim_desc, mesh );

    return true;
}

void TOOLSkinningComponent::Initialize( const TOOLMeshDescComponent* mesh_desc, const TOOLAnimDescComponent* anim_desc, GFXMeshInstanceID mesh )
{
    if( mesh.i )
        id_mesh = mesh;

    InitializeSkinningBones( bone_offsets, to_array_span( mesh_desc->bones_offsets ) );
    num_valid_mappings = InitializeSkinningMapping( mapping, to_array_span( mesh_desc->bones_names ), to_array_span( anim_desc->bones_names ) );
}

void TOOLSkinningComponent::ComputeSkinningMatrices( array_span_t<mat44_t> output_span, const array_span_t<const mat44_t> anim_matrices )
{
    //array_span_t<mat44_t> output_span = to_array_span( skinning_matrices );

    for( uint32_t i = 0; i < num_valid_mappings; ++i )
    {
        const SKINBoneMapping& m = mapping[i];
        const mat44_t& offset = bone_offsets[m.skin_idx];

        const mat44_t& anim_matrix = anim_matrices[m.anim_idx];
        output_span[m.skin_idx] = anim_matrix * offset;
    }

    for( uint32_t i = num_valid_mappings; i < array::size( mapping ); ++i )
    {
        const SKINBoneMapping& m = mapping[i];
        output_span[m.skin_idx] = mat44_t::identity();
    }
 }

void TOOLMeshDescComponent::Initialize( const RDIXMeshFile* mesh_file )
{
    const uint32_t num_bones = mesh_file->num_bones;
    array::clear( bones_names );
    array::clear( bones_offsets );
    array::reserve( bones_names, num_bones );
    array::reserve( bones_offsets, num_bones );

    const hashed_string_t* src_bones_names = TYPE_OFFSET_GET_POINTER( hashed_string_t, mesh_file->offset_bones_names );
    const mat44_t* src_bones_offsets = TYPE_OFFSET_GET_POINTER( mat44_t, mesh_file->offset_bones );

    for( uint32_t i = 0; i < num_bones; ++i )
    {
        array::push_back( bones_names, src_bones_names[i] );
        array::push_back( bones_offsets, src_bones_offsets[i] );
    }
}

void InitializeSkinningComponent( ECSComponentID skinning_id, ECS* ecs )
{
    if( TOOLSkinningComponent* comp = Component<TOOLSkinningComponent>( ecs, skinning_id ) )
    {
        
    }
}