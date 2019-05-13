#include "asset_app.h"

#include <window\window.h>
#include <window\window_interface.h>

#include <foundation/time.h>
#include <rdix/rdix.h>
#include <rdix/rdix_debug_draw.h>

#include <gui/gui.h>
#include <3rd_party/imgui/imgui.h>

#include <entity/entity_system.h>
#include <node/node.h>
#include <common/ground_mesh.h>

#include "components.h"

#include "material_tool.h"
#include "mesh_tool.h"
#include "anim_tool.h"
#include "anim_matching_tool.h"
#include "foundation\array.h"
#include "util\color.h"

namespace
{
    static CMNGroundMesh g_ground_mesh;
    
    static GFXCameraInputContext g_camera_input_ctx = {};
    static GFXCameraID g_idcamera = { 0 };
    static GFXSceneID g_idscene = { 0 };

    static ECSComponentID g_transform_system_id = {};

    namespace ToolType
    {
        enum E
        {
            MATERIAL = 0,
            MESH,
            ANIM,
            ANIM_MATCH,

            _COUNT_,
        };
    }//
    static TOOLInterface* g_tools[ToolType::_COUNT_] = {};
    static ECSEntityID g_tool_entities[ToolType::_COUNT_] = {};

    static TOOLFolders g_folders = {};
}

static const f32 shape_scale = 0.1f;
const vec3_t shape[] =
{
    vec3_t( 0, 0.01f, 0 ) * shape_scale,
    vec3_t( 1, 0.01f, 0 ) * shape_scale,
    vec3_t( 2, 0.01f, 1 ) * shape_scale,
    vec3_t( 2, 0.01f, 2 ) * shape_scale,
    vec3_t( 3, 0.01f, 2 ) * shape_scale,
    vec3_t( 3, 0.01f, 3 ) * shape_scale,
    vec3_t( 4, 0.01f, 3 ) * shape_scale,
    vec3_t( 4, 0.01f, 4 ) * shape_scale,
    vec3_t( 5, 0.01f, 4 ) * shape_scale,
    vec3_t( 5, 0.01f, 5 ) * shape_scale,
    vec3_t( 5, 0.01f, 6 ) * shape_scale,
    vec3_t( 4, 0.01f, 6 ) * shape_scale,
    vec3_t( 3, 0.01f, 6 ) * shape_scale,
    vec3_t( 3, 0.01f, 5 ) * shape_scale,
    vec3_t( 2, 0.01f, 5 ) * shape_scale,
    vec3_t( 2, 0.01f, 4 ) * shape_scale,
    vec3_t( 1, 0.01f, 4 ) * shape_scale,
    vec3_t( 1, 0.01f, 3 ) * shape_scale,
    vec3_t( 0, 0.01f, 3 ) * shape_scale,
    vec3_t( 0, 0.01f, 2 ) * shape_scale,
    vec3_t( 0, 0.01f, 1 ) * shape_scale,
};
static constexpr u32 num_points = (u32)sizeof_array( shape );

static f32 TriangleArea( f32 a, f32 b, f32 c )
{
    const f32 p = 0.5f * (a + b + c);
    const f32 area = ::sqrtf( p * (p - a)*(p - b)*(p - c) );
    return area;
}
struct Tri
{
    u32 i[3];
    f32 area;
};
struct SmallestTriResult
{
    f32 area;
    u32 middle_point_index;
};

SmallestTriResult ComputeTriangles( array_t<Tri>& triangles, const array_span_t<const vec3_t> input )
{
    SmallestTriResult result;
    result.area = FLT_MAX;
    result.middle_point_index = UINT32_MAX;

    for( u32 i = 1; i < input.size() - 1; ++i )
    {
        Tri t;
        t.i[0] = i - 1;
        t.i[1] = i;
        t.i[2] = i + 1;

        const vec3_t& p0 = input[t.i[0]];
        const vec3_t& p1 = input[t.i[1]];
        const vec3_t& p2 = input[t.i[2]];

        const vec3_t v01 = p1 - p0;
        const vec3_t v12 = p2 - p1;
        const vec3_t v20 = p0 - p2;

        const f32 a = length( v01 );
        const f32 b = length( v12 );
        const f32 c = length( v20 );

        t.area = TriangleArea( a, b, c );

        array::push_back( triangles, t );

        if( t.area < result.area )
        {
            result.area = t.area;
            result.middle_point_index = t.i[1];
        }
    }

    return result;
}

void SimplifyShape( array_t<vec3_t>& output, const array_span_t<const vec3_t> input )
{
    for( vec3_t in : input )
        array::push_back( output, in );

    array_t<Tri> triangles;
    while( output.size > input.size() / 2 )
    {
        if( output.size <= 4 )
            break;

        array::clear( triangles );
        SmallestTriResult smallest = ComputeTriangles( triangles, to_array_span( output ) );
    
        if( smallest.area >= 0.03f + FLT_EPSILON )
            break;

        array::erase( output, smallest.middle_point_index );
    }

}


