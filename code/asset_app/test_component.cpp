#include "test_component.h"
#include <util/random/random.h>
#include <util/grid.h>
#include <foundation/time.h>


RTTI_DEFINE_TYPE_DERIVED( TestComponent, ENTIComponent, {
    RTTI_ATTR( TestComponent, _mesh_pose, "MeshPose" )->SetDefault( mat44_t::identity() ),
    RTTI_ATTR( TestComponent, _shape_type, "ShapeType" )->SetDefault( 0u ),
    RTTI_ATTR( TestComponent, _material_name, "MaterialName" )->SetDefault( "red" ),
} );

TestComponent::~TestComponent()
{

}

void TestComponent::Initialize( ENTEntityID entity_id, ENTSystemInfo* sys )
{
    static uint32_t instance_index = 0;

    const char* shapes[] = { "box", "sphere", "box", "sphere", "box", "sphere", "box", "sphere", "box", "sphere", "box", "sphere" };
    const char* materials[] = { "red", "green", "blue" };
    const uint32_t nb_shapes = (uint32_t)sizeof_array( shapes );
    const uint32_t nb_materials = (uint32_t)sizeof_array( materials );

    uint32_t i = instance_index++;
    random_t rnd = RandomInit( (uint64_t)this + i * 192, 0xda3e39cb94b95bdbULL );

    for( uint32_t dummy = 0; dummy <= i; ++dummy )
        Random( &rnd );

    const uint32_t shape_index = Random( &rnd ) % nb_shapes;
    const uint32_t material_index = Random( &rnd, nb_materials );

    const uint32_t grid_w = 32;
    const uint32_t grid_h = 16;
    const uint32_t grid_d = 32;

    grid_t grid( grid_w, grid_h, grid_d );
    i = i % grid.NumCells();

    uint32_t coords[3];
    grid.Coords( coords, i );

    _mesh_pose.set_translation( vec3_t( coords[0], coords[1], coords[2] ) * 2.f );

    const char* mat_name = materials[material_index]; // (strlen( _material_name.c_str() )) ? _material_name.c_str() : "red";
    GFXMeshInstanceDesc meshi_desc = {};
    meshi_desc.idmesh_resource = sys->rsm->Find( shapes[shape_index] );
    meshi_desc.idmaterial.i = 0; // sys->gfx->FindMaterial( mat_name );
    GFXMeshInstanceID mesh_instance_id = sys->gfx->AddMeshToScene( sys->gfx_scene_id, meshi_desc, _mesh_pose );
    sys->ent->AttachComponent( entity_id, ENTComponentExtID( ENTCComponent::GFX_MESH, mesh_instance_id.i ) );
}

void TestComponent::Deinitialize( ENTEntityID entity_id, ENTSystemInfo* sys )
{
    const array_span_t<ENTComponentExtID> system_components = sys->ent->GetSystemComponents( entity_id );
    for( uint32_t i = 0; i < system_components.size(); ++i )
    {
        if( system_components[i].type == ENTCComponent::GFX_MESH )
        {
            sys->gfx->RemoveMeshFromScene( { system_components[i].id } );
        }
    }
}

void TestComponent::ParallelStep( ENTEntityID entity_id, ENTSystemInfo* sys, uint64_t dt_us )
{
    const float dt_s = (float)BXTime::Micro_2_Sec( dt_us );

    const array_span_t<ENTComponentExtID> system_components = sys->ent->GetSystemComponents( entity_id );
    for( uint32_t i = 0; i < system_components.size(); ++i )
    {
        if( system_components[i].type == ENTCComponent::GFX_MESH )
        {
            _mesh_pose = _mesh_pose * mat44_t::rotationx( dt_s );
            _mesh_pose = _mesh_pose * mat44_t::rotationy( dt_s * 0.5f );
            _mesh_pose = _mesh_pose * mat44_t::rotationz( dt_s * 0.25f );
            sys->gfx->SetWorldPose( { system_components[i].id }, _mesh_pose );
        }
    }
}

void TestComponent::SerialStep( ENTEntityID entity_id, ENTIComponent* parent, ENTSystemInfo*, uint64_t dt_us )
{

}
