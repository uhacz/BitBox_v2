#pragma once

#include <foundation/type.h>
//#include "util/viewport.h"

namespace bx{ namespace rdi {

namespace EStage
{
    enum Enum
    {
        VERTEX = 0,
        PIXEL,
        COMPUTE,
        COUNT,
        DRAW_STAGES_COUNT = PIXEL + 1,

        ALL_STAGES_MASK = 0xFF,

        VERTEX_MASK  = BIT_OFFSET( VERTEX ),
        PIXEL_MASK   = BIT_OFFSET( PIXEL ),
        COMPUTE_MASK = BIT_OFFSET( COMPUTE ),
    };

    static const char* name[COUNT] =
    {
        "vertex",
        "pixel",
        "compute",
    };
};

namespace EBindMask
{
    enum Enum
    {
        NONE = 0,
        VERTEX_BUFFER    = BIT_OFFSET( 0 ),
        INDEX_BUFFER     = BIT_OFFSET( 1 ),
        CONSTANT_BUFFER  = BIT_OFFSET( 2 ),
        SHADER_RESOURCE  = BIT_OFFSET( 3 ),
        STREAM_OUTPUT    = BIT_OFFSET( 4 ),
        RENDER_TARGET    = BIT_OFFSET( 5 ),
        DEPTH_STENCIL    = BIT_OFFSET( 6 ),
        UNORDERED_ACCESS = BIT_OFFSET( 7 ),
    };
};

namespace EDataType
{
    enum Enum
    {
        UNKNOWN = 0,
        BYTE,
        UBYTE,
        SHORT,
        USHORT,
        INT,
        UINT,
        FLOAT,
        DOUBLE,
        DEPTH16,
        DEPTH24_STENCIL8,
        DEPTH32F,

        COUNT,
    };
    static const unsigned stride[] =
    {
        0, //
        1, //BYTE,
        1, //UBYTE,
        2, //SHORT,
        2, //USHORT,
        4, //INT,
        4, //UINT,
        4, //FLOAT,
        8, //DOUBLE,
        2, //DEPTH16,
        4, //DEPTH24_STENCIL8,
        4, //DEPTH32F,
    };
    static const char* name[] =
    {
        "none",
        "byte",
        "ubyte",
        "short",
        "ushort",
        "int",
        "uint",
        "float",
        "double",
        "depth16",
        "depth24_stencil8",
        "depth32F",
    };
    Enum FromName( const char* name );
    Enum FindBaseType( const char* name );

};


struct Format
{
    uint8_t type;
    uint8_t numElements : 6;
    uint8_t normalized : 1;
    uint8_t srgb : 1;

    Format()
        : type(EDataType::UNKNOWN)
        , numElements(0), normalized(0), srgb(0)
    {}

    Format( EDataType::Enum dt, uint8_t ne ) { type = dt; numElements = ne; normalized = 0; srgb = 0; }
    Format& Normalized( uint32_t onOff ) { normalized = onOff; return *this; }
    Format& Srgb( uint32_t onOff ) { srgb = onOff; return *this; }
    inline uint32_t ByteWidth() const { return EDataType::stride[type] * numElements; }
};


namespace ECpuAccess
{
    enum Enum
    {
        READ = BIT_OFFSET( 0 ),
        WRITE = BIT_OFFSET( 1 ),
    };
};
namespace EGpuAccess
{
    enum Enum
    {
        READ = BIT_OFFSET( 1 ),
        WRITE = BIT_OFFSET( 2 ),
        READ_WRITE = READ | WRITE,
    };
};

namespace EMapType
{
    enum Enum
    {
        WRITE = 0,
        NO_DISCARD,
    };
};

namespace ETopology
{
    enum Enum
    {
        POINTS = 0,
        LINES,
        LINE_STRIP,
        TRIANGLES,
        TRIANGLE_STRIP,
    };
};

namespace EVertexSlot
{
    enum Enum
    {
        POSITION = 0,
        BLENDWEIGHT,
        NORMAL,
        COLOR0,
        COLOR1,
        FOGCOORD,
        PSIZE,
        BLENDINDICES,
        TEXCOORD0,
        TEXCOORD1,
        TEXCOORD2,
        TEXCOORD3,
        TEXCOORD4,
        TEXCOORD5,
        TANGENT,
        BINORMAL,
        COUNT,
    };

