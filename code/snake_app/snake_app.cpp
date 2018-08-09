#include "snake_app.h"
#include <memory\memory.h>
#include <plugin\plugin_registry.h>

#include <filesystem\filesystem_plugin.h>
#include <window\window.h>
#include <window\window_interface.h>

#include <resource_manager\resource_manager.h>
#include <gui\gui.h>
#include <gfx\gfx.h>
#include <entity\entity.h>

#include <common\ground_mesh.h>

#include <3rd_party/imgui/imgui.h>
#include <foundation\time.h>
#include <rdix\rdix.h>
#include "rdix\rdix_debug_draw.h"
#include "util\grid.h"
#include "foundation\type_compound.h"

static GFXSceneID g_idscene = { 0 };
static GFXCameraID g_idcamera = {0};

static GFXCameraInputContext g_camera_input_ctx = {};
static CMNGroundMesh g_ground_mesh = {};

vec3_t GridSizeAsVec3( const grid_t& grid )
{
    return vec3_t( (float)grid.width, (float)grid.height, (float)grid.depth );
}
vec3_t GridExtentAsVec3( const grid_t& grid )
{
    return vec3_t( (float)(grid.width/2), (float)(grid.height/2), (float)(grid.depth/2) );
}

inline float Wrap( float value, float extent )
{
    const float size = extent * 2.f;
    if( value > extent )
        value -= size;
    else if( value < -extent )
        value += size;

    return value;
}
inline vec3_t Wrap( const vec3_t& value, const vec3_t& extent )
{
    return vec3_t( Wrap( value.x, extent.x ), Wrap( value.y, extent.y ), Wrap( value.z, extent.z ) );
}
inline vec3_t Unwrap( const vec3_t& value, const vec3_t& size, uint32_t mask )
{
    vec3_t result = value;
    if( mask & 0x1 )
        result.x = (value.x > 0.f) ? value.x - size.x : value.x + size.x;
    if( mask & 0x2 )
        result.y = (value.y > 0.f) ? value.y - size.y : value.y + size.y;
    if( mask & 0x4 )
        result.z = (value.z > 0.f) ? value.z - size.z : value.z + size.z;

    return result;
}

using vec3i_t = int32x3_t;

struct SnakeInput
{
    vec3_t dir;
};

struct Snake
{
    static constexpr uint32_t MAX_LEN = 64;
    static_array_t<vec3_t, MAX_LEN> pos;
    vec3_t vel;

    uint32_t current_length;
};

void SnakeInit( Snake* snake, const vec3_t& pos, const vec3_t& vel )
{
    for( uint32_t i = 0; i < Snake::MAX_LEN; ++i )
        snake->pos[i] = pos;

    snake->vel = vel;
    snake->current_length = 8;
}

vec3_t SnakeCollectInput( const BXInput& input )
{
    vec3_t result( 0.f );
    const BXInput::PadState& pad = input.pad.CurrentState();
    if( pad.connected )
    {
        
    }
    else
    {
        if( input.IsKeyPressed( 'W' ) )
            result.y += 1.f;
        if( input.IsKeyPressed( 'S' ) )
            result.y -= 1.f;
        if( input.IsKeyPressed( 'A' ) )
            result.x -= 1.f;
        if( input.IsKeyPressed( 'D' ) )
            result.x += 1.f;
    }

    return result;
}

void SnakeSteer( Snake* snake, const vec3_t& input )
{
    vec3_t input_rounded = input;
    //input_rounded.x = (input.x > 0.5f) ? 1.f : -1.f;
    //input_rounded.y = (input.y > 0.5f) ? 1.f : -1.f;
    //input_rounded.z = (input.z > 0.5f) ? 1.f : -1.f;
    if( length_sqr( input_rounded ) > FLT_EPSILON )
    {
        snake->vel = input_rounded;
        snake->vel = min_per_elem( max_per_elem( snake->vel, vec3_t( -1.f ) ), vec3_t( 1.f ) );
    }
}

void SnakeMove( Snake* snake, const grid_t& grid, float grid_cell_size, float dt )
{
    snake->pos[0] += snake->vel * dt * 8;
    
    const vec3_t grid_ext = GridExtentAsVec3( grid );
    const vec3_t grid_size = GridSizeAsVec3( grid );

    for( uint32_t i = 1; i < snake->current_length; ++i )
    {
        const vec3_t& p0 = snake->pos[i - 1];
        const vec3_t& p1 = snake->pos[i];

        vec3_t vec = p0 - p1;

        uint32_t unwrap_mask = 0;
        if( ::fabs( vec.x ) >= grid_ext.x )
            unwrap_mask |= 0x1;
        if( ::fabs( vec.y ) >= grid_ext.y )
            unwrap_mask |= 0x2;
        if( ::fabs( vec.z ) >= grid_ext.z )
            unwrap_mask |= 0x4;

        vec = Unwrap( vec, grid_size, unwrap_mask );
        vec3_t dir = vec;
        float len = normalized( &dir );

        BXCheckFloat( len );
        if( len > grid_cell_size )
        {
            snake->pos[i] = p0 - dir*grid_cell_size;
        }
    }
    
    for( uint32_t i = 0; i < snake->current_length; ++i )
    {
        const vec3_t p = snake->pos[i];
        snake->pos[i] = Wrap( p, grid_ext );
    }

}

void SnakeDebugDraw( const Snake& snake )
{
    for( uint32_t i = 0; i < snake.current_length; ++i )
    {
        RDIXDebug::AddAABB( snake.pos[i], vec3_t( 0.5f ), RDIXDebugParams().Solid() );
        RDIXDebug::AddAABB( snake.pos[i], vec3_t( 0.501f ), RDIXDebugParams( 0x00FF00FF ) );
    }

    RDIXDebug::AddLine( snake.pos[0], snake.pos[0] + snake.vel, RDIXDebugParams(0xFF0000FF) );

}

struct Level
{
    grid_t grid = { 32, 32, 32 };
};

static Snake g_snake = {};
static Level g_level = {};

bool SnakeApp::Startup( int argc, const char** argv, BXPluginRegistry* plugins, BXIAllocator* allocator )
{
    ::Startup( (CMNEngine*)this, argc, argv, plugins, allocator );

    GFXSceneDesc desc;
    desc.max_renderables = 16 + 1;
    desc.name = "test scene";
    g_idscene = _gfx->CreateScene( desc );

    {
        CreateGroundMesh( &g_ground_mesh, _gfx, g_idscene, _rsm, vec3_t( 100.f, 0.5f, 100.f ), mat44_t::translation( vec3_t( 0.f, -32.f, 0.f ) ) );
    }

    {// sky
        BXFileWaitResult filewait = _filesystem->LoadFileSync( _filesystem, "texture/sky_cubemap.dds", BXEFIleMode::BIN, allocator );
        if( filewait.file.pointer )
        {
            if( _gfx->SetSkyTextureDDS( g_idscene, filewait.file.pointer, filewait.file.size ) )
            {
                _gfx->EnableSky( g_idscene, true );
            }
        }
        _filesystem->CloseFile( filewait.handle );

    }
    g_idcamera = _gfx->CreateCamera( "main", GFXCameraParams(), mat44_t( mat33_t::identity(), vec3_t( 0.f, 16.f, 64.f ) ) );
    
    SnakeInit( &g_snake, vec3_t( 0.f ), vec3_t( 0.f ) );

    return true;
}

void SnakeApp::Shutdown( BXPluginRegistry* plugins, BXIAllocator* allocator )
{
    _gfx->DestroyCamera( g_idcamera );
    _gfx->DestroyScene( g_idscene );
    DestroyGroundMesh( &g_ground_mesh, _gfx );
    ::Shutdown( (CMNEngine*)this, allocator );
}

bool SnakeApp::Update( BXWindow* win, unsigned long long deltaTimeUS, BXIAllocator* allocator )
{
    if( win->input.IsKeyPressedOnce( BXInput::eKEY_ESC ) )
        return false;

    GUI::NewFrame();

    const float delta_time_sec = (float)BXTime::Micro_2_Sec( deltaTimeUS );

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
    const GFXCameraMatrices& current_camera_matrices = _gfx->CameraMatrices( g_idcamera );

    const mat44_t new_camera_world = CameraMovementFPP( g_camera_input_ctx, current_camera_matrices.world, delta_time_sec * 20.f );
    _gfx->SetCameraWorld( g_idcamera, new_camera_world );
    _gfx->ComputeCamera( g_idcamera );

    {
        ENTSystemInfo ent_sys_info = {};
        ent_sys_info.ent = _ent;
        ent_sys_info.gfx = _gfx;
        ent_sys_info.rsm = _rsm;
        ent_sys_info.default_allocator = allocator;
        ent_sys_info.scratch_allocator = allocator;
        ent_sys_info.gfx_scene_id = g_idscene;
        _ent->Step( &ent_sys_info, deltaTimeUS );
    }

    {
        const vec3_t snake_input = SnakeCollectInput( win->input );
        SnakeSteer( &g_snake, snake_input );
        SnakeMove( &g_snake, g_level.grid, 1.f, delta_time_sec );
        SnakeDebugDraw( g_snake );
    }


    GFXFrameContext* frame_ctx = _gfx->BeginFrame( _rdicmdq, _rsm );
    
    _gfx->GenerateCommandBuffer( frame_ctx, g_idscene, g_idcamera );

    if( ImGui::Begin( "Frame info" ) )
    {
        ImGui::Text( "dt: %2.4f", delta_time_sec );
    }
    ImGui::End();

    RDIXDebug::AddAxes( mat44_t::identity() );

    {
        const vec3_t size = GridSizeAsVec3( g_level.grid );
        const vec3_t ext = size * 0.5f;

        RDIXDebug::AddAABB( vec3_t( 0.f ), ext, RDIXDebugParams( 0xFF0000FF ) );
    }


    BindRenderTarget( _rdicmdq, _gfx->Framebuffer() );
    ClearRenderTarget( _rdicmdq, _gfx->Framebuffer(), 0.f, 0.f, 0.f, 1.f, 1.f );

    _gfx->SubmitCommandBuffer( frame_ctx, g_idscene );

    _gfx->PostProcess( frame_ctx, g_idcamera );

    {
        const GFXCameraMatrices& camera_matrices = _gfx->CameraMatrices( g_idcamera );
        const mat44_t viewproj = camera_matrices.proj_api * camera_matrices.view;

        BindRenderTarget( _rdicmdq, _gfx->Framebuffer(), { 1 }, true );
        RDIXDebug::Flush( _rdicmdq, viewproj );
    }
    _gfx->RasterizeFramebuffer( _rdicmdq, 1, g_idcamera );

    GUI::Draw();

    _gfx->EndFrame( frame_ctx );
    return true;
}


BX_APPLICATION_PLUGIN_DEFINE( snake_app, SnakeApp )