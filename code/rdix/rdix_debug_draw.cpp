#include "rdix_debug_draw.h"
#include "rdix.h"

#include <foundation/array.h>
#include <util/poly_shape/poly_shape.h>
#include <resource_manager/resource_manager.h>
#include <rdi_backend/rdi_backend.h>
#include <atomic>

namespace RDIXDebug
{
struct Vertex
{
    vec3_t pos;
    uint32_t color;
};

struct Cmd
{
    uint32_t data_offset;
    uint32_t data_count; // depends of cmd type (ex. number of instances, or number of vertices)
    union
    {
        uint32_t key;
        struct  
        {
            uint32_t rsource_index : 8;
            uint32_t pipeline_index : 8;
            uint32_t depth : 16;
        };
    };
};

enum EDrawRange : uint32_t
{
    DRAW_RANGE_BOX = 0,
    DRAW_RANGE_SHERE,
};

enum ERSource : uint32_t
{
    RSOURCE_OBJ = 0,
    RSOURCE_LINES,
    RSOURCE_COUNT,
};

enum EPipeline : uint32_t
{
    PIPELINE_OBJ_DEPTH_WIREFRAME = 0,
    PIPELINE_OBJ_NDEPTH_WIREFRAME,
    PIPELINE_OBJ_DEPTH_SOLID,
    PIPELINE_OBJ_NDEPTH_SOLID,
    PIPELINE_LINES_DEPTH,
    PIPELINE_LINES_NDEPTH,
    PIPELINE_COUNT,
};

static constexpr uint32_t INITIAL_INSTANCE_CAPACITY = 128;
static constexpr uint32_t INITIAL_VERTEX_CAPACITY = 512;
static constexpr uint32_t INITIAL_CMD_CAPACITY = 128;
static constexpr uint32_t GPU_LINE_VBUFFER_CAPACITY = 1024 * 4; // number of vertices, NOT lines


template<typename T>
struct Array
{
    T* data = nullptr;
    uint32_t capacity = 0;
    std::atomic_uint32_t count = 0;

    void Init( uint32_t initial_cap, BXIAllocator* allocator )
    {
        if( initial_cap <= capacity )
            return;

        if( data )
        {
            BX_FREE0( allocator, data );
        }

        data = (T*)BX_MALLOC( allocator, initial_cap * sizeof(T), ALIGNOF( T ) );
        capacity = initial_cap;
        count = 0;
    }

    T* Add( uint32_t amount )
    {
        uint32_t index = count.fetch_add( amount, std::memory_order_acquire );
        if( (index + amount) > capacity )
            return nullptr;

        return &data[index];
    }

    void Clear( BXIAllocator* allocator )
    {
        const int32_t overflow = count - capacity;
        if( overflow > 0 )
        {
            Init( capacity * 2 );
        }

        count = 0;
    }

    void Free( BXIAllocator* allocator )
    {
        BX_FREE0( allocator, data );
        capacity = 0;
        count = 0;
    }
};

struct Data
{
    BXIAllocator* allocator = nullptr;

    RDIXRenderSource* rsource[RSOURCE_COUNT];
    RDIXPipeline* pipeline[PIPELINE_COUNT];

    RDIConstantBuffer cbuffer_mdata;
    RDIConstantBuffer cbuffer_idata;

    Array<mat44_t> instance_buffer;
    Array<Vertex>  vertex_buffer;

    Array<Cmd> cmd_boxes;
    Array<Cmd> cmd_spheres;
    Array<Cmd> cmd_lines;
};



namespace
{
    RDIXPipeline* CreatePipeline( RDIDevice* dev, RDIXShaderFile* shader_file, const char* pass_name, BXIAllocator* allocator )
    {
        RDIXPipelineDesc pipeline_desc( shader_file, pass_name );
        return CreatePipeline( dev, pipeline_desc, allocator );
    }
}//

static Data g_data = {};

void StartUp( RDIDevice* dev, RSM* rsm, BXIAllocator* allocator )
{
    g_data.allocator = allocator;

    { // render source
        poly_shape_t box, sphere;
        poly_shape::createBox( &box, 1, allocator );
        poly_shape::createShpere( &sphere, 4, allocator );
        
        const uint32_t pos_stride = box.n_elem_pos * sizeof( float );
        const uint32_t index_stride = sizeof( *box.indices );

        const uint32_t nb_vertices = box.num_vertices + sphere.num_vertices;
        const uint32_t nb_indices = box.num_indices + sphere.num_indices;
        
        vec3_t* positions = (vec3_t*)BX_MALLOC( allocator, nb_vertices * pos_stride, 16 );
        memcpy( positions, box.positions, box.num_vertices * pos_stride );
        memcpy( positions + box.num_vertices, sphere.positions, sphere.num_vertices * pos_stride );

        uint32_t* indices = (uint32_t*)BX_MALLOC( allocator, nb_indices * index_stride, 4 );
        memcpy( indices, box.indices, box.num_indices * index_stride );
        memcpy( indices + box.num_indices, sphere.indices, sphere.num_indices * index_stride );

        RDIXRenderSourceRange draw_ranges[2];
        draw_ranges[DRAW_RANGE_BOX] = RDIXRenderSourceRange( 0u, box.num_indices );
        draw_ranges[DRAW_RANGE_SHERE] = RDIXRenderSourceRange( box.num_indices, sphere.num_indices );

        RDIXRenderSourceDesc desc;
        desc.Count( nb_vertices, nb_indices );
        desc.VertexBuffer( RDIVertexBufferDesc::POS(), positions );
        desc.IndexBuffer( RDIEType::UINT, indices );
        desc.draw_ranges = draw_ranges;
        desc.num_draw_ranges = (uint32_t)sizeof_array( draw_ranges );

        g_data.rsource[RSOURCE_OBJ] = CreateRenderSource( dev, desc, allocator );

        BX_FREE0( allocator, indices );
        BX_FREE0( allocator, positions );

        poly_shape::deallocateShape( &sphere );
        poly_shape::deallocateShape( &box );
    }

    { // render source
        RDIXRenderSourceDesc desc;
        desc.Count( GPU_LINE_VBUFFER_CAPACITY );
        desc.VertexBuffer( RDIVertexBufferDesc::POS4(), nullptr );
        
        g_data.rsource[RSOURCE_LINES] = CreateRenderSource( dev, desc, allocator );
    }

    {// constant buffer
        g_data.cbuffer_mdata = CreateConstantBuffer( dev, sizeof( mat44_t ) );
        g_data.cbuffer_idata = CreateConstantBuffer( dev, 16 );
    }

    {// shader
        RDIXShaderFile* shader_file = LoadShaderFile( "shader/hlsl/bin/debug.shader", rsm->Filesystem(), allocator );

        g_data.pipeline[PIPELINE_OBJ_DEPTH_WIREFRAME] = CreatePipeline( dev, shader_file, "object_D_W", allocator );
        g_data.pipeline[PIPELINE_OBJ_NDEPTH_WIREFRAME] = CreatePipeline( dev, shader_file, "object_ND_W", allocator );
        g_data.pipeline[PIPELINE_OBJ_DEPTH_SOLID] = CreatePipeline( dev, shader_file, "object_D_S", allocator );
        g_data.pipeline[PIPELINE_OBJ_NDEPTH_SOLID] = CreatePipeline( dev, shader_file, "object_ND_S", allocator );
        g_data.pipeline[PIPELINE_LINES_DEPTH] = CreatePipeline( dev, shader_file, "lines_D", allocator );
        g_data.pipeline[PIPELINE_LINES_NDEPTH] = CreatePipeline( dev, shader_file, "lines_ND", allocator );

        UnloadShaderFile( &shader_file, allocator );

        // pipeline init
        for( uint32_t i = 0; i < PIPELINE_COUNT; ++i )
        {
            RDIXResourceBinding* binding = ResourceBinding( g_data.pipeline[i] );
            SetConstantBuffer( binding, "MaterialData", &g_data.cbuffer_mdata );

            const uint32_t index = FindResource( binding, "InstanceData" );
            if( index != UINT32_MAX )
            {
                SetConstantBufferByIndex( binding, index, &g_data.cbuffer_idata );
            }
        }

        g_data.instance_buffer.Init( INITIAL_INSTANCE_CAPACITY, allocator );
        g_data.vertex_buffer.Init( INITIAL_VERTEX_CAPACITY, allocator );
        g_data.cmd_boxes.Init( INITIAL_CMD_CAPACITY, allocator );
        g_data.cmd_spheres.Init( INITIAL_CMD_CAPACITY, allocator );
        g_data.cmd_lines.Init( INITIAL_CMD_CAPACITY, allocator );
    }
}
void ShutDown( RDIDevice* dev )
{
    g_data.instance_buffer.Free( g_data.allocator );
    g_data.vertex_buffer.Free( g_data.allocator );
    g_data.cmd_boxes.Free( g_data.allocator );
    g_data.cmd_spheres.Free( g_data.allocator );
    g_data.cmd_lines.Free( g_data.allocator );

    for( uint32_t i = 0; i < PIPELINE_COUNT; ++i )
    {
        DestroyPipeline( dev, &g_data.pipeline[i] );
    }

    {
        ::Destroy( &g_data.cbuffer_idata );
        ::Destroy( &g_data.cbuffer_mdata );
    }

    for( uint32_t i = 0; i < RSOURCE_COUNT; ++i )
    {
        DestroyRenderSource( dev, &g_data.rsource[i] );
    }
}

void AddAABB( const vec3_t& center, const vec3_t& extents, const RDIXDebugParams& params )
{}
void AddSphere( const vec3_t& pos, float radius, const RDIXDebugParams& params )
{}
void AddLine( const vec3_t& start, const vec3_t& end, const RDIXDebugParams& params )
{}
void AddAxes( const mat44_t& pose, const RDIXDebugParams& params )
{}

void Flush( RDICommandQueue& cmdq, const mat44_t& viewproj )
{}

}//