    static const char* name[EVertexSlot::COUNT] =
    {
        "POSITION",
        "BLENDWEIGHT",
        "NORMAL",
        "COLOR",
        "COLOR",
        "FOGCOORD",
        "PSIZE",
        "BLENDINDICES",
        "TEXCOORD",
        "TEXCOORD",
        "TEXCOORD",
        "TEXCOORD",
        "TEXCOORD",
        "TEXCOORD",
        "TANGENT",
        "BINORMAL",
    };
    static const int semanticIndex[EVertexSlot::COUNT] =
    {
        0, //POSITION = 0
        0, //BLENDWEIGHT,
        0, //NORMAL,	
        0, //COLOR0,	
        1, //COLOR1,	
        0, //FOGCOORD,	
        0, //PSIZE,		
        0, //BLENDINDICES
        0, //TEXCOORD0,	
        1, //TEXCOORD1,
        2, //TEXCOORD2,
        3, //TEXCOORD3,
        4, //TEXCOORD4,
        5, //TEXCOORD5,
        0, //TANGENT,
    };
    EVertexSlot::Enum FromString( const char* n );

};


//////////////////////////////////////////////////////////////////////
/// hwState
namespace EDepthFunc
{
    enum Enum
    {
        NEVER = 0,
        LESS,
        EQUAL,
        LEQUAL,
        GREATER,
        NOTEQUAL,
        GEQUAL,
        ALWAYS,
    };
};


namespace EBlendFactor
{
    enum Enum
    {
        ZERO = 0,
        ONE,
        SRC_COLOR,
        ONE_MINUS_SRC_COLOR,
        DST_COLOR,
        ONE_MINUS_DST_COLOR,
        SRC_ALPHA,
        ONE_MINUS_SRC_ALPHA,
        DST_ALPHA,
        ONE_MINUS_DST_ALPHA,
        SRC_ALPHA_SATURATE,
    };
};

namespace EBlendEquation
{
    enum Enum
    {
        ADD = 0,
        SUB,
        REVERSE_SUB,
        MIN,
        MAX,
    };
};

namespace ECullMode
{
    enum Enum
    {
        NONE = 0,
        BACK,
        FRONT,
    };
};

namespace EFillMode
{
    enum Enum
    {
        SOLID = 0,
        WIREFRAME,
    };
};

namespace EColorMask
{
    enum Enum
    {
        NONE = 0,
        RED = BIT_OFFSET( 0 ),
        GREEN = BIT_OFFSET( 1 ),
        BLUE = BIT_OFFSET( 2 ),
        ALPHA = BIT_OFFSET( 3 ),
        ALL = RED | GREEN | BLUE | ALPHA,
    };
};

namespace ESamplerFilter
{
    enum Enum
    {
        NEAREST = 0,
        LINEAR,
        BILINEAR,
        TRILINEAR,
        BILINEAR_ANISO,
        TRILINEAR_ANISO,
    };

    static const char* name[] =
    {
        "NEAREST",
        "LINEAR",
        "BILINEAR",
        "TRILINEAR",
        "BILINEAR_ANISO",
        "TRILINEAR_ANISO",
    };
    inline int hasMipmaps( Enum filter )	{ return ( filter >= BILINEAR ); }
    inline int hasAniso  ( Enum filter )    { return ( filter >= BILINEAR_ANISO ); }
};


namespace ESamplerDepthCmp
{
    enum Enum
    {
        NONE = 0,
        GREATER,
        GEQUAL,
        LESS,
        LEQUAL,
        ALWAYS,
        NEVER,
        COUNT
    };
};

namespace EAddressMode
{
    enum Enum
    {
        WRAP,
        CLAMP,
        BORDER,
    };

    static const char* name[] =
    {
        "WRAP",
        "CLAMP",
        "BORDER",
    };
};


static const uint32_t cMAX_RENDER_TARGETS = 8;
static const uint32_t cMAX_CBUFFERS = 6;
static const uint32_t cMAX_TEXTURES = 8;
static const uint32_t cMAX_RESOURCES_RO = 8;
static const uint32_t cMAX_RESOURCES_RW = 6;
static const uint32_t cMAX_SAMPLERS = 8;
static const uint32_t cMAX_VERTEX_BUFFERS = 6;
static const uint32_t cMAX_SHADER_MACRO = 32;

struct ShaderReflection;

//////////////////////////////////////////////////////////////////////////
union SamplerDesc
{
    SamplerDesc()
        : filter( ESamplerFilter::TRILINEAR_ANISO )
        , addressU( EAddressMode::WRAP )
        , addressV( EAddressMode::WRAP )
        , addressT( EAddressMode::WRAP )
        , depthCmpMode( ESamplerDepthCmp::NONE )
        , aniso( 16 )
    {}

    SamplerDesc& Filter  ( ESamplerFilter::Enum f )     { filter = f; return *this; }
    SamplerDesc& Address ( EAddressMode::Enum u )       { addressU = addressV = addressT = u; return *this;  }
    SamplerDesc& Address ( EAddressMode::Enum u, 
                           EAddressMode::Enum v, 
                           EAddressMode::Enum t )       { addressU = u;  addressV = v;  addressT = t; return *this; }
    SamplerDesc& DepthCmp( ESamplerDepthCmp::Enum dc )  { depthCmpMode = dc; return *this; }
    SamplerDesc& Aniso   ( uint8_t a )                       { aniso = a; return *this; }

    uint64_t key;
    struct
    {
        uint8_t filter;
        uint8_t addressU;
        uint8_t addressV;
        uint8_t addressT;
        uint8_t depthCmpMode;
        uint8_t aniso;
        uint16_t _padding;
    };
};

struct HardwareStateDesc
{
    union Blend
    {
        Blend& Enable     ()                                                 { enable = 1; return *this; }
        Blend& Disable    ()                                                 { enable = 0; return *this; }
        Blend& ColorMask  ( EColorMask::Enum m )                             { color_mask = m; return *this; }
        Blend& Factor     ( EBlendFactor::Enum src, EBlendFactor::Enum dst ) { srcFactor = src; dstFactor = dst; return *this; }
        Blend& FactorAlpha( EBlendFactor::Enum src, EBlendFactor::Enum dst ) { srcFactorAlpha = src; dstFactorAlpha = dst; return *this; }
        Blend& Equation   ( EBlendEquation::Enum eq )                        { equation = eq; return *this; }

        uint32_t key;
        struct
        {
            uint64_t enable : 1;
            uint64_t color_mask : 4;
            uint64_t equation : 4;
            uint64_t srcFactor : 4;
            uint64_t dstFactor : 4;
            uint64_t srcFactorAlpha : 4;
            uint64_t dstFactorAlpha : 4;
        };
    };

    union Depth
    {
        Depth& Function( EDepthFunc::Enum f ) { function = f; return *this; }
        Depth& Test    ( uint8_t onOff )           { test = onOff; return *this; }
        Depth& Write   ( uint8_t onOff )           { write = onOff; return *this; }

        uint32_t key;
        struct
        {
            uint16_t function;
            uint8_t  test;
            uint8_t	write;
        };
    };

    union Raster
    {
        Raster& CullMode        ( ECullMode::Enum c )  { cullMode = c; return *this; }
        Raster& FillMode        ( EFillMode::Enum f ) { fillMode = f; return *this; }
        Raster& Multisample     ( uint32_t onOff )         { multisample = onOff; return *this; }
        Raster& AntialiassedLine( uint32_t onOff )         { antialiasedLine = onOff; return *this; }
        Raster& Scissor         ( uint32_t onOff )         { scissor = onOff; return *this; }
            
        uint32_t key;
        struct
        {
            uint32_t cullMode : 2;
            uint32_t fillMode : 2;
            uint32_t multisample : 1;
            uint32_t antialiasedLine : 1;
            uint32_t scissor : 1;
        };
    };

    HardwareStateDesc()
    {
        blend
            .Disable()
            .ColorMask( EColorMask::ALL )
            .FactorAlpha( EBlendFactor::ONE, EBlendFactor::ZERO )
            .Factor( EBlendFactor::ONE, EBlendFactor::ZERO )
            .Equation( EBlendEquation::ADD );
            
        depth.Function( EDepthFunc::LEQUAL ).Test( 1 ).Write( 1 );

        raster.CullMode( ECullMode::BACK ).FillMode( EFillMode::SOLID ).Multisample( 1 ).AntialiassedLine( 1 ).Scissor( 0 );
    }
    Blend blend;
    Depth depth;
    Raster raster;
};

struct Viewport
{
	int16_t x, y;
	uint16_t w, h;
};

//////////////////////////////////////////////////////////////////////////
union VertexBufferDesc
{
    VertexBufferDesc() {}
    VertexBufferDesc( EVertexSlot::Enum s ) { slot = s; }
    VertexBufferDesc& DataType( EDataType::Enum dt, uint32_t ne ) 
    { 
        dataType = dt; 
        numElements = ne;
        return *this; 
    }
    VertexBufferDesc& Normalized() { typeNorm = 1; return *this; }

