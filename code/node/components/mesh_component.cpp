#include "mesh_component.h"
#include <typeinfo>

NODE_COMP_DEFINE( NODECompMesh );

void NODECompMesh::OnInitialize( NODE* node, NODEInitContext* ctx, NODEFlags* flags )
{
    if( _mesh_resource_path.length() )
    {
        _mesh_resource_id = RSM::Load( _mesh_resource_path.c_str() );
    }

    flags->offline.tick = 0;
}

void NODECompMesh::OnUninitialize( NODE* node, NODEInitContext* ctx )
{
    RSM::Release( _mesh_resource_id );
}

bool NODECompMesh::OnAttach( NODE* node, NODEAttachContext* ctx )
{
    return true;
}

void NODECompMesh::OnDetach( NODE* node, NODEAttachContext* ctx )
{

}

void NODECompMesh::OnSerialize( NODE* node, NODETextSerializer* serializer )
{

}
