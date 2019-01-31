#include "base_engine_init.h"

#include <memory\memory.h>
#include <plugin\plugin_registry.h>

#include <filesystem\filesystem_plugin.h>
#include <window\window.h>
#include <window\window_interface.h>

#include <rdi_backend\rdi_backend.h>
#include <rdix\rdix_debug_draw.h>

#include <resource_manager\resource_manager.h>
#include <gui\gui.h>
#include <gfx\gfx.h>
#include <entity\entity_system.h>

#include <entity/components/name_component.h>

static void RegisterCommonComponents( ECS* ecs )
{
    RegisterComponentNoPOD<CMPName>( ecs, "Name" );
}

bool CMNEngine::Startup( CMNEngine* e, int argc, const char** argv, BXPluginRegistry* plugins, BXIAllocator* allocator )
{
    BXIFilesystem* filesystem = (BXIFilesystem*)BXGetPlugin( plugins, BX_FILESYSTEM_PLUGIN_NAME );
    filesystem->SetRoot( "x:/dev/assets/" );

    RSM::StartUp( filesystem, allocator );

    RDIDevice* rdidev = nullptr;
    RDICommandQueue* rdicmdq = nullptr;

    BXIWindow* win_plugin = (BXIWindow*)BXGetPlugin( plugins, BX_WINDOW_PLUGIN_NAME );
    const BXWindow* window = win_plugin->GetWindow();
    ::Startup( &rdidev, &rdicmdq, window->GetSystemHandle( window ), window->width, window->height, 0, allocator );

    GUI::StartUp( win_plugin, rdidev );
    RDIXDebug::StartUp( rdidev, allocator );

    GFXDesc gfxdesc = {};
    GFX* gfx = GFX::StartUp( rdidev, gfxdesc, filesystem, allocator );

    ECS* ecs = ECS::StartUp( allocator );
    RegisterCommonComponents( ecs );

    e->filesystem = filesystem;
    e->rdidev = rdidev;
    e->rdicmdq = rdicmdq;

    e->gfx = gfx;
    e->ecs = ecs;

    return true;
}

void CMNEngine::Shutdown( CMNEngine* e, BXIAllocator* allocator )
{
    ECS::ShutDown( &e->ecs );
    GFX::ShutDown( &e->gfx );
    RDIXDebug::ShutDown( e->rdidev );
    GUI::ShutDown();
    
    RSM::ShutDown();
    ::Shutdown( &e->rdidev, &e->rdicmdq, allocator );

    e->filesystem = nullptr;
    e->rdidev = nullptr;
    e->rdicmdq = nullptr;
    e->gfx = nullptr;
}