bool BXAssetApp::Startup( int argc, const char** argv, BXPluginRegistry* plugins, BXIAllocator* allocator )
{
    CMNEngine::Startup( (CMNEngine*)this, argc, argv, plugins, allocator );

    //NODE* node = nodes->CreateNode( "test_node" );

    //nodes->DestroyNode( &node );



    {
        RegisterComponent< TOOLMeshComponent >( ecs, "TOOL_Mesh" );
        RegisterComponentNoPOD< TOOLSkinningComponent >( ecs, "TOOL_Skinning" );
        RegisterComponentNoPOD< TOOLMeshDescComponent >( ecs, "TOOL_MeshDesc" );
        RegisterComponentNoPOD< TOOLAnimDescComponent >( ecs, "TOOL_AnimDesc" );
        RegisterComponent< TOOLTransformComponent >( ecs, "TOOL_Transform" );
        RegisterComponent< TOOLTransformMeshComponent >( ecs, "TOOL_TransfomMesh" );

        RegisterComponent< TOOLTransformSystem>( ecs, "TOOL_TransformSystem" );
    }

    GFXSceneDesc desc;
    desc.max_renderables = 16 + 1;
    desc.name = "test scene";
    g_idscene = gfx->CreateScene( desc );

    CreateGroundMesh( &g_ground_mesh, gfx, g_idscene, vec3_t(100.f, 1.f, 100.f), mat44_t::translation( vec3_t( 0.f, -0.5f, 0.f ) ) );
    
    {// sky
        BXFileWaitResult filewait = filesystem->LoadFileSync( filesystem, "texture/sky_cubemap.dds", BXEFIleMode::BIN, allocator );
        if( filewait.file.pointer )
        {
            if( gfx->SetSkyTextureDDS( g_idscene, filewait.file.pointer, filewait.file.size ) )
            {
                gfx->EnableSky( g_idscene, true );
            }
        }
        filesystem->CloseFile( &filewait.handle );

    }
    g_idcamera = gfx->CreateCamera( "main", GFXCameraParams(), mat44_t( mat33_t::identity(), vec3_t( 0.f, 4.f, 18.f ) ) );

    g_transform_system_id = CreateComponent<TOOLTransformSystem>( ecs ).id;

    TOOLFolders::StartUp( &g_folders, filesystem, allocator );

    g_tools[ToolType::MATERIAL] = BX_NEW( allocator, MATERIALTool );
    g_tools[ToolType::ANIM] = BX_NEW( allocator, ANIMTool );
    g_tools[ToolType::ANIM_MATCH] = BX_NEW( allocator, ANIMMatchingTool );
    g_tools[ToolType::MESH] = BX_NEW( allocator, MESHTool );

    g_tools[ToolType::MATERIAL]->StartUp( this, nullptr, allocator );
    g_tools[ToolType::MESH]->StartUp( this, ".src/mesh/", allocator );
    g_tools[ToolType::ANIM]->StartUp( this, ".src/anim/", allocator );
    g_tools[ToolType::ANIM_MATCH]->StartUp( this, ".src/anim/", allocator );

    const f32 separation = 3.0f;
    const f32 delta_angle = PI2 / ToolType::_COUNT_;
    const f32 radius = ( separation / sinf( delta_angle ) ) * 2.f;
    const f32 check = sinf( delta_angle ) * radius * 0.5f;
    
    for( uint32_t i = 0; i < ToolType::_COUNT_; ++i )
    {
        g_tool_entities[i] = ecs->CreateEntity();

        const f32 angle = (f32)i * delta_angle;
        const f32 x = ::cosf( angle ) * radius;
        const f32 z = ::sinf( angle ) * radius;
        const f32 y = 0.f;

        mat44_t pose = mat44_t::translation( vec3_t( x, y, z ) );

        auto xform_proxy = ECSComponentProxy<TOOLTransformComponent>::New( ecs );
        xform_proxy->Initialize( pose );
        xform_proxy.SetOwner( g_tool_entities[i] );
    }

    return true;
}

void BXAssetApp::Shutdown( BXPluginRegistry* plugins, BXIAllocator* allocator )
{
    for( uint32_t i = 0; i < ToolType::_COUNT_; ++i )
    {
        ecs->MarkForDestroy( g_tool_entities[i] );
    }
    for( uint32_t i = 0; i < ToolType::_COUNT_; ++i )
    {
        g_tools[i]->ShutDown( this );
        BX_DELETE0( allocator, g_tools[i] );
    }

    ecs->MarkForDestroy( g_transform_system_id );

    DestroyGroundMesh( &g_ground_mesh, gfx );
    gfx->DestroyScene( g_idscene );
    gfx->DestroyCamera( g_idcamera );

    CMNEngine::Shutdown( (CMNEngine*)this, allocator );
}