    inline uint32_t ByteWidth() const { return EDataType::stride[dataType] * numElements; }

    uint16_t hash = 0;
    struct
    {
        uint16_t slot : 7;
        uint16_t dataType : 4;
        uint16_t typeNorm : 1;
        uint16_t numElements : 4;
    };
};

struct VertexLayout
{
    VertexBufferDesc descs[cMAX_VERTEX_BUFFERS] = {};
    uint32_t count = 0;

    uint32_t InputMask() const
    {
        uint32_t mask = 0;
        for( uint32_t i = 0; i < count; ++i )
            mask |= 1 << descs[i].slot;
        return mask;
    }
};

}}///


//////////////////////////////////////////////////////////////////////////
/// dx11 structs
struct ID3D11Device;
struct ID3D11DeviceContext;

struct ID3D11Resource;
struct ID3D11DeviceChild;
struct ID3D11Buffer;
struct ID3D11VertexShader;
struct ID3D11PixelShader;
struct ID3D11ComputeShader;
struct ID3D11InputLayout;
struct ID3D11BlendState;
struct ID3D11DepthStencilState;
struct ID3D11RasterizerState;
struct ID3D11SamplerState;

struct ID3D11Texture1D;
struct ID3D11Texture2D;
struct ID3D11Texture3D;
struct ID3D11ShaderResourceView;
struct ID3D11RenderTargetView;
struct ID3D11DepthStencilView;
struct ID3D11UnorderedAccessView;
//////////////////////////////////////////////////////////////////////////


namespace bx { namespace rdi {

struct Resource
{
    union
    {
        uintptr_t id = 0;
        ID3D11Resource*  resource;
        ID3D11Texture1D* texture1D;
        ID3D11Texture2D* texture2D;
        ID3D11Texture3D* texture3D;
        ID3D11Buffer*    buffer;
    };
};

struct ResourceRO : Resource
{
    ID3D11ShaderResourceView* viewSH = nullptr;
};
struct ResourceRW : ResourceRO
{
    ID3D11UnorderedAccessView* viewUA = nullptr;
};



struct VertexBuffer : Resource
{
    VertexBufferDesc desc = {};
    uint32_t numElements = 0;
};
struct IndexBuffer : Resource
{
    uint32_t dataType = 0;
    uint32_t numElements = 0;
};

struct ConstantBuffer : Resource
{
    uint32_t size_in_bytes = 0;
};

struct BufferDesc
{
    uint32_t sizeInBytes = 0;
    uint32_t bind_flags = 0;
    Format format = {};
};

struct BufferRO : ResourceRO, BufferDesc
{
};
struct BufferRW : ResourceRW, BufferDesc
{
};

struct TextureInfo
{
    uint16_t width = 0;
    uint16_t height = 0;
    uint16_t depth = 0;
    uint8_t mips = 0;
    Format format = {};
    
};

struct TextureRO : ResourceRO
{
    TextureInfo info;
};

struct TextureRW : ResourceRW
{
    ID3D11RenderTargetView* viewRT = nullptr;
    TextureInfo info;
};

struct TextureDepth : ResourceRW
{
    ID3D11DepthStencilView* viewDS = nullptr;
    TextureInfo info;
};

struct Shader
{
    union
    {
        uintptr_t id = 0;
        union
        {
            ID3D11DeviceChild*      object;
            ID3D11VertexShader*     vertex;
            ID3D11PixelShader*      pixel;
            ID3D11ComputeShader*    compute;
        };
    };

