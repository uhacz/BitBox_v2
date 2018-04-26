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
// shader
//
struct GFX_EXPORT GFXShaderResourceLoader : RSMLoader
{
    RTTI_DECLARE_TYPE( GFXShaderResourceLoader );

    virtual const char* SupportedType() const { return "shader"; }
    virtual bool IsBinary() const { return true; }
};
