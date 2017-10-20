#pragma once

#include <foundation/type.h>
#include <initializer_list>

#include "gfx_type.h"

struct RDICommandQueue;
struct RDIDevice;
struct BXIAllocator;

// --- 
GFXShaderFile* LoadShaderFile  ( RDIDevice* dev, const char* name );
void		   UnloadShaderFile( RDIDevice* dev, GFXShaderFile** shaderfile );

GFXPipeline* CreatePipeline ( RDIDevice* dev, const GFXPipelineDesc& desc );
void		 DestroyPipeline( RDIDevice* dev, GFXPipeline** pipeline );

GFXResourceBinding* CreateResourceBinding	 ( RDIDevice* dev, const GFXResourceLayout& layout );
void				DestroyResourceDescriptor( RDIDevice* dev, GFXResourceBinding** binding );

GFXRenderTarget* CreateRenderTarget ( RDIDevice* dev, const GFXRenderTargetDesc& desc );
void			 DestroyRenderTarget( RDIDevice* dev, GFXRenderTarget** renderTarget );

GFXRenderSource* CreateRenderSource ( RDIDevice* dev, const GFXRenderSourceDesc& desc );
void			 DestroyRenderSource( RDIDevice* dev, GFXRenderSource** rsource );
	
// --- 
void BindPipeline ( RDICommandQueue* cmdq, GFXPipeline* pipeline, bool bindResources );
	
bool ClearResource( RDICommandQueue* cmdq, GFXResourceBinding* binding, const char* name );
void BindResources( RDICommandQueue* cmdq, GFXResourceBinding* binding );

void ClearRenderTarget     ( RDICommandQueue* cmdq, GFXRenderTarget* rtarget, float r, float g, float b, float a, float d );
void ClearRenderTargetDepth( RDICommandQueue* cmdq, GFXRenderTarget* rtarget, float d );
void BindRenderTarget      ( RDICommandQueue* cmdq, GFXRenderTarget* renderTarget, const std::initializer_list<uint8_t>& colorTextureIndices, bool useDepth );
void BindRenderTarget      ( RDICommandQueue* cmdq, GFXRenderTarget* renderTarget );

void BindRenderSource	        ( RDICommandQueue* cmdq, GFXRenderSource* renderSource );
void SubmitRenderSource         ( RDICommandQueue* cmdq, GFXRenderSource* renderSource, uint32_t rangeIndex = 0 );
void SubmitRenderSourceInstanced( RDICommandQueue* cmdq, GFXRenderSource* renderSource, uint32_t numInstances, uint32_t rangeIndex = 0 );


// --- utils
// -- shader file
uint32_t GenerateShaderFileHashedName( const char* name, uint32_t version );
uint32_t FindShaderPass			    ( GFXShaderFile* shaderfile, const char* passname );
	
// --- pipeline
GFXResourceBinding* ResourceBinding (const GFXPipeline* p);

// --- resources
GFXResourceBindingMemoryRequirments CalculateResourceBindingMemoryRequirments( const GFXResourceLayout& layout );
uint32_t							GenerateResourceHashedName				 ( const char* name );

uint32_t FindResource            ( GFXResourceBinding* binding, const char* name );
void	 SetResourceROByIndex    ( GFXResourceBinding* binding, uint32_t index, const RDIResourceRO* resource );
void	 SetResourceRWByIndex    ( GFXResourceBinding* binding, uint32_t index, const RDIResourceRW* resource );
void	 SetConstantBufferByIndex( GFXResourceBinding* binding, uint32_t index, const RDIConstantBuffer* cbuffer );
void	 SetSamplerByIndex       ( GFXResourceBinding* binding, uint32_t index, const RDISampler* sampler );

bool	SetResourceRO	 ( GFXResourceBinding* binding, const char* name, const RDIResourceRO* resource );
bool	SetResourceRW	 ( GFXResourceBinding* binding, const char* name, const RDIResourceRW* resource );
bool	SetConstantBuffer( GFXResourceBinding* binding, const char* name, const RDIConstantBuffer* cbuffer );
bool	SetSampler		 ( GFXResourceBinding* binding, const char* name, const RDISampler* sampler );

// --- render target
static RDITextureRW		Texture	    ( GFXRenderTarget* rtarget, uint32_t index );
static RDITextureDepth  TextureDepth( GFXRenderTarget* rtarget );

// --- render source
uint32_t             NumVertexBuffers( GFXRenderSource* rsource );
uint32_t             NumVertices     ( GFXRenderSource* rsource );
uint32_t             NumIndices      ( GFXRenderSource* rsource );
uint32_t             NumRanges       ( GFXRenderSource* rsource );
RDIVertexBuffer      VertexBuffer    ( GFXRenderSource* rsource, uint32_t index );
RDIIndexBuffer       IndexBuffer     ( GFXRenderSource* rsource );
GFXRenderSourceRange Range           ( GFXRenderSource* rsource, uint32_t index );



