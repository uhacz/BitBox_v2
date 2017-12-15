#include "asset_app.h"

#include <window\window.h>
#include <window\window_interface.h>

#include <filesystem\filesystem_plugin.h>

#include <foundation\plugin\plugin_registry.h>
#include <foundation\memory\memory.h>
#include <foundation/time.h>

#include <util/par_shapes/par_shapes.h>

#include <rdix/rdix.h>
#include <rdix/rdix_command_buffer.h>
#include <gfx/gfx.h>
#include <gfx/gfx_camera.h>

#include <string.h>

#include <gfx/gfx_shader_interop.h>

#include <shaders/hlsl/material_frame_data.h>
#include <shaders/hlsl/material_data.h>
#include <shaders/hlsl/transform_instance_data.h>

#include <tuple>
#include <foundation/id_array.h>
#include <foundation/dense_container.h>

namespace
{
	// batch
	static RDIXCommandBuffer* g_cmdbuffer = nullptr;
	static RDIXTransformBuffer* g_transform_buffer = nullptr;
	
	// object
	static RDIXRenderSource* g_rsource = nullptr;
	static mat44_t g_instance_matrix = mat44_t::identity();

	// camera
	static mat44_t g_camera_world = mat44_t::identity();
	static GFXCameraParams g_camera_params = {};
	static GFXCameraMatrices g_camera_matrices = {};

	// material system
	static RDIConstantBuffer g_material_data_gpu = {};
	static RDIConstantBuffer g_material_frame_data_gpu = {};
	static RDIConstantBuffer g_transform_instance_data_gpu = {};

	// material instance
	static Material g_material = {};
	static RDIXResourceBinding* g_material_resource_binding = nullptr;
	
	GFXCameraInputContext g_camera_input_ctx = {};

	GFX g_gfx = {};

}

#include <foundation/hash.h>
namespace dense_container
{
    static constexpr uint8_t MAX_STREAMS = 16;
}//
struct dense_container_desc_t
{
    uint8_t strides[dense_container::MAX_STREAMS] = {};
    uint32_t names_hash[dense_container::MAX_STREAMS] = {};
    uint32_t num_streams = 0;

    template< typename T >
    void add_stream( const char* name )
    {
        SYS_ASSERT( num_streams < dense_container::MAX_STREAMS );
        const uint32_t index = num_streams++;
        strides[index] = sizeof( T );

        const uint32_t name_len = (uint32_t)strlen( name );
        names_hash[index] = murmur3_32x86_hash( name, name_len, name_len * sizeof( T ) );
    }
};

template< uint32_t MAX >
struct BIT_ALIGNMENT_16 dense_container_type
{
    BXIAllocator* _allocator;
    id_array_t<MAX> _ids;
    
    uint32_t _num_streams;
    uint8_t  _streams_stride[dense_container::MAX_STREAMS];
    uint32_t _names_hash[dense_container::MAX_STREAMS];
    uint8_t* _streams[dense_container::MAX_STREAMS];

    id_t create()
    {
        id_t id = id_array::create( _ids );
        return id;
    }

    void destroy( id_t id )
    {
        if( !id_array::has( _ids, id ) )
            return;

        const id_array_destroy_info_t copy_info = id_array::destroy( _ids, id );
        for( uint32_t i = 0; i < _num_streams; ++i )
        {
            const uint8_t stride = _streams_stride[i];
            const uint8_t* src = _streams[i] + copy_info.copy_data_from_index * stride;
            uint8_t* dst = _streams[i] + copy_info.copy_data_to_index * stride;
            memcpy( dst, src, stride );
        }
    }

    template< uint32_t STREAM_INDEX, typename T >
    const T& get( id_t id ) const
    {
        SYS_ASSERT( STREAM_INDEX < _num_streams );
        SYS_ASSERT( sizeof( T ) == _streams_stride[STREAM_INDEX] );
        const uint32_t index = id_array::index( _id, id );
        return (const T&)((T*)_streams[STREAM_INDEX])[index];
    }
    
    template< uint32_t STREAM_INDEX, typename T >
    void set( id_t id, const T& value )
    {
        SYS_ASSERT( STREAM_INDEX < _num_streams );
        SYS_ASSERT( sizeof( T ) == _streams_stride[STREAM_INDEX] );
        const uint32_t index = id_array::index( _id, id );
        ((T*)_streams[STREAM_INDEX])[index] = value;
    }

    template< uint32_t STREAM_INDEX, typename T >
    array_span_t<T> stream()
    {
        SYS_ASSERT( STREAM_INDEX < _num_streams );
        T* data = (T*)_streams[STREAM_INDEX];
        return array_span_t<T>( data, id_array::size( _ids ) );
    }
};