    void* inputSignature = nullptr;
    int16_t stage = -1;
    uint16_t vertexInputMask = 0;
};

struct ShaderPass
{
    ID3D11VertexShader* vertex = nullptr;
    ID3D11PixelShader* pixel = nullptr;
    void* input_signature = nullptr;
    uint32_t vertex_input_mask = 0;
};
struct ShaderPassCreateInfo
{
    void* vertex_bytecode = nullptr;
    void* pixel_bytecode = nullptr;
    size_t vertex_bytecode_size = 0;
    size_t pixel_bytecode_size = 0;
    ShaderReflection* reflection = nullptr;
};

union InputLayout
{
    uintptr_t id = 0;
    ID3D11InputLayout* layout;
};

union BlendState
{
    uintptr_t id = 0;
    ID3D11BlendState* state;
};
union DepthState
{
    uintptr_t id = 0;
    ID3D11DepthStencilState* state;
};
union RasterState
{
    uintptr_t id = 0;
    ID3D11RasterizerState* state;
};

struct HardwareState
{
    ID3D11BlendState* blend = nullptr;
    ID3D11DepthStencilState* depth = nullptr;
    ID3D11RasterizerState* raster = nullptr;
};

union Sampler
{
    uintptr_t id = 0;
    ID3D11SamplerState* state;
};

struct Rect
{
    int16_t left, top, right, bottom;

    Rect() {}
    Rect( int l, int t, int r, int b )
        : left( l ), top( t ), right( r ), bottom( b )
    {}
};
//
struct CommandQueue;
struct Device;
}}///


struct BXIAllocator;
namespace bx{ namespace rdi{

void Startup( Device** dev, CommandQueue** cmdq, uintptr_t hWnd, int winWidth, int winHeight, int fullScreen, BXIAllocator* allocator );
void Shutdown( Device** dev, CommandQueue** cmdq, BXIAllocator* allocator );

namespace device
{
    VertexBuffer   CreateVertexBuffer  ( Device* dev, const VertexBufferDesc& desc, uint32_t numElements, const void* data = 0 );
    IndexBuffer    CreateIndexBuffer   ( Device* dev, EDataType::Enum dataType, uint32_t numElements, const void* data = 0 );
    ConstantBuffer CreateConstantBuffer( Device* dev, uint32_t sizeInBytes, const void* data = nullptr );
    BufferRO       CreateBufferRO      ( Device* dev, int numElements, Format format, unsigned cpuAccessFlag, unsigned gpuAccessFlag );

	ShaderPass CreateShaderPass( Device* dev, const ShaderPassCreateInfo& info );

    TextureRO    CreateTextureFromDDS( Device* dev, const void* dataBlob, size_t dataBlobSize );
    TextureRO    CreateTextureFromHDR( Device* dev, const void* dataBlob, size_t dataBlobSize );
    TextureRW    CreateTexture1D     ( Device* dev, int w, int mips, Format format, unsigned bindFlags, unsigned cpuaFlags, const void* data );
    TextureRW    CreateTexture2D     ( Device* dev, int w, int h, int mips, Format format, unsigned bindFlags, unsigned cpuaFlags, const void* data );
    TextureDepth CreateTexture2Ddepth( Device* dev, int w, int h, int mips, EDataType::Enum dataType );
    Sampler      CreateSampler       ( Device* dev, const SamplerDesc& desc );

    InputLayout CreateInputLayout( Device* dev, const VertexBufferDesc* blocks, int nblocks, Shader vertex_shader );
    InputLayout CreateInputLayout( Device* dev, const VertexLayout vertexLayout, ShaderPass shaderPass );
    HardwareState CreateHardwareState( Device* dev, HardwareStateDesc desc );

    void DestroyVertexBuffer  ( VertexBuffer* id );
    void DestroyIndexBuffer   ( IndexBuffer* id );
    void DestroyInputLayout   ( InputLayout * id );
    void DestroyConstantBuffer( ConstantBuffer* id );
    void DestroyBufferRO      ( BufferRO* id );
    void DestroyShaderPass    ( ShaderPass* id );
    void DestroyTexture       ( TextureRO* id );
    void DestroyTexture       ( TextureRW* id );
    void DestroyTexture       ( TextureDepth* id );
    void DestroySampler       ( Sampler* id );
    void DestroyBlendState    ( BlendState* id );
    void DestroyDepthState    ( DepthState* id );
    void DestroyRasterState   ( RasterState * id );
    void DestroyHardwareState ( HardwareState* id );

