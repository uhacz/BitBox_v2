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
#include <node/node.h>

#include <entity/components/name_component.h>

static void RegisterCommonComponents( ECS* ecs )
{
    RegisterComponentNoPOD<CMPName>( ecs, "Name" );
}

bool CMNBaseEngine::Startup( CMNBaseEngine* e, int argc, const char** argv, BXPluginRegistry* plugins, BXIAllocator* main_allocator )
{
    e->allocator = main_allocator;
    e->filesystem = (BXIFilesystem*)BXGetPlugin( plugins, BX_FILESYSTEM_PLUGIN_NAME );
    e->filesystem->SetRoot( "x:/dev/assets/" );

    RSM::StartUp( e->filesystem, e->allocator );

    BXIWindow* win_plugin = (BXIWindow*)BXGetPlugin( plugins, BX_WINDOW_PLUGIN_NAME );
    const BXWindow* window = win_plugin->GetWindow();
    ::Startup( &e->rdidev, &e->rdicmdq, window->GetSystemHandle( window ), window->width, window->height, 0, e->allocator );

    RDIXDebug::StartUp( e->rdidev, e->allocator );
    return true;
}

void CMNBaseEngine::Shutdown( CMNBaseEngine* e )
{
    RDIXDebug::ShutDown( e->rdidev );
    RSM::ShutDown();
    ::Shutdown( &e->rdidev, &e->rdicmdq, e->allocator );

    e->filesystem = nullptr;
    e->allocator = nullptr;
}

bool CMNEngine::Startup( CMNEngine* e, int argc, const char** argv, BXPluginRegistry* plugins, BXIAllocator* main_allocator )
{
    CMNBaseEngine::Startup( e, argc, argv, plugins, main_allocator );

    BXIWindow* win_plugin = (BXIWindow*)BXGetPlugin( plugins, BX_WINDOW_PLUGIN_NAME );
    GUI::StartUp( win_plugin, e->rdidev );

    GFXDesc gfxdesc = {};
    e->gfx = GFX::StartUp( e->rdidev, gfxdesc, e->filesystem, e->allocator );

    e->ecs = ECS::StartUp( e->allocator );
    RegisterCommonComponents( e->ecs );

    NODEContainer::StartUp( &e->nodes, e->allocator );
    return true;
}

void CMNEngine::Shutdown( CMNEngine* e )
{
    {
        NODEInitContext ctx;
        ctx.gfx = e->gfx;
        NODEContainer::ShutDown( &e->nodes, &ctx );
    }
    ECS::ShutDown( &e->ecs );
    GFX::ShutDown( &e->gfx );
    
    GUI::ShutDown();
    
    CMNBaseEngine::Shutdown(e);
}


