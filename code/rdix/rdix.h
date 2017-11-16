#pragma once

#include <foundation/type.h>
#include <initializer_list>

#include "rdix_type.h"
#include <util/par_shapes/par_shapes.h>

struct RDICommandQueue;
struct RDIDevice;
struct RDIXCommand;
struct RDIXCommandBuffer;
struct BXIAllocator;

// ---
struct BXIFilesystem;

// --- Shader
RDIXShaderFile* LoadShaderFile  ( const char* name, BXIFilesystem* filesystem, BXIAllocator* allocator );
RDIXShaderFile* CreateShaderFile( const void* data, uint32_t dataSize );
void		    UnloadShaderFile( RDIXShaderFile** shaderfile, BXIAllocator* allocator );
uint32_t		GenerateShaderFileHashedName( const char* name, uint32_t version );
uint32_t		FindShaderPass( RDIXShaderFile* shaderfile, const char* passname );

// --- Pipeline
RDIXPipeline*		 CreatePipeline ( RDIDevice* dev, const RDIXPipelineDesc& desc, BXIAllocator* allocator );
void				 DestroyPipeline( RDIDevice* dev, RDIXPipeline** pipeline, BXIAllocator* allocator );
void				 BindPipeline( RDICommandQueue* cmdq, RDIXPipeline* pipeline, bool bindResources );
RDIXResourceBinding* ResourceBinding( const RDIXPipeline* p );


// --- Resources
RDIXResourceBinding* CreateResourceBinding ( const RDIXResourceLayout& layout, BXIAllocator* allocator );
void				 DestroyResourceBinding( RDIXResourceBinding** binding, BXIAllocator* allocator );
RDIXResourceBinding* CloneResourceBinding( const RDIXResourceBinding* binding, BXIAllocator* allocator );
bool				 ClearResource( RDICommandQueue* cmdq, RDIXResourceBinding* binding, const char* name );
void				 BindResources( RDICommandQueue* cmdq, RDIXResourceBinding* binding );
uint32_t			 FindResource( RDIXResourceBinding* binding, const char* name );
void				 SetResourceROByIndex( RDIXResourceBinding* binding, uint32_t index, const RDIResourceRO* resource );
void				 SetResourceRWByIndex( RDIXResourceBinding* binding, uint32_t index, const RDIResourceRW* resource );
void				 SetConstantBufferByIndex( RDIXResourceBinding* binding, uint32_t index, const RDIConstantBuffer* cbuffer );
void				 SetSamplerByIndex( RDIXResourceBinding* binding, uint32_t index, const RDISampler* sampler );
bool				 SetResourceRO( RDIXResourceBinding* binding, const char* name, const RDIResourceRO* resource );
bool				 SetResourceRW( RDIXResourceBinding* binding, const char* name, const RDIResourceRW* resource );
bool				 SetConstantBuffer( RDIXResourceBinding* binding, const char* name, const RDIConstantBuffer* cbuffer );
bool				 SetSampler( RDIXResourceBinding* binding, const char* name, const RDISampler* sampler );

uint32_t			 GenerateResourceHashedName( const char* name );
RDIXResourceBindingMemoryRequirments CalculateResourceBindingMemoryRequirments( const RDIXResourceLayout& layout );



// --- RenderTarget
RDIXRenderTarget* CreateRenderTarget ( RDIDevice* dev, const RDIXRenderTargetDesc& desc, BXIAllocator* allocator );
void			  DestroyRenderTarget( RDIDevice* dev, RDIXRenderTarget** renderTarget, BXIAllocator* allocator );
void			  ClearRenderTarget( RDICommandQueue* cmdq, RDIXRenderTarget* rtarget, float rgbad[5] );
void			  ClearRenderTarget( RDICommandQueue* cmdq, RDIXRenderTarget* rtarget, float r, float g, float b, float a, float d );
void			  ClearRenderTargetDepth( RDICommandQueue* cmdq, RDIXRenderTarget* rtarget, float d );
void			  BindRenderTarget( RDICommandQueue* cmdq, RDIXRenderTarget* renderTarget, const std::initializer_list<uint8_t>& colorTextureIndices, bool useDepth );
void			  BindRenderTarget( RDICommandQueue* cmdq, RDIXRenderTarget* renderTarget );
RDITextureRW	  Texture( RDIXRenderTarget* rtarget, uint32_t index );
RDITextureDepth   TextureDepth( RDIXRenderTarget* rtarget );

// --- RenderSource
RDIXRenderSource* CreateRenderSource ( RDIDevice* dev, const RDIXRenderSourceDesc& desc, BXIAllocator* allocator );
RDIXRenderSource* CreateRenderSourceFromShape( RDIDevice* dev, const par_shapes_mesh* shape, BXIAllocator* allocator );
void			  DestroyRenderSource( RDIDevice* dev, RDIXRenderSource** rsource, BXIAllocator* allocator );
void			  BindRenderSource( RDICommandQueue* cmdq, RDIXRenderSource* renderSource );
void			  SubmitRenderSource( RDICommandQueue* cmdq, RDIXRenderSource* renderSource, uint32_t rangeIndex = 0 );
void			  SubmitRenderSourceInstanced( RDICommandQueue* cmdq, RDIXRenderSource* renderSource, uint32_t numInstances, uint32_t rangeIndex = 0 );


uint32_t             NumVertexBuffers( RDIXRenderSource* rsource );
uint32_t             NumVertices( RDIXRenderSource* rsource );
uint32_t             NumIndices( RDIXRenderSource* rsource );
uint32_t             NumRanges( RDIXRenderSource* rsource );
RDIVertexBuffer      VertexBuffer( RDIXRenderSource* rsource, uint32_t index );
RDIIndexBuffer       IndexBuffer( RDIXRenderSource* rsource );
RDIXRenderSourceRange Range( RDIXRenderSource* rsource, uint32_t index );

// --- TransformBuffer
RDIXTransformBuffer* CreateTransformBuffer( RDIDevice* dev, const RDIXTransformBufferDesc& desc, BXIAllocator* allocator );
void				 DestroyTransformBuffer( RDIDevice* dev, RDIXTransformBuffer** buffer, BXIAllocator* allocator );
void				 ClearTransformBuffer( RDIXTransformBuffer* buffer );
uint32_t			 AppendMatrix( RDIXTransformBuffer* buffer, const struct mat44_t& matrix );

void				 UploadTransformBuffer( RDICommandQueue* cmdq, RDIXTransformBuffer* buffer );
RDIXCommand*		 UploadTransformBuffer( RDIXCommandBuffer* cmdbuff, RDIXCommand* parentcmd, RDIXTransformBuffer* buffer );

void				 BindTransformBuffer( RDICommandQueue* cmdq, RDIXTransformBuffer* buffer, uint32_t slot, uint32_t stagemask );
RDIXCommand*		 BindTransformBuffer( RDIXCommandBuffer* cmdbuff, RDIXCommand* parentcmd, RDIXTransformBuffer* buffer, uint32_t slot, uint32_t stagemask );

struct RDIXTransformBufferCommands
{
	RDIXCommand* first;
	RDIXCommand* last;
};
RDIXTransformBufferCommands UploadAndSetTransformBuffer( RDIXCommandBuffer* cmdbuff, RDIXCommand* parentcmd, RDIXTransformBuffer* buffer, uint32_t slot, uint32_t stagemask );