#pragma once

#include "../gfx/gfx_forward_decl.h"
#include "../gfx/gfx_camera.h"
#include "../util/color.h"
#include "../foundation/EASTL/string.h"


struct BXIAllocator;
struct BXInput;
struct GFX;
struct CMNEngine;

namespace sandbox
{
    struct WorldEditor
    {
        BXIAllocator* _allocator = 0;
        eastl::string _world_path;

        struct
        {
            GFXCameraID id = { 0 };

            GFXCameraInputContext input_ctx = {};
            mat44_t prev_matrix = mat44_t::identity();
            mat44_t next_matrix = mat44_t::identity();
            f32 time_acc = 0.f;
            f32 tick_dt = 1.f / 60.f;
            f32 sensitivity = 0.1f;
        } _camera;


        GFXSceneID _gfx_scene_id = { 0 };

        struct
        {
            i32 extent = 10;
            color32_t color = color32_t::GRAY();
        }_ground_plane;

        void Initialize( CMNEngine* e, BXIAllocator* allocator );
        void Uninitialize( CMNEngine* e );

        void TickCamera( GFX* gfx, const BXInput& input, f32 dt );
        void RenderGUI( GFX* gfx );
        void RenderGUI_Camera( GFX* gfx );
        void RenderFrame( RDICommandQueue* rdicmdq, GFX* gfx );

    };
}//