    // ---
    void GetAPIDevice( Device* dev, ID3D11Device** apiDev, ID3D11DeviceContext** apiCtx );
}///
//

namespace context
{
    void SetViewport      ( CommandQueue* cmdq, Viewport vp );
    void SetVertexBuffers ( CommandQueue* cmdq, VertexBuffer* vbuffers, unsigned start, unsigned n );
    void SetIndexBuffer   ( CommandQueue* cmdq, IndexBuffer ibuffer );
    void SetShaderPrograms( CommandQueue* cmdq, Shader* shaders, int n );
    void SetShader        ( CommandQueue* cmdq, Shader shader, EStage::Enum stage );
    void SetShaderPass    ( CommandQueue* cmdq, ShaderPass pass );
    void SetInputLayout   ( CommandQueue* cmdq, InputLayout ilay );

    void SetCbuffers   ( CommandQueue* cmdq, ConstantBuffer* cbuffers, unsigned startSlot, unsigned n, unsigned stageMask );
    void SetResourcesRO( CommandQueue* cmdq, ResourceRO* resources, unsigned startSlot, unsigned n, unsigned stageMask );
    void SetResourcesRW( CommandQueue* cmdq, ResourceRW* resources, unsigned startSlot, unsigned n, unsigned stageMask );
    void SetSamplers   ( CommandQueue* cmdq, Sampler* samplers, unsigned startSlot, unsigned n, unsigned stageMask );

    void SetDepthState  ( CommandQueue* cmdq, DepthState state );
    void SetBlendState  ( CommandQueue* cmdq, BlendState state );
    void SetRasterState ( CommandQueue* cmdq, RasterState state );
    void SetHardwareState( CommandQueue* cmdq, HardwareState hwstate );
    void SetScissorRects( CommandQueue* cmdq, const Rect* rects, int n );
    void SetTopology    ( CommandQueue* cmdq, int topology );

    void ChangeToMainFramebuffer( CommandQueue* cmdq );
    void ChangeRenderTargets( CommandQueue* cmdq, TextureRW* colorTex, unsigned nColor, TextureDepth depthTex = TextureDepth(), bool changeViewport = true );

    unsigned char* Map( CommandQueue* cmdq, Resource resource, int offsetInBytes, int mapType = EMapType::WRITE );
    void           Unmap( CommandQueue* cmdq, Resource resource );

    unsigned char* Map( CommandQueue* cmdq, VertexBuffer vbuffer, int firstElement, int numElements, int mapType );
    unsigned char* Map( CommandQueue* cmdq, IndexBuffer ibuffer, int firstElement, int numElements, int mapType );

    void UpdateCBuffer( CommandQueue* cmdq, ConstantBuffer cbuffer, const void* data );
    void UpdateTexture( CommandQueue* cmdq, TextureRW texture, const void* data );

    void Draw                ( CommandQueue* cmdq, unsigned numVertices, unsigned startIndex );
    void DrawIndexed         ( CommandQueue* cmdq, unsigned numIndices, unsigned startIndex, unsigned baseVertex );
    void DrawInstanced       ( CommandQueue* cmdq, unsigned numVertices, unsigned startIndex, unsigned numInstances );
    void DrawIndexedInstanced( CommandQueue* cmdq, unsigned numIndices, unsigned startIndex, unsigned numInstances, unsigned baseVertex );


    void ClearState       ( CommandQueue* cmdq );
    void ClearBuffers     ( CommandQueue* cmdq, TextureRW* colorTex, unsigned nColor, TextureDepth depthTex, const float rgbad[5], int flag_color, int flag_depth );
    void ClearDepthBuffer ( CommandQueue* cmdq, TextureDepth depthTex, float clearValue );
    void ClearColorBuffers( CommandQueue* cmdq, TextureRW* colorTex, unsigned nColor, float r, float g, float b, float a );
    inline void ClearColorBuffer( CommandQueue* cmdq, TextureRW colorTex, float r, float g, float b, float a )
    {
        ClearColorBuffers( cmdq, &colorTex, 1, r, g, b, a );
    }

    void Swap( CommandQueue* cmdq, unsigned syncInterval = 0 );
    void GenerateMipmaps( CommandQueue* cmdq, TextureRW texture );
    TextureRW GetBackBufferTexture( CommandQueue* cmdq );
}/// 

namespace util
{
    inline unsigned GetNumElements( const BufferDesc& desc )
    {
        return desc.sizeInBytes / desc.format.ByteWidth();
    }
}

}}///

