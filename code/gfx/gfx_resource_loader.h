#pragma once

#include <resource_manager/resource_loader.h>
#include <rtti/rtti.h>

//
// mesh
//
struct GFXMeshResourceLoader : RSMLoader
{
    RSM_DEFINE_LOADER( GFXMeshResourceLoader );
    virtual const char* SupportedType() const { return "mesh"; }
    virtual bool IsBinary() const { return true; }
    
    virtual void Unload( RSMResourceData* in_out ) override;
};

//
// texture
//
struct GFXTextureResourceLoader: RSMLoader
{
    RSM_DEFINE_LOADER( GFXTextureResourceLoader );

    virtual const char* SupportedType() const override { return "DDS"; }
    virtual bool IsBinary() const override { return true; }

    virtual bool Load( RSMResourceData* out, const void* data, uint32_t size, BXIAllocator* allocator, void* system ) override;
    virtual void Unload( RSMResourceData* in_out ) override;
};


//
// shader
//
struct GFXShaderResourceLoader : RSMLoader
{
    RSM_DEFINE_LOADER( GFXShaderResourceLoader );

    virtual const char* SupportedType() const { return "shader"; }
    virtual bool IsBinary() const { return true; }
};
