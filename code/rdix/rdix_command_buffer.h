#pragma once

#include <foundation/type.h>
#include <rdi_backend/rdi_backend_type.h>

struct BXIAllocator;
struct RDICommandQueue;
struct RDIXCommandBuffer;
struct RDIXPipeline;
struct RDIXResourceBinding;
struct RDIXRenderTarget;
struct RDIXRenderSource;

struct RDIXCommand;
typedef void( *DispatchFunction )(RDICommandQueue* cmdq, RDIXCommand* cmdAddr);

struct RDIXCommand
{
	DispatchFunction _dispatch_ptr = nullptr;
	RDIXCommand* _next = nullptr;
};
struct RDIXSetPipelineCmd : RDIXCommand
{
	static const DispatchFunction DISPATCH_FUNCTION;
	RDIXPipeline* pipeline = nullptr;
	uint8_t bindResources = 0;
};
struct RDIXSetResourcesCmd : RDIXCommand
{
	static const DispatchFunction DISPATCH_FUNCTION;
	RDIXResourceBinding* rbind = nullptr;
};
struct RDIXSetRenderSourceCmd : RDIXCommand
{
	static const DispatchFunction DISPATCH_FUNCTION;
	RDIXRenderSource* rsource = nullptr;
};

struct RDIXDrawCmd : RDIXCommand
{
	static const DispatchFunction DISPATCH_FUNCTION;
	RDIXRenderSource* rsource = nullptr;
	uint16_t rsouce_range = 0;
	uint16_t num_instances = 0;
};
struct RDIXRawDrawCallCmd : RDIXCommand
{
	static const DispatchFunction DISPATCH_FUNCTION;
	uint32_t num_vertices = 0;
	uint32_t start_index = 0;
	uint32_t num_instances = 0;
};
struct RDIXUpdateConstantBufferCmd : RDIXCommand
{
	static const DispatchFunction DISPATCH_FUNCTION;
	RDIConstantBuffer cbuffer = {};
	uint8_t* DataPtr() { return (uint8_t*)(this + 1); }
};
struct RDIXUpdateBufferCmd : RDIXCommand
{
	static const DispatchFunction DISPATCH_FUNCTION;
	RDIResource resource = {};
	uint32_t size = 0;
	uint8_t* DataPtr() { return (uint8_t*)(this + 1); }
};

using RDIXDrawCallback = void( *)( RDICommandQueue* cmdq, uint32_t flags, void* userData);
struct RDIXDrawCallbackCmd : RDIXCommand
{
	static const DispatchFunction DISPATCH_FUNCTION;
	RDIXDrawCallback ptr;
	void* user_data;
	uint32_t flags;
};

void SetPipelineCmdDispatch         ( RDICommandQueue* cmdq, RDIXCommand* cmdAddr );
void SetResourcesCmdDispatch        ( RDICommandQueue* cmdq, RDIXCommand* cmdAddr );
void SetRenderSourceCmdDispatch     ( RDICommandQueue* cmdq, RDIXCommand* cmdAddr );
void DrawCmdDispatch                ( RDICommandQueue* cmdq, RDIXCommand* cmdAddr );
void RawDrawCallCmdDispatch         ( RDICommandQueue* cmdq, RDIXCommand* cmdAddr );
void UpdateConstantBufferCmdDispatch( RDICommandQueue* cmdq, RDIXCommand* cmdAddr );
void UpdateBufferCmdDispatch        ( RDICommandQueue* cmdq, RDIXCommand* cmdAddr );
void DrawCallbackCmdDispatch        ( RDICommandQueue* cmdq, RDIXCommand* cmdAddr );

//////////////////////////////////////////////////////////////////////////
/// @dataCapacity : additional data for commands eg. for UpdateConstantBufferCmd data
RDIXCommandBuffer* CreateCommandBuffer( BXIAllocator* allocator, uint32_t maxCommands = 64, uint32_t dataCapacity = 1024 * 4 );
void DestroyCommandBuffer( RDIXCommandBuffer** cmdBuff, BXIAllocator* allocator );
void ClearCommandBuffer  ( RDIXCommandBuffer* cmdBuff, BXIAllocator* allocator );
void BeginCommandBuffer  ( RDIXCommandBuffer* cmdBuff );
void EndCommandBuffer    ( RDIXCommandBuffer* cmdBuff );
bool SubmitCommand       ( RDIXCommandBuffer* cmdbuff, RDIXCommand* cmdPtr, uint64_t sortKey );
void* _AllocateCommand   ( RDIXCommandBuffer* cmdbuff, uint32_t cmdSize );
void SubmitCommandBuffer ( RDICommandQueue* cmdq, RDIXCommandBuffer* cmdBuff );

template< typename T >
T* AllocateCommand( RDIXCommandBuffer* cmdbuff, uint32_t dataSize, RDIXCommand* parent_cmd )
{
	u32 mem_size = sizeof( T ) + dataSize;
	void* mem = _AllocateCommand( cmdbuff, mem_size );
	if( !mem )
		return nullptr;

	T* cmd = new(mem) T();
	cmd->_dispatch_ptr = T::DISPATCH_FUNCTION;
	if( parent_cmd )
	{
		SYS_ASSERT( parent_cmd->_next == nullptr );
		parent_cmd->_next = cmd;
	}
	return cmd;
}

template< typename T >
inline T* AllocateCommand( RDIXCommandBuffer* cmdbuff, RDIXCommand* parent_cmd )
{
	return AllocateCommand<T>( cmdbuff, 0, parent_cmd );
}
