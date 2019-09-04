#pragma once

#include "../node.h"
#include "../node_comp.h"
#include "../../gfx/gfx.h"

struct NODECompMesh : NODEComp
{
    NODE_COMP_DECLARE();

    virtual void OnInitialize  ( NODE* node, NODEInitContext* ctx, NODEFlags* flags ) override;
    virtual void OnUninitialize( NODE* node, NODEInitContext* ctx ) override;
    virtual bool OnAttach      ( NODE* node, NODEAttachContext* ctx ) override;
    virtual void OnDetach      ( NODE* node, NODEAttachContext* ctx ) override;
    virtual void OnSerialize   ( NODE* node, NODETextSerializer* serializer ) override;
    
    GFXMeshInstanceID _mesh_id;
    RSMResourceID _mesh_resource_id;
    
    string_t _mesh_resource_path;
    string_t _material_name;
};