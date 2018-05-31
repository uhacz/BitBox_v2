#pragma once

#include "dll_interface.h"
#include <resource_manager/resource_loader.h>
#include <rtti/rtti.h>

//
// mesh
//
struct GFX_EXPORT GFXMeshResourceLoader : RSMLoader
{
    RTTI_DECLARE_TYPE( GFXMeshResourceLoader );

    virtual const char* SupportedType() const { return "mesh"; }
    virtual bool IsBinary() const { return true; }
};

//
// texture
//
struct GFX_EXPORT GFXTextureResourceLoader: RSMLoader
{
    RTTI_DECLARE_TYPE( GFXTextureResourceLoader );

    virtual const char* SupportedType() const override { return "DDS"; }
    virtual bool IsBinary() const override { return true; }

    virtual bool Load( RSMResourceData* out, const void* data, uint32_t size, BXIAllocator* allocator, void* system ) override;
    virtual void Unload( RSMResourceData* in_out ) override;
};


//
// shader
//
struct GFX_EXPORT GFXShaderResourceLoader : RSMLoader
{
    RTTI_DECLARE_TYPE( GFXShaderResourceLoader );

    virtual const char* SupportedType() const { return "shader"; }
    virtual bool IsBinary() const { return true; }
};
