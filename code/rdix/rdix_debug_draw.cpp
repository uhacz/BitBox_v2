#include "rdix_debug_draw.h"
#include "rdix.h"

#include <foundation/array.h>
#include <util/poly_shape/poly_shape.h>
#include <resource_manager/resource_manager.h>
#include <rdi_backend/rdi_backend.h>

#include <atomic>
#include <algorithm>
#include "util/range_splitter.h"

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
            uint32_t pipeline_index : 8;
            uint32_t rsource_range_index : 4;
            uint32_t rsource_index : 4;
            uint32_t depth : 16;
        };
    };
};
inline bool operator < ( const Cmd& a, const Cmd& b )
{
    return a.key < b.key;
}

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
    PIPELINE_OBJ_DEPTH_SOLID,

    PIPELINE_OBJ_NDEPTH_WIREFRAME,
    PIPELINE_OBJ_NDEPTH_SOLID,
    
    PIPELINE_LINES_DEPTH,
    PIPELINE_LINES_NDEPTH,

    PIPELINE_COUNT,
};

static inline EPipeline SelectPipeline( const RDIXDebugParams& params, ERSource rsource_type )
{
    static const EPipeline router[RSOURCE_COUNT][2][2] =
    {
        {
            { PIPELINE_OBJ_DEPTH_WIREFRAME, PIPELINE_OBJ_DEPTH_SOLID },
            { PIPELINE_OBJ_NDEPTH_WIREFRAME, PIPELINE_OBJ_NDEPTH_SOLID }
        },
        {
            { PIPELINE_LINES_DEPTH, PIPELINE_LINES_DEPTH },
            { PIPELINE_LINES_NDEPTH, PIPELINE_LINES_NDEPTH }
        }
    };

    const uint32_t depth_index = params.flag & DEPTH ? 0 : 1;
    const uint32_t raster_index = params.flag & SOLID ? 1 : 0;

    return router[rsource_type][depth_index][raster_index];
}

static constexpr uint32_t INITIAL_INSTANCE_CAPACITY = 128;
static constexpr uint32_t INITIAL_VERTEX_CAPACITY = 512;
static constexpr uint32_t INITIAL_CMD_CAPACITY = 128;
static constexpr uint32_t GPU_LINE_VBUFFER_CAPACITY = 1024 * 4; // number of vertices, NOT lines
static constexpr uint32_t GPU_MATRIX_BUFFER_CAPACITY = 128;


template<typename T>
struct Array
{
    T* data = nullptr;
    uint32_t capacity = 0;
    std::atomic_uint32_t count = 0;

    T* begin() { return data; }
    T* end() { return data + count; }

    bool Empty() const { return count == 0; }

    void Init( uint32_t initial_cap, BXIAllocator* allocator )
    {
        if( initial_cap <= capacity )
            return;

        if( data )
        {
            BX_FREE0( allocator, data );
        }

        data = (T*)BX_MALLOC( allocator, initial_cap * sizeof( T ), ALIGNOF( T ) );
        capacity = initial_cap;
        count = 0;
    }

    T& operator[]( uint32_t i )
    {
        SYS_ASSERT( i < count );
        return data[i];
    }
    const T& operator[]( uint32_t i ) const
    {
        SYS_ASSERT( i < count );
        return data[i];
    }
    
    uint32_t Add( uint32_t amount )
    {
        uint32_t index = count.fetch_add( amount, std::memory_order_acquire );
        if( (index + amount) > capacity )
            return UINT32_MAX;

        return index;
    }

    void Clear( BXIAllocator* allocator )
    {
        const int32_t overflow = count - capacity;
        if( overflow > 0 )
        {
            Init( capacity * 2, allocator );
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
    RDIBufferRO       buffer_matrices;
    
    Array<mat44_t> instance_buffer;
    Array<Vertex>  vertex_buffer;

    Array<Cmd> cmd_objects;
    Array<Cmd> cmd_lines;
};

struct InstanceData
{
    uint32_t instance_batch_offset = 0;
    uint32_t padding_[3];
};
struct MaterialData
{
    mat44_t view_proj_matrix = mat44_t::identity();
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
        poly_shape::createShpere( &sphere, 3, allocator );
        
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
        draw_ranges[DRAW_RANGE_SHERE] = RDIXRenderSourceRange( box.num_indices, sphere.num_indices, box.num_vertices );

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
        g_data.cbuffer_mdata = CreateConstantBuffer( dev, sizeof( MaterialData ) );
        g_data.cbuffer_idata = CreateConstantBuffer( dev, sizeof( InstanceData ) );
        g_data.buffer_matrices = CreateStructuredBufferRO( dev, GPU_MATRIX_BUFFER_CAPACITY, sizeof( mat44_t ), RDIECpuAccess::WRITE );
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

            uint32_t index = FindResource( binding, "InstanceData" );
            if( index != UINT32_MAX )
            {
                SetConstantBufferByIndex( binding, index, &g_data.cbuffer_idata );
            }

            index = FindResource( binding, "g_matrices" );
            if( index != UINT32_MAX )
            {
                SetResourceROByIndex( binding, index, &g_data.buffer_matrices );
            }
        }

        g_data.instance_buffer.Init( INITIAL_INSTANCE_CAPACITY, allocator );
        g_data.vertex_buffer.Init( INITIAL_VERTEX_CAPACITY, allocator );
        g_data.cmd_objects.Init( INITIAL_CMD_CAPACITY, allocator );
        g_data.cmd_lines.Init( INITIAL_CMD_CAPACITY, allocator );
    }
}
void ShutDown( RDIDevice* dev )
{
    g_data.instance_buffer.Free( g_data.allocator );
    g_data.vertex_buffer.Free( g_data.allocator );
    g_data.cmd_lines.Free( g_data.allocator );
    g_data.cmd_objects.Free( g_data.allocator );

    for( uint32_t i = 0; i < PIPELINE_COUNT; ++i )
    {
        DestroyPipeline( dev, &g_data.pipeline[i] );
    }

    {
        ::Destroy( &g_data.buffer_matrices );
        ::Destroy( &g_data.cbuffer_idata );
        ::Destroy( &g_data.cbuffer_mdata );
    }

    for( uint32_t i = 0; i < RSOURCE_COUNT; ++i )
    {
        DestroyRenderSource( dev, &g_data.rsource[i] );
    }
}

struct ObjectCmd
{
    Cmd* cmd;
    mat44_t* matrix;
};
static ObjectCmd AddObject( ERSource rsource, EDrawRange draw_range, const RDIXDebugParams& params )
{
    const uint32_t cmd_index = g_data.cmd_objects.Add( 1 );
    const uint32_t data_index = g_data.instance_buffer.Add( 1 );
    Cmd* cmd = &g_data.cmd_objects[cmd_index];
    mat44_t* matrix = &g_data.instance_buffer[data_index];

    cmd->data_offset = data_index;
    cmd->data_count = 1;
    cmd->rsource_index = rsource;
    cmd->rsource_range_index = draw_range;
    cmd->pipeline_index = SelectPipeline( params, rsource );
    cmd->depth = 0;

    ObjectCmd result;
    result.cmd = cmd;
    result.matrix = matrix;
    return result;
}

void AddAABB( const vec3_t& center, const vec3_t& extents, const RDIXDebugParams& params )
{
    ObjectCmd objcmd = AddObject( RSOURCE_OBJ, DRAW_RANGE_BOX, params );
    objcmd.matrix[0] = append_scale( mat44_t::translation( center ), extents * params.scale );
    objcmd.matrix[0].c3.w = TypeReinterpert( params.color ).f;
}
void AddSphere( const vec3_t& pos, float radius, const RDIXDebugParams& params )
{
    ObjectCmd objcmd = AddObject( RSOURCE_OBJ, DRAW_RANGE_SHERE, params );
    objcmd.matrix[0] = append_scale( mat44_t::translation( pos ), vec3_t( radius * params.scale ) );
    objcmd.matrix[0].c3.w = TypeReinterpert( params.color ).f;
}

void AddLine( const vec3_t& start, const vec3_t& end, const RDIXDebugParams& params )
{
    const uint32_t cmd_index = g_data.cmd_lines.Add( 1 );
    const uint32_t data_index = g_data.vertex_buffer.Add( 2 );
    Cmd* cmd = &g_data.cmd_lines[cmd_index];
    Vertex* vertices = &g_data.vertex_buffer[data_index];

    cmd->data_offset = data_index;
    cmd->data_count = 2;
    cmd->rsource_index = RSOURCE_LINES;
    cmd->rsource_range_index = 0;
    cmd->pipeline_index = SelectPipeline( params, RSOURCE_LINES );
    cmd->depth = 0;

    vertices[0].pos = start;
    vertices[1].pos = end;
    vertices[0].color = params.color;
    vertices[1].color = params.color;
}
void AddAxes( const mat44_t& pose, const RDIXDebugParams& params )
{}



void Flush( RDICommandQueue* cmdq, const mat44_t& viewproj )
{
    if( g_data.cmd_lines.Empty() && g_data.cmd_objects.Empty() )
        return;

    MaterialData mdata = {};
    mdata.view_proj_matrix = viewproj;
    UpdateCBuffer( cmdq, g_data.cbuffer_mdata, &mdata );

    InstanceData idata = {};
    UpdateCBuffer( cmdq, g_data.cbuffer_idata, &idata );

    std::sort( g_data.cmd_objects.begin(), g_data.cmd_objects.end(), std::less<Cmd>() );
    std::sort( g_data.cmd_lines.begin(), g_data.cmd_lines.end(), std::less<Cmd>() );
    
    {
        const uint32_t nb_objects = g_data.cmd_objects.count;
        RangeSplitter splitter = RangeSplitter::SplitByGrab( nb_objects, GPU_MATRIX_BUFFER_CAPACITY );
        while( splitter.ElementsLeft() )
        {
            const RangeSplitter::Grab grab = splitter.NextGrab();
            
            const mat44_t* src_data = g_data.instance_buffer.begin() + grab.begin;
            uint8_t* mapped_data = Map( cmdq, g_data.buffer_matrices, 0, RDIEMapType::WRITE );

            memcpy( mapped_data, src_data, grab.count * g_data.buffer_matrices.elementStride );
            Unmap( cmdq, g_data.buffer_matrices );

            const uint32_t begin = grab.begin;
            const uint32_t end = grab.end();

            for( uint32_t i = begin; i < end; ++i )
            {
                const Cmd& cmd = g_data.cmd_objects[i];
                idata.instance_batch_offset = cmd.data_offset;
                UpdateCBuffer( cmdq, g_data.cbuffer_idata, &idata );

                RDIXPipeline* pipeline = g_data.pipeline[cmd.pipeline_index];
                RDIXRenderSource* rsource = g_data.rsource[cmd.rsource_index];

                BindPipeline( cmdq, pipeline, true );
                BindRenderSource( cmdq, rsource );
                SubmitRenderSourceInstanced( cmdq, rsource, 1, cmd.rsource_range_index );
            }
        }
    }



    g_data.cmd_objects.Clear( g_data.allocator );
    g_data.cmd_lines.Clear( g_data.allocator );
    g_data.instance_buffer.Clear( g_data.allocator );
    g_data.vertex_buffer.Clear( g_data.allocator );
}

}//