#include "sandbox_app.h"
#include <window\window.h>
#include <memory\memory.h>
#include <plugin/plugin_load.h>

#include <plugin/plugin_registry.h>
#include <filesystem/filesystem_plugin.h>

#include <window/window.h>
#include <window/window_interface.h>

#include <stdio.h>
#include <atomic>

#include <gfx/gfx.h>
#include <entity/entity_system.h>
#include <entity\components\name_component.h>
#include "common\ground_mesh.h"
#include "common\sky.h"
#include "gui\gui.h"
#include "3rd_party\imgui\imgui.h"
#include "rdix\rdix.h"
#include "rdix\rdix_debug_draw.h"
#include "foundation\time.h"
#include "anim\anim_player.h"
#include "common\common.h"
#include "anim\anim.h"
#include "foundation\array.h"
#include "util\color.h"
#include "anim\anim_debug.h"
#include "foundation\eastl\vector.h"
#include "foundation\eastl\span.h"

#include "util\signal_filter.h"
#include "math\math_common.h"

#include "../foundation/EASTL/string.h"

#include "world_editor.h"

namespace sandbox
{
    static WorldEditor* _world_editor = nullptr;
}//


bool BXSandboxApp::Startup( int argc, const char** argv, BXPluginRegistry* plugins, BXIAllocator* allocator )
{
    CMNEngine::Startup( this, argc, argv, plugins, allocator );
   	
    sandbox::_world_editor = BX_NEW( allocator, sandbox::WorldEditor );
    sandbox::_world_editor->Initialize( this, allocator );
    
    return true;
}

void BXSandboxApp::Shutdown( BXPluginRegistry* plugins, BXIAllocator* allocator )
{
    sandbox::_world_editor->Uninitialize( this );
    BX_DELETE0( allocator, sandbox::_world_editor );

    CMNEngine::Shutdown( this );
}

bool BXSandboxApp::Update( BXWindow* win, unsigned long long deltaTimeUS, BXIAllocator* allocator )
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

    if( !ImGui::GetIO().WantCaptureMouse )
    {
        sandbox::_world_editor->TickCamera( gfx, win->input, delta_time_sec );
    }

    sandbox::_world_editor->RenderGUI( gfx );
    sandbox::_world_editor->RenderFrame( rdicmdq, gfx );

    return true;
}

BX_APPLICATION_PLUGIN_DEFINE( sandbox_app, BXSandboxApp );
