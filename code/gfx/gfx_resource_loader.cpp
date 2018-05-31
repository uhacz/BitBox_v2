#include "gfx_resource_loader.h"
#include <memory/memory.h>

RTTI_DEFINE_EMPTY_TYPE_DERIVED( GFXMeshResourceLoader, RSMLoader );
RTTI_DEFINE_EMPTY_TYPE_DERIVED( GFXTextureResourceLoader, RSMLoader );
RTTI_DEFINE_EMPTY_TYPE_DERIVED( GFXShaderResourceLoader, RSMLoader );


#include <rdi_backend/rdi_backend.h>
#include "gfx.h"
bool GFXTextureResourceLoader::Load( RSMResourceData* out, const void* data, uint32_t size, BXIAllocator* allocator, void* system )
{
    GFX* gfx = (GFX*)system;
    RDITextureRO* texture = BX_NEW( allocator, RDITextureRO );
    texture[0] = CreateTextureFromDDS( gfx->Device(), data, size );
    out->allocator = allocator;
    out->pointer = texture;
    out->size = sizeof( RDITextureRO );

    return true;
}

void GFXTextureResourceLoader::Unload( RSMResourceData* in_out )
{
    RDITextureRO* texture = (RDITextureRO*)in_out->pointer;
    if( texture )
    {
        Destroy( texture );
        BX_DELETE( in_out->allocator, texture );
    }
    in_out[0] = {};
}