#pragma once

#include <foundation/type.h>

namespace GFXERenderMask
{
    enum E : uint8_t
    {
        COLOR = BIT_OFFSET( 0 ),
        SHADOW = BIT_OFFSET( 1 ),

        COLOR_SHADOW = COLOR | SHADOW,
    };
}//

namespace GFXEMaterialTextureSlot
{
    enum E : uint8_t
    {
        BASE_COLOR = 0,
        NORMAL,
        ROUGHNESS,
        METALNESS,
        _COUNT_,
    };
}//

struct BXIFilesystem;
struct BXIAllocator;

struct RDIDevice;
struct RDICommandQueue;
struct RDITextureRW;

struct RDIXPipeline;
struct RDIXResourceBinding;
struct RDIXRenderTarget;
struct RDIXRenderSource;
struct RDIXCommandBuffer;

struct GFXCameraParams;
struct GFXCameraMatrices;
struct GFXFrameContext;
struct GFXSystem;

struct GFXSceneID        { uint32_t i; };
struct GFXCameraID       { uint32_t i; };
struct GFXMeshID         { uint32_t i; };
struct GFXMaterialID     { uint32_t i; };
struct GFXMeshInstanceID { uint64_t i; };
