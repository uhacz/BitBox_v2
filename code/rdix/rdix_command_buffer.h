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

struct RDIXSetRenderTargetCmd : RDIXCommand
{
    static const DispatchFunction DISPATCH_FUNCTION;
    RDIXRenderTarget* rtarget = nullptr;
    uint8_t color_textures_mask = 0;
    uint8_t depth = 0;

    RDIXSetRenderTargetCmd() = default;
    RDIXSetRenderTargetCmd( RDIXRenderTarget* rt )
        : rtarget( rt ), color_textures_mask( 0xFF ), depth( 1 )
    {}
    RDIXSetRenderTargetCmd( RDIXRenderTarget* rt, const std::initializer_list<uint8_t>& color_textures_indices, bool usedepth )
        : rtarget( rt )
        , depth( usedepth )
    {
        for( uint8_t i : color_textures_indices )
        {
            color_textures_mask |= 1 << i;
        }
    }
};

struct RDIXSetPipelineCmd : RDIXCommand
{
	static const DispatchFunction DISPATCH_FUNCTION;
	RDIXPipeline* pipeline = nullptr;
	uint8_t bindResources = 0;

    RDIXSetPipelineCmd( RDIXPipeline* p, uint8_t br )
        : pipeline( p ), bindResources( br ) {}
};
struct RDIXSetResourcesCmd : RDIXCommand
{
	static const DispatchFunction DISPATCH_FUNCTION;
	RDIXResourceBinding* rbind = nullptr;

    RDIXSetResourcesCmd( RDIXResourceBinding* rb )
        : rbind( rb ) {}
};

struct RDIXSetResourceROCmd : RDIXCommand
{
	static const DispatchFunction DISPATCH_FUNCTION;
	RDIResourceRO resource;
	uint8_t slot = 0;
	uint8_t stage_mask = 0;

    RDIXSetResourceROCmd() = default;
    RDIXSetResourceROCmd( const RDIResourceRO& rs, uint8_t s, uint8_t sm )
        : resource( rs ), slot( s ), stage_mask( sm ) {}

};
struct RDIXSetConstantBufferCmd : RDIXCommand
{
    static const DispatchFunction DISPATCH_FUNCTION;
    RDIConstantBuffer resource;
    uint8_t slot = 0;
    uint8_t stage_mask = 0;

    RDIXSetConstantBufferCmd() = default;
    RDIXSetConstantBufferCmd( const RDIConstantBuffer& cb, uint8_t s, uint8_t sm )
        : resource( cb ), slot( s ), stage_mask( sm ) {}
};

struct RDIXSetRenderSourceCmd : RDIXCommand
{
	static const DispatchFunction DISPATCH_FUNCTION;
	RDIXRenderSource* rsource = nullptr;

    RDIXSetRenderSourceCmd( RDIXRenderSource* rs ) : rsource( rs ) {}
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

    RDIXUpdateConstantBufferCmd() = default;
    RDIXUpdateConstantBufferCmd( const RDIConstantBuffer& cb, const void* data );

};
struct RDIXUpdateBufferCmd : RDIXCommand
{
	static const DispatchFunction DISPATCH_FUNCTION;
	RDIResource resource = {};
	uint32_t size = 0;
	uint8_t* DataPtr() { return (uint8_t*)(this + 1); }

    RDIXUpdateBufferCmd() = default;
    RDIXUpdateBufferCmd( const RDIResource& rs, const void* data, uint32_t datasize );
};

using RDIXDrawCallback = void( *)( RDICommandQueue* cmdq, uint32_t flags, void* userData);
struct RDIXDrawCallbackCmd : RDIXCommand
{
	static const DispatchFunction DISPATCH_FUNCTION;
	RDIXDrawCallback ptr;
	void* user_data;
	uint32_t flags;
};

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

template< typename T, class ...CmdArgs >
T* AllocateCommand( RDIXCommandBuffer* cmdbuff, uint32_t dataSize, RDIXCommand* parent_cmd, CmdArgs&&... cmdargs )
{
	uint32_t mem_size = sizeof( T ) + dataSize;
	void* mem = _AllocateCommand( cmdbuff, mem_size );
	if( !mem )
		return nullptr;

    T* cmd = new(mem) T( std::forward<CmdArgs>( cmdargs )... );
	cmd->_dispatch_ptr = T::DISPATCH_FUNCTION;
	if( parent_cmd )
	{
		SYS_ASSERT( parent_cmd->_next == nullptr );
		parent_cmd->_next = cmd;
	}
	return cmd;
}

template< typename T, class ...CmdArgs >
inline T* AllocateCommand( RDIXCommandBuffer* cmdbuff, RDIXCommand* parent_cmd, CmdArgs&&... cmdargs )
{
	return AllocateCommand<T>( cmdbuff, 0, parent_cmd, std::forward<CmdArgs>( cmdargs )... );
}

// --- helper
struct RDIXCommandChain
{
    RDIXCommandChain( RDIXCommandBuffer* cmdbuff, RDIXCommand* parent_cmd )
        : _cmdbuff( cmdbuff ), _head_cmd( parent_cmd ), _tail_cmd( parent_cmd )
    {}

    template< typename T, class ...CmdArgs >
    T* AppendCmd( CmdArgs&&... cmdargs )
    {
        T* cmd = AllocateCommand<T>( _cmdbuff, _tail_cmd, std::forward<CmdArgs>( cmdargs )... );
        _tail_cmd = cmd;
        if( !_head_cmd )
            _head_cmd = cmd;
        return cmd;
    }

    template< typename T, class ...CmdArgs >
    T* AppendCmdWithData( uint32_t data_size, CmdArgs&&... cmdargs )
    {
        T* cmd = AllocateCommand<T>( _cmdbuff, data_size, _tail_cmd, std::forward<CmdArgs>( cmdargs )... );
        _tail_cmd = cmd;
        if( !_head_cmd )
            _head_cmd = cmd;
        return cmd;
    }

    void Submit( uint64_t sort_key )
    {
        SubmitCommand( _cmdbuff, _head_cmd, sort_key );
    }


    RDIXCommandBuffer* _cmdbuff = nullptr;
    RDIXCommand* _head_cmd = nullptr;
    RDIXCommand* _tail_cmd = nullptr;
};