namespace dense_container
{
    template< uint32_t MAX >
    dense_container_type<MAX>* create( const dense_container_desc_t& desc, BXIAllocator* allocator )
    {
        using container_t = dense_container_type<MAX>;
        uint32_t mem_size = 0;
        mem_size += sizeof( container_t );
        for( uint32_t i = 0; i < desc.num_streams; ++i )
            mem_size += desc.strides[i] * MAX;

        void* mem = BX_MALLOC( allocator, mem_size, 16 );
        memset( mem, 0x00, mem_size );

        container_t* c = (container_t*)mem;
        c->_allocator = allocator;
        uint8_t* data_begin = (uint8_t*)(c + 1);

        c->_num_streams = desc.num_streams;
        for( uint32_t i = 0; i < desc.num_streams; ++i )
        {
            c->_streams_stride[i] = desc.strides[i];
            c->_names_hash[i] = desc.names_hash[i];
            c->_streams[i] = data_begin;

            data_begin += MAX * desc.strides[i];
        }

        SYS_ASSERT( data_begin == (uint8_t*)mem + mem_size );

        return c;

    }

    template< uint32_t MAX >
    void destroy( dense_container_type<MAX>** cnt )
    {
        if( !*cnt )
            return;

        BXIAllocator* allocator = cnt[0]->_allocator;
        BX_FREE0( allocator, cnt[0] );
    }
}


template< class T >
struct type_adapter_t : public T
{
    constexpr type_adapter_t()
        : T( 0 ) {}

    operator T& (){ return (T&)*this; }
    operator const T&() const { return (const T&)*this; }

    T& operator = ( const T& v )
    {
        *this = v;
        return *this;
    }
};

bool BXAssetApp::Startup( int argc, const char** argv, BXPluginRegistry* plugins, BXIAllocator* allocator )
{
    const uint32_t MAX = 10;
    //dense_container_t < MAX, type_adapter_t<mat44_t>[MAX], int[MAX], float[MAX] > t1;

    //id_t ids[10];
    //for( uint32_t i = 0; i < MAX; ++i )
    //    ids[i] = dense_container::create( t1 );

    //for( uint32_t i = 0; i < MAX; ++i )
    //{
    //    t1.set<0>( ids[i], mat44_t( i ) );
    //    t1.set<1>( ids[i], int( i ) );
    //    t1.set<2>( ids[i], float( i * 10 ) );
    //}
    //int n = t1.NUM_STREAMS;

    //auto stream = t1.stream<0>();
    ////auto& stream = std::get<0>( t1 );

    ////double d9 = t1.get<0, double>( ids[9] );

    ////dense_container::destroy( t1, ids[5] );

    ////d9 = t1.get<0, double>( ids[9] );

    dense_container_desc_t desc;
    desc.add_stream<mat44_t>( "matrix" );
    desc.add_stream<uint32_t>( "number" );
    desc.add_stream<double>( "double" );

    dense_container_type<MAX>* cnt = dense_container::create<MAX>( desc, allocator );
    dense_container::destroy( &cnt );

	_filesystem = (BXIFilesystem*)BXGetPlugin( plugins, BX_FILESYSTEM_PLUGIN_NAME );
	_filesystem->SetRoot( "x:/dev/assets/" );

	BXIWindow* win_plugin = (BXIWindow*)BXGetPlugin( plugins, BX_WINDOW_PLUGIN_NAME );
	const BXWindow* window = win_plugin->GetWindow();
	::Startup( &_rdidev, &_rdicmdq, window->GetSystemHandle( window ), window->width, window->height, 0, allocator );

	
	GFXDesc gfx_desc = {};
	g_gfx.StartUp( gfx_desc, _rdidev, _filesystem, allocator );


	RDIXTransformBufferDesc transform_buffer_desc = {};
	g_transform_buffer = CreateTransformBuffer( _rdidev, transform_buffer_desc, allocator );

	g_cmdbuffer = CreateCommandBuffer( allocator );

	{
		par_shapes_mesh* shape = par_shapes_create_parametric_sphere( 64, 64, FLT_EPSILON );
		//par_shapes_mesh* shape = par_shapes_create_torus( 64, 64, 0.5f);
        //par_shapes_mesh* shape = par_shapes_create_rock( 0xBCAA, 4 );
		g_rsource = CreateRenderSourceFromShape( _rdidev, shape, allocator );
		par_shapes_free_mesh( shape );
	}

	g_material_data_gpu = CreateConstantBuffer( _rdidev, sizeof( Material ) );
	g_material_frame_data_gpu = CreateConstantBuffer( _rdidev, sizeof( MaterialFrameData ) );
	g_transform_instance_data_gpu = CreateConstantBuffer( _rdidev, sizeof( TransformInstanceData ) );

	g_material.specular_albedo = vec3_t( 1.f, 1.f, 1.f );
	g_material.diffuse_albedo = vec3_t( 1.f, 0.f, 0.f );
	g_material.metal = 0.f;
	g_material.roughness = 0.25f;
	UpdateCBuffer( _rdicmdq, g_material_data_gpu, &g_material );
	g_material_resource_binding = CloneResourceBinding( ResourceBinding( g_gfx.MaterialBase() ), allocator );
	SetConstantBuffer( g_material_resource_binding, "MaterialData", &g_material_data_gpu );

	
	GFXUtils::StartUp( _rdidev, _filesystem, allocator );

	g_camera_world = mat44_t( mat33_t::identity(), vec3_t( 0.f, 0.f, 5.f ) );


    return true;
}

