#include "gfx_resource_loader.h"
#include <memory/memory.h>

#include <rdi_backend/rdi_backend.h>
#include "gfx.h"
#include <rdix/rdix.h>

void GFXMeshResourceLoader::Unload( RSMResourceData* in_out )
{
    RDIXRenderSource* rsource = (RDIXRenderSource*)in_out->pointer;
    DestroyRenderSource( &rsource );
}

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


