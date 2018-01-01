#include "rdix_command_buffer.h"
#include "rdix.h"
#include <rdi_backend/rdi_backend.h>

#include <foundation/memory/memory.h>
#include <foundation/buffer.h>

#include <string.h>
#include <algorithm>

void SetPipelineCmdDispatch( RDICommandQueue* cmdq, RDIXCommand* cmdAddr )
{
	RDIXSetPipelineCmd* cmd = (RDIXSetPipelineCmd*)cmdAddr;
	BindPipeline( cmdq, cmd->pipeline, cmd->bindResources ? true : false );
}
void SetResourcesCmdDispatch( RDICommandQueue* cmdq, RDIXCommand* cmdAddr )
{
	RDIXSetResourcesCmd* cmd = (RDIXSetResourcesCmd*)cmdAddr;
	BindResources( cmdq, cmd->rbind );
}

void SetResourceROCmdDispatch( RDICommandQueue* cmdq, RDIXCommand* cmdAddr )
{
	auto* cmd = (RDIXSetResourceROCmd*)cmdAddr;
	SetResourcesRO( cmdq, &cmd->resource, cmd->slot, 1, cmd->stage_mask );
}

void SetConstantBufferCmdDispatch( RDICommandQueue * cmdq, RDIXCommand * cmdAddr )
{
    auto* cmd = (RDIXSetConstantBufferCmd*)cmdAddr;
    SetCbuffers( cmdq, &cmd->resource, cmd->slot, 1, cmd->stage_mask );
}

void SetRenderSourceCmdDispatch( RDICommandQueue * cmdq, RDIXCommand * cmdAddr )
{
	RDIXSetRenderSourceCmd* cmd = (RDIXSetRenderSourceCmd*)cmdAddr;
	BindRenderSource( cmdq, cmd->rsource );
}

void DrawCmdDispatch( RDICommandQueue* cmdq, RDIXCommand* cmdAddr )
{
	RDIXDrawCmd* cmd = (RDIXDrawCmd*)cmdAddr;
	BindRenderSource( cmdq, cmd->rsource );
	SubmitRenderSourceInstanced( cmdq, cmd->rsource, cmd->num_instances, cmd->rsouce_range );
}

void RawDrawCallCmdDispatch( RDICommandQueue * cmdq, RDIXCommand * cmdAddr )
{
	RDIXRawDrawCallCmd* cmd = (RDIXRawDrawCallCmd*)cmdAddr;
	DrawInstanced( cmdq, cmd->num_vertices, cmd->start_index, cmd->num_instances );
}

void UpdateConstantBufferCmdDispatch( RDICommandQueue* cmdq, RDIXCommand* cmdAddr )
{
	RDIXUpdateConstantBufferCmd* cmd = (RDIXUpdateConstantBufferCmd*)cmdAddr;
	UpdateCBuffer( cmdq, cmd->cbuffer, cmd->DataPtr() );
}

void UpdateBufferCmdDispatch( RDICommandQueue* cmdq, RDIXCommand* cmdAddr )
{
	RDIXUpdateBufferCmd* cmd = (RDIXUpdateBufferCmd*)cmdAddr;
	uint8_t* mapped_ptr = Map( cmdq, cmd->resource, 0, RDIEMapType::WRITE );
	memcpy( mapped_ptr, cmd->DataPtr(), cmd->size );
	Unmap( cmdq, cmd->resource );
}

void DrawCallbackCmdDispatch( RDICommandQueue* cmdq, RDIXCommand* cmdAddr )
{
	RDIXDrawCallbackCmd* cmd = (RDIXDrawCallbackCmd*)cmdAddr;
	(*cmd->ptr)(cmdq, cmd->flags, cmd->user_data);
}
// ---
const DispatchFunction RDIXDrawCmd::DISPATCH_FUNCTION                 = DrawCmdDispatch;
const DispatchFunction RDIXUpdateConstantBufferCmd::DISPATCH_FUNCTION = UpdateConstantBufferCmdDispatch;
const DispatchFunction RDIXSetPipelineCmd::DISPATCH_FUNCTION          = SetPipelineCmdDispatch;
const DispatchFunction RDIXSetResourcesCmd::DISPATCH_FUNCTION         = SetResourcesCmdDispatch;
const DispatchFunction RDIXSetResourceROCmd::DISPATCH_FUNCTION		  = SetResourceROCmdDispatch;
const DispatchFunction RDIXSetConstantBufferCmd::DISPATCH_FUNCTION    = SetConstantBufferCmdDispatch;
const DispatchFunction RDIXSetRenderSourceCmd::DISPATCH_FUNCTION      = SetRenderSourceCmdDispatch;
const DispatchFunction RDIXRawDrawCallCmd::DISPATCH_FUNCTION          = RawDrawCallCmdDispatch;
const DispatchFunction RDIXUpdateBufferCmd::DISPATCH_FUNCTION         = UpdateBufferCmdDispatch;
const DispatchFunction RDIXDrawCallbackCmd::DISPATCH_FUNCTION         = DrawCallbackCmdDispatch;
// --- 
struct RDIXCommandBuffer
{
	struct CmdInternal
	{
		uint64_t key;
		RDIXCommand* cmd;
	};

	struct Data
	{
		void* _memory_handle = nullptr;
		CmdInternal* commands = nullptr;
		uint8_t* data = nullptr;

