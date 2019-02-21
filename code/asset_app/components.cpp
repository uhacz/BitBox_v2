#include "components.h"

#include <entity/entity_proxy.h>

#include "foundation\hashed_string.h"
#include <foundation\array.h>

#include "rdix\rdix_type.h"
#include "rdix\rdix.h"
#include "gfx\gfx.h"

#include "anim\anim_struct.h"


#include <algorithm>



#include "common\base_engine_init.h"
#include "tool_context.h"

bool TOOLMeshComponent::Initialize( GFX* gfx, GFXSceneID idscene, const GFXMeshInstanceDesc& mesh_desc, const mat44_t& pose )
{
    id_mesh = gfx->AddMeshToScene( idscene, mesh_desc, pose );
    return true;
}

void TOOLMeshComponent::Uninitialize( GFX* gfx )
{
    gfx->RemoveMeshFromScene( id_mesh );
}

void TOOLMeshComponent::SetWorldPose( ECSRawComponent* _this, GFX* gfx, const mat44_t& pose )
{
    TOOLMeshComponent* mesh_cmp = (TOOLMeshComponent*)_this;
    gfx->SetWorldPose( mesh_cmp->id_mesh, pose );
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



bool TOOLSkinningComponent::Initialize( const RDIXMeshFile* mesh_file, const ANIMSkel* skel, GFXMeshInstanceID mesh )
{
    const uint32_t num_mesh_bones = mesh_file->num_bones;
    if( !num_mesh_bones )
    {
        SYS_LOG_ERROR( "Trying to initialize skinning component with no bones in mesh" );
        return false;
    }
    const mat44_t* mesh_bone_offsets = TYPE_OFFSET_GET_POINTER( const mat44_t, mesh_file->offset_bones );
    const hashed_string_t* mesh_bones_names = TYPE_OFFSET_GET_POINTER( const hashed_string_t, mesh_file->offset_bones_names );
    const hashed_string_t* skel_bones_names = TYPE_OFFSET_GET_POINTER( const hashed_string_t, skel->offsetJointNames );

    InitializeSkinningBones( bone_offsets, to_array_span( mesh_bone_offsets, num_mesh_bones ) );
    num_valid_mappings = InitializeSkinningMapping( mapping, to_array_span( mesh_bones_names, num_mesh_bones ), to_array_span( skel_bones_names, skel->numJoints ) );
    
    id_mesh = mesh;
    
    return true;
}

bool TOOLSkinningComponent::Initialize( ECS* ecs, ECSComponentID id_mesh_desc )
{
    const ECSEntityID entity = ecs->Owner( this );
    ECSEntityProxy eproxy( ecs, entity );

    const ECSComponentProxy<TOOLMeshDescComponent> mesh_desc( ecs, id_mesh_desc );

    if( mesh_desc )
    {
        id_mesh = mesh_desc->id_mesh;
    }

    const ECSComponentProxy<TOOLAnimDescComponent> anim_desc( ecs, eproxy.Lookup<TOOLAnimDescComponent>() );

    if( !anim_desc || !mesh_desc )
        return false;

    Initialize( mesh_desc.Get(), anim_desc.Get() );

    return true;
}

bool TOOLSkinningComponent::Initialize( const TOOLMeshDescComponent* mesh_desc, const TOOLAnimDescComponent* anim_desc )
{
    id_mesh = mesh_desc->id_mesh;

    InitializeSkinningBones( bone_offsets, to_array_span( mesh_desc->bones_offsets ) );
    num_valid_mappings = InitializeSkinningMapping( mapping, to_array_span( mesh_desc->bones_names ), to_array_span( anim_desc->bones_names ) );

    return num_valid_mappings != 0;
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

bool TOOLMeshDescComponent::Initialize( ECS* ecs, ECSComponentID id_mesh_comp, const RDIXMeshFile* mesh_file )
{
    ECSComponentProxy<TOOLMeshComponent> mesh_comp( ecs, id_mesh_comp );
    if( !mesh_comp )
        return false;

    id_mesh = mesh_comp->id_mesh;

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



    return true;
}

bool TOOLTransformComponent::Initialize( const vec3_t& pos )
{
    pose = mat44_t::translation( pos );
    return true;
}

bool TOOLTransformComponent::Initialize( const quat_t& rot )
{
    pose = mat44_t( rot );
    return true;
}

bool TOOLTransformComponent::Initialize( const mat44_t& mtx )
{
    pose = mtx;
    return true;
}

bool TOOLTransformComponent::Initialize( const xform_t& xform )
{
    pose = mat44_t( xform.rot, xform.pos );
    return true;
}

bool TOOLTransformMeshComponent::Initialize( ECS* ecs, ECSComponentID src, ECSComponentID dst )
{
    ECSComponentProxy<TOOLTransformComponent> tr( ecs, src );
    ECSComponentProxy<TOOLMeshComponent> mesh( ecs, dst );
    if( tr && mesh )
    {
        source = src;
        destination = dst;
        return true;
    }
    return false;
}

ECSComponentID TOOLTransformSystem::Link( ECS* ecs, ECSComponentID src, ECSComponentID dst )
{
    ECSComponentProxy<TOOLTransformMeshComponent> proxy = ECSComponentProxy<TOOLTransformMeshComponent>::New( ecs );
    if( !proxy->Initialize( ecs, src, dst ) )
    {
        proxy.Release();
    }

    return proxy.Id();
}

void TOOLTransformSystem::Tick( ECS* ecs, GFX* gfx )
{
    array_span_t<TOOLTransformMeshComponent*> links = Components<TOOLTransformMeshComponent>( ecs );
    for( TOOLTransformMeshComponent* link : links )
    {
        ECSComponentProxy< TOOLTransformComponent> tr_proxy( ecs, link->source );
        ECSComponentProxy< TOOLMeshComponent> mesh_proxy( ecs, link->destination );

        if( !tr_proxy || !mesh_proxy )
            continue;

        gfx->SetWorldPose( mesh_proxy->id_mesh, tr_proxy->pose );
    }
}


//

void UnloadTOOLMeshComponent( CMNEngine* e, ECSComponentID id_mesh_comp )
{
    ECSComponentProxy<TOOLMeshComponent> proxy( e->ecs, id_mesh_comp );
    if( proxy )
    {
        ECSEntityProxy eproxy = proxy.OwnerProxy();
        ECSComponentIDSpan comp_id_span = eproxy.Components();

        ECSComponentIterator it( e->ecs, eproxy.Id() );

        while( ECSComponentProxy<TOOLMeshDescComponent> mesh_desc = it.FindNext<TOOLMeshDescComponent>() )
        {
            if( mesh_desc->id_mesh.i == proxy->id_mesh.i )
            {
                mesh_desc.Release();
                break;
            }
        }

        it.Reset();
        while( ECSComponentProxy<TOOLSkinningComponent> skin = it.FindNext<TOOLSkinningComponent>() )
        {
            if( skin->id_mesh.i == proxy->id_mesh.i )
            {
                skin.Release();
                break;
            }
        }

        proxy->Uninitialize( e->gfx );
        proxy.Release();
    }
}

ECSComponentID LoadTOOLMeshComponent( CMNEngine* e, ECSComponentID id_mesh_comp, const TOOLContext& ctx, const RDIXMeshFile* mesh_file, BXIAllocator* allocator )
{
    RDIXRenderSource* rsource = CreateRenderSourceFromMemory( e->rdidev, mesh_file, allocator );

    GFXMeshInstanceDesc desc = {};
    desc.AddRenderSource( rsource );

    ECSComponentProxy<TOOLMeshComponent> mesh_comp( e->ecs, id_mesh_comp );
    if( mesh_comp )
    {
        desc.idmaterial = e->gfx->Material( mesh_comp->id_mesh );
    }
    else
    {
        desc.idmaterial = e->gfx->FindMaterial( "editable" );
    }

    UnloadTOOLMeshComponent( e, id_mesh_comp );

    mesh_comp = ECSComponentProxy<TOOLMeshComponent>::New( e->ecs );
    mesh_comp->Initialize( e->gfx, ctx.gfx_scene, desc, mat44_t::identity() );
    mesh_comp.SetOwner( ctx.entity );

    ECSComponentProxy<TOOLMeshDescComponent> mesh_desc = ECSComponentProxy<TOOLMeshDescComponent>::New( e->ecs );
    mesh_desc->Initialize( e->ecs, mesh_comp.Id(), mesh_file );
    mesh_desc.SetOwner( ctx.entity );

    ECSComponentProxy<TOOLSkinningComponent> skin_comp = ECSComponentProxy<TOOLSkinningComponent>::New( e->ecs );
    skin_comp->Initialize( e->ecs, mesh_desc.Id() );
    skin_comp.SetOwner( ctx.entity );

    return mesh_comp.Id();
}

bool TOOLAnimDescComponent::Initialize( const ANIMSkel* skel )
{
    if( !skel )
        return false;

    const hashed_string_t* skel_bones_names = TYPE_OFFSET_GET_POINTER( hashed_string_t, skel->offsetJointNames );
    array::clear( bones_names );
    array::reserve( bones_names, skel->numJoints );
    for( uint32_t ijoint = 0; ijoint < skel->numJoints; ++ijoint )
        array::push_back( bones_names, skel_bones_names[ijoint] );

    return true;
}
