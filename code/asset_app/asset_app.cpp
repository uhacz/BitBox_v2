#include "asset_app.h"

#include <memory\memory.h>
#include <rtti\rtti.h>
#include <rtti\serializer.h>
#include <plugin\plugin_registry.h>
#include <plugin\plugin_load.h>

#include <window\window.h>
#include <window\window_interface.h>

#include <filesystem\filesystem_plugin.h>

#include <string.h>
#include <foundation/time.h>
#include <foundation/array.h>
#include <foundation/id_array.h>
#include <foundation/id_table.h>
#include <foundation/string_util.h>
#include <foundation\buffer.h>
#include <foundation\id_allocator_dense.h>
#include <foundation\container_soa.h>

#include <util/guid.h>
#include <util/random\random.h>
#include <util/grid.h>
#include <util/poly_shape/poly_shape.h>

#include <resource_manager\resource_manager.h>
#include <rdix/rdix.h>
#include <rdix/rdix_command_buffer.h>

#include <gfx/gfx.h>
#include <gfx/gfx_camera.h>
#include <gfx/gfx_shader_interop.h>


#include <gui/gui.h>
#include <3rd_party/imgui/imgui.h>

#include <entity/entity.h>

#include "material_editor.h"

struct GroundMesh
{
    mat44_t _world_pose;
    GFXMeshInstanceID _mesh_id;
};
void CreateGroundMesh( GroundMesh* mesh, GFX* gfx, GFXSceneID scene_id, RSM* rsm, const vec3_t& scale = vec3_t( 100.f, 0.5f, 100.f ), const mat44_t& pose = mat44_t::identity() )
{
    GFXMeshInstanceDesc desc = {};
    desc.idmaterial = gfx->FindMaterial( "checkboard" );
    desc.idmesh_resource = rsm->Find( "box" );
    const mat44_t final_pose = append_scale( pose, scale );

    mesh->_mesh_id = gfx->AddMeshToScene( scene_id, desc, final_pose );
    mesh->_world_pose = final_pose;
}
void DestroyGroundMesh( GroundMesh* mesh, GFX* gfx )
{
    gfx->RemoveMeshFromScene( mesh->_mesh_id );
}

namespace
{
    GFXCameraInputContext g_camera_input_ctx = {};
    GFXCameraID g_idcamera = { 0 };
    GFXSceneID g_idscene = { 0 };
    GroundMesh g_ground_mesh;
}



static MATEditor g_mat_editor;

bool BXAssetApp::Startup( int argc, const char** argv, BXPluginRegistry* plugins, BXIAllocator* allocator )
{
    _filesystem = (BXIFilesystem*)BXGetPlugin( plugins, BX_FILESYSTEM_PLUGIN_NAME );
    _filesystem->SetRoot( "x:/dev/assets/" );

    //GFXMaterialResource mat_res;
    //gfx_shader::Material& mat = mat_res.data;
    //mat.diffuse_albedo = vec3_t( 1.f, 0.f, 1.f );
    //mat.roughness = 0.2f;
    //mat.specular_albedo = vec3_t( 0.f, 1.f, 0.f );
    //mat.metal = 0.f;

    //string::create( &mat_res.textures[0], "abc", allocator );
    //string::create( &mat_res.textures[1], "defgh", allocator );
    //string::create( &mat_res.textures[2], "ijklmno", allocator );
    //string::create( &mat_res.textures[3], "pqrstuvwxfdsjklfdjsaklfjhsdauof", allocator );


    //SRLInstance srl;
    //data_buffer::create( &srl.data, 128, allocator );
    //srl.version = 0;
    //srl.is_writting = true;
    //srl.allocator = allocator;

    //Serialize( &srl, &mat_res );

    //_filesystem->WriteFileSync( _filesystem, "material/test.material", srl.data.data.begin(), srl.data.data.size );

    ////fopen_s( &srl.file, "test.material", "rb" );
    //srl.is_writting = false;

    //GFXMaterialResource mat_res1;
    //Serialize( &srl, &mat_res1 );
    //fclose( srl.file );

    _rsm = RSM::StartUp( _filesystem, allocator );

	BXIWindow* win_plugin = (BXIWindow*)BXGetPlugin( plugins, BX_WINDOW_PLUGIN_NAME );
	const BXWindow* window = win_plugin->GetWindow();
	::Startup( &_rdidev, &_rdicmdq, window->GetSystemHandle( window ), window->width, window->height, 0, allocator );
    
    GUI::StartUp( win_plugin, _rdidev );

    GFXDesc gfxdesc = {};
    _gfx = GFX::StartUp( _rdidev, _rsm, gfxdesc, _filesystem, allocator );

    _ent = ENT::StartUp( allocator );

    GFXSceneDesc desc;
    desc.max_renderables = 16 + 1;
    desc.name = "test scene";
    g_idscene = _gfx->CreateScene( desc );
	
    {
        {
            CreateGroundMesh( &g_ground_mesh, _gfx, g_idscene, _rsm, vec3_t(100.f, 0.5f, 100.f), mat44_t::translation( vec3_t( 0.f, -2.f, 0.f ) ) );
        }

        //for( uint32_t i = 0; i < NUM_ENTITIES; ++i )
        //{
        //    entity[i] = _ent->CreateEntity();
        //    _ent->CreateComponent( entity[i], "TestComponent" );
        //}
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
    g_idcamera = _gfx->CreateCamera( "main", GFXCameraParams(), mat44_t( mat33_t::identity(), vec3_t( 0.f, 0.f, 5.f ) ) );

    g_mat_editor.StartUp( _gfx, g_idscene, _rsm, allocator );

    return true;
}

void BXAssetApp::Shutdown( BXPluginRegistry* plugins, BXIAllocator* allocator )
{
    g_mat_editor.ShutDown( _gfx, _rsm );
    //for( uint32_t i = 0; i < NUM_ENTITIES; ++i )
    //    _ent->DestroyEntity( entity[i] );
    
    {
        ENTSystemInfo ent_sys_info = {};
        ent_sys_info.ent = _ent;
        ent_sys_info.gfx = _gfx;
        ENT::ShutDown( &_ent, &ent_sys_info );
    }

    DestroyGroundMesh( &g_ground_mesh, _gfx );
    _gfx->DestroyScene( g_idscene );
    _gfx->DestroyCamera( g_idcamera );

    GFX::ShutDown( &_gfx, _rsm );

    GUI::ShutDown();
	
    ::Shutdown( &_rdidev, &_rdicmdq, allocator );
    
    RSM::ShutDown( &_rsm );
    _filesystem = nullptr;
}



bool BXAssetApp::Update( BXWindow* win, unsigned long long deltaTimeUS, BXIAllocator* allocator )
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

	const mat44_t new_camera_world = CameraMovementFPP( g_camera_input_ctx, current_camera_matrices.world, delta_time_sec * 10.f );
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

    g_mat_editor.Tick( _gfx, _rsm, _filesystem );

    GFXFrameContext* frame_ctx = _gfx->BeginFrame( _rdicmdq, _rsm );
    //static bool tmp = false;
    //if( !tmp )
    {
        _gfx->GenerateCommandBuffer( frame_ctx, g_idscene, g_idcamera );
       // tmp = true;
    }

    if( ImGui::Begin( "Frame info" ) )
    {
        ImGui::Text( "dt: %2.4f", delta_time_sec );
    }
    ImGui::End();
		
    BindRenderTarget( _rdicmdq, _gfx->Framebuffer() );
    ClearRenderTarget( _rdicmdq, _gfx->Framebuffer(), 0.f, 0.f, 0.f, 1.f, 1.f );
	
    _gfx->SubmitCommandBuffer( frame_ctx, g_idscene );

    _gfx->PostProcess( frame_ctx, g_idcamera );

    _gfx->RasterizeFramebuffer( _rdicmdq, 1, g_idcamera );

    GUI::Draw();

    _gfx->EndFrame( frame_ctx );

    return true;
}

BX_APPLICATION_PLUGIN_DEFINE( asset_app, BXAssetApp )