		uint32_t max_commands = 0;
		uint32_t num_commands = 0;
		uint32_t data_capacity = 0;
		uint32_t data_size = 0;
	}_data;

	union
	{
		uint32_t all = 0;
		struct
		{
			uint16_t commands;
			uint16_t data;
		};
	} _overflow;

	uint32_t _can_add_commands = 0;

	void AllocateData( uint32_t maxCommands, uint32_t dataCapacity, BXIAllocator* allocator );
	void FreeData( BXIAllocator* allocator );
};

void RDIXCommandBuffer::AllocateData( uint32_t maxCommands, uint32_t dataCapacity, BXIAllocator* allocator )
{
	SYS_ASSERT( _can_add_commands == 0 );

	if( _data.max_commands >= maxCommands && _data.data_capacity >= dataCapacity )
		return;

	uint32_t mem_size = maxCommands * sizeof( CmdInternal );
	mem_size += dataCapacity;

	void* mem = BX_MALLOC( allocator, mem_size, 16 );
	memset( mem, 0x00, mem_size );

	Data newdata = {};
	newdata._memory_handle = mem;
	newdata.max_commands = maxCommands;
	newdata.data_capacity = dataCapacity;

	BufferChunker chunker( mem, mem_size );
	newdata.commands = chunker.Add< CmdInternal >( maxCommands );
	newdata.data = chunker.AddBlock( dataCapacity );
	chunker.Check();

	FreeData( allocator );
	_data = newdata;
}

void RDIXCommandBuffer::FreeData( BXIAllocator* allocator )
{
	BX_FREE( allocator, _data._memory_handle );
	_data = {};
}

RDIXCommandBuffer* CreateCommandBuffer( BXIAllocator* allocator, uint32_t maxCommands, uint32_t dataCapacity )
{
	RDIXCommandBuffer* impl = BX_NEW( allocator, RDIXCommandBuffer );

	dataCapacity += maxCommands * sizeof( RDIXDrawCmd );
	impl->AllocateData( maxCommands, dataCapacity, allocator );
	return impl;
}

void DestroyCommandBuffer( RDIXCommandBuffer** cmdBuff, BXIAllocator* allocator )
{
	if( !cmdBuff[0] )
		return;

	RDIXCommandBuffer* impl = cmdBuff[0];
	impl->FreeData( allocator );
	BX_FREE0( allocator, cmdBuff[0] );
}

void ClearCommandBuffer( RDIXCommandBuffer* cmdBuff, BXIAllocator* allocator )
{
	if( cmdBuff->_overflow.all )
	{
		uint32_t maxCommands = (cmdBuff->_overflow.commands) ? cmdBuff->_data.max_commands * 2 : cmdBuff->_data.max_commands;
		uint32_t dataCapacity = (cmdBuff->_overflow.data) ? cmdBuff->_data.data_capacity * 2 : cmdBuff->_data.data_capacity;
		cmdBuff->AllocateData( maxCommands, dataCapacity, allocator );
		cmdBuff->_overflow.all = 0;
	}
	cmdBuff->_data.num_commands = 0;
	cmdBuff->_data.data_size = 0;
}

void BeginCommandBuffer( RDIXCommandBuffer* cmdBuff )
{
	cmdBuff->_can_add_commands = 1;
}
void EndCommandBuffer( RDIXCommandBuffer* cmdBuff )
{
	cmdBuff->_can_add_commands = 0;
}
void SubmitCommandBuffer( RDICommandQueue* cmdq, RDIXCommandBuffer* cmdBuff )
{
	SYS_ASSERT( cmdBuff->_can_add_commands == 0 );

	struct CmdInternalCmp {
		inline bool operator () ( const RDIXCommandBuffer::CmdInternal a, const RDIXCommandBuffer::CmdInternal b ) { return a.key < b.key; }
	};

	RDIXCommandBuffer::Data& data = cmdBuff->_data;
	std::sort( data.commands, data.commands + data.num_commands, CmdInternalCmp() );

	for( uint32_t i = 0; i < data.num_commands; ++i )
	{
		RDIXCommand* cmd = data.commands[i].cmd;
		while( cmd )
		{
			(*cmd->_dispatch_ptr)(cmdq, cmd);
			cmd = cmd->_next;
		}
	}
}
bool SubmitCommand( RDIXCommandBuffer* cmdbuff, RDIXCommand* cmdPtr, uint64_t sortKey )
{
	SYS_ASSERT( cmdbuff->_can_add_commands );
	RDIXCommandBuffer::Data& data = cmdbuff->_data;
	if( data.num_commands >= data.max_commands )
		return false;

	uint32_t index = data.num_commands++;
	RDIXCommandBuffer::CmdInternal cmd_int = {};
	cmd_int.key = sortKey;
	cmd_int.cmd = cmdPtr;
	data.commands[index] = cmd_int;
	return true;
}
void* _AllocateCommand( RDIXCommandBuffer* cmdbuff, uint32_t cmdSize )
{
	SYS_ASSERT( cmdbuff->_can_add_commands );
	RDIXCommandBuffer::Data& data = cmdbuff->_data;
	if( data.data_size + cmdSize > data.data_capacity )
		return nullptr;

	uint8_t* ptr = data.data + data.data_size;
	data.data_size += cmdSize;

	return ptr;
}