bool BXAssetApp::Update( BXWindow* win, unsigned long long deltaTimeUS, BXIAllocator* allocator )
{
    if( win->input.IsKeyPressedOnce( BXInput::eKEY_ESC ) )
        return false;

    GUI::NewFrame();

	const float delta_time_sec = (float)BXTime::Micro_2_Sec( deltaTimeUS );
    if( ImGui::Begin( "Frame info" ) )
    {
        ImGui::Text( "dt: %2.4f", delta_time_sec );
    }
    ImGui::End();

    
    {
        NODESystemContext ctx;
        ctx.gfx = gfx;
        nodes->Tick( &ctx, delta_time_sec );
    }
    ecs->Update();

    if( !ImGui::GetIO().WantCaptureMouse )
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
    const GFXCameraMatrices& current_camera_matrices = gfx->CameraMatrices( g_idcamera );

	const mat44_t new_camera_world = CameraMovementFPP( g_camera_input_ctx, current_camera_matrices.world, delta_time_sec * 10.f );
    gfx->SetCameraWorld( g_idcamera, new_camera_world );
    gfx->ComputeCamera( g_idcamera );

    {
        if( ImGui::Begin( "Camera" ) )
        {
            const vec3_t camera_pos = new_camera_world.translation();
            ImGui::InputFloat3( "position", (float*)&camera_pos.x );
        }
        ImGui::End();
    }

    {
        TOOLContext tool_ctx;
        tool_ctx.camera = g_idcamera;
        tool_ctx.gfx_scene = g_idscene;
        tool_ctx.folders = &g_folders;

        for( uint32_t i = 0; i < ToolType::_COUNT_; ++i )
        {
            tool_ctx.entity = g_tool_entities[i];
            g_tools[i]->Tick( this, tool_ctx, delta_time_sec );
        }
    }

    if( auto proxy = ECSComponentProxy<TOOLTransformSystem>( ecs, g_transform_system_id ) )
    {
        proxy->Tick( ecs, gfx );
    }

    {
        array_t<vec3_t> simple_shape;
        SimplifyShape( simple_shape, to_array_span( shape, num_points ) );
        
        for( u32 i = 0; i < num_points; ++i )
        {
            const u32 i0 = i;
            const u32 i1 = (i + 1) % num_points;
            RDIXDebug::AddLine( shape[i0], shape[i1] );
            RDIXDebug::AddSphere( shape[i0], shape_scale * 0.1f );
        }


        RDIXDebugParams params( color32_t::RED() );
        for( u32 i = 0; i < simple_shape.size; ++i )
        {
            const u32 i0 = i;
            const u32 i1 = (i + 1) % simple_shape.size;
            RDIXDebug::AddLine( simple_shape[i0], simple_shape[i1], params );
            RDIXDebug::AddSphere( simple_shape[i0], shape_scale * 0.1f, params );
        }


    }

    { // debug draw all transforms
        array_span_t<TOOLTransformComponent*> xform_comps = Components<TOOLTransformComponent>( ecs );
        for( TOOLTransformComponent* xform_comp : xform_comps )
        {
            RDIXDebug::AddAxes( xform_comp->pose );
        }
    }

    gfx->DoSkinningCPU( rdicmdq );
    gfx->DoSkinningGPU( rdicmdq );

    GFXFrameContext* frame_ctx = gfx->BeginFrame( rdicmdq );
    
    gfx->GenerateCommandBuffer( frame_ctx, g_idscene, g_idcamera );
		
    

    BindRenderTarget( rdicmdq, gfx->Framebuffer() );
    ClearRenderTarget( rdicmdq, gfx->Framebuffer(), 0.f, 0.f, 0.f, 1.f, 1.f );
	
    gfx->SubmitCommandBuffer( frame_ctx, g_idscene );

    gfx->PostProcess( frame_ctx, g_idcamera );

    {
        const GFXCameraMatrices& camera_matrices = gfx->CameraMatrices( g_idcamera );
        const mat44_t viewproj = camera_matrices.proj_api * camera_matrices.view;

        BindRenderTarget( rdicmdq, gfx->Framebuffer(), { 1 }, true );
        RDIXDebug::Flush( rdicmdq, viewproj );
    }

    gfx->RasterizeFramebuffer( rdicmdq, 1, g_idcamera );

    GUI::Draw();

    gfx->EndFrame( frame_ctx );

    return true;
}

BX_APPLICATION_PLUGIN_DEFINE( asset_app, BXAssetApp )