void BXAssetApp::Shutdown( BXPluginRegistry* plugins, BXIAllocator* allocator )
{
	g_gfx.ShutDown();
	GFXUtils::ShutDown( _rdidev );

	Destroy( &g_transform_instance_data_gpu );
	Destroy( &g_material_frame_data_gpu );
	Destroy( &g_material_data_gpu );

	DestroyResourceBinding( &g_material_resource_binding, allocator );
	DestroyRenderSource( _rdidev, &g_rsource, allocator );
	DestroyTransformBuffer( _rdidev, &g_transform_buffer, allocator );
	DestroyCommandBuffer( &g_cmdbuffer, allocator );

	::Shutdown( &_rdidev, &_rdicmdq, allocator );
    _filesystem = nullptr;
}



bool BXAssetApp::Update( BXWindow* win, unsigned long long deltaTimeUS, BXIAllocator* allocator )
{
    if( win->input.IsKeyPressedOnce( BXInput::eKEY_ESC ) )
        return false;

	const float delta_time_sec = (float)BXTime::Micro_2_Sec( deltaTimeUS );

	{
		const float sensitivity_in_pix = delta_time_sec;
		BXInput* input = &win->input;
		BXInput::Mouse* input_mouse = &input->mouse;

		const BXInput::MouseState* current_state = input_mouse->CurrentState();

		g_camera_input_ctx.UpdateInput( 
			current_state->lbutton,
			current_state->mbutton,
			current_state->rbutton,
			current_state->dx,
			current_state->dy,
			sensitivity_in_pix,
			delta_time_sec );
	}

	g_camera_world = CameraMovementFPP( g_camera_input_ctx, g_camera_world, delta_time_sec * 10.f );

	ComputeMatrices( &g_camera_matrices, g_camera_params, g_camera_world );
	{
		MaterialFrameData fdata;
		fdata.camera_world = g_camera_matrices.world;
		fdata.camera_view = g_camera_matrices.view;
		fdata.camera_view_proj = g_camera_matrices.view_proj;
		fdata.camera_eye = vec4_t( g_camera_matrices.eye(), 1.f );
		fdata.camera_dir = vec4_t( g_camera_matrices.dir(), 0.f );
		UpdateCBuffer( _rdicmdq, g_material_frame_data_gpu, &fdata );
	};
	
	ClearTransformBuffer( g_transform_buffer );
	uint32_t instance_offset = AppendMatrix( g_transform_buffer, g_instance_matrix );
	
	{
		ClearCommandBuffer( g_cmdbuffer, allocator );
		BeginCommandBuffer( g_cmdbuffer );

		RDIXTransformBufferCommands transform_buff_cmds = UploadAndSetTransformBuffer( g_cmdbuffer, nullptr, g_transform_buffer, TRANSFORM_INSTANCE_WORLD_SLOT, RDIEPipeline::VERTEX_MASK );

		RDIXUpdateConstantBufferCmd* instance_offset_cmd = AllocateCommand<RDIXUpdateConstantBufferCmd>( g_cmdbuffer, sizeof( uint32_t ), transform_buff_cmds.last );
		instance_offset_cmd->cbuffer = g_transform_instance_data_gpu;
		memcpy( instance_offset_cmd->DataPtr(), &instance_offset, 4 );

		RDIXSetPipelineCmd* pipeline_cmd = AllocateCommand<RDIXSetPipelineCmd>( g_cmdbuffer, instance_offset_cmd );
		pipeline_cmd->pipeline = g_gfx.MaterialBase();
		pipeline_cmd->bindResources = false;

		RDIXSetResourcesCmd* resources_cmd = AllocateCommand<RDIXSetResourcesCmd>( g_cmdbuffer, pipeline_cmd );
		resources_cmd->rbind = g_material_resource_binding;
		
		RDIXDrawCmd* draw_cmd = AllocateCommand<RDIXDrawCmd>( g_cmdbuffer, resources_cmd );
		draw_cmd->rsource = g_rsource;
		draw_cmd->num_instances = 1;

		SubmitCommand( g_cmdbuffer, transform_buff_cmds.first, 0 );

		EndCommandBuffer( g_cmdbuffer );
	}

	g_gfx.BeginFrame( _rdicmdq );
	
	SetCbuffers( _rdicmdq, &g_material_frame_data_gpu, MATERIAL_FRAME_DATA_SLOT, 1, RDIEPipeline::ALL_STAGES_MASK );
		
	BindRenderTarget( _rdicmdq, g_gfx.Framebuffer() );
	ClearRenderTarget( _rdicmdq, g_gfx.Framebuffer() , 0.f, 0.f, 0.f, 1.f, 1.f );
	
	SubmitCommandBuffer( _rdicmdq, g_cmdbuffer );

	g_gfx.RasterizeFramebuffer( _rdicmdq, 0, g_camera_params.aspect() );
	g_gfx.EndFrame( _rdicmdq );
	
    return true;
}

BX_APPLICATION_PLUGIN_DEFINE( asset_app, BXAssetApp )