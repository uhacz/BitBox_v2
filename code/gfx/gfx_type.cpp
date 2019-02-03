#include "gfx_type.h"


void GFXMeshInstanceDesc::AddRenderSource( const RDIXRenderSource* rsource )
{
    idmesh_resource = RSM::Create( rsource );
}

void GFXMeshInstanceDesc::ForceCPUskinning()
{
    flags.cpu_skinning = 1;
}
