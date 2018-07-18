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
#include <entity\entity.h>

bool Startup( CMNEngine* e, int argc, const char** argv, BXPluginRegistry* plugins, BXIAllocator* allocator )
{
    BXIFilesystem* filesystem = (BXIFilesystem*)BXGetPlugin( plugins, BX_FILESYSTEM_PLUGIN_NAME );
    filesystem->SetRoot( "x:/dev/assets/" );

    RDIDevice* rdidev = nullptr;
    RDICommandQueue* rdicmdq = nullptr;

    BXIWindow* win_plugin = (BXIWindow*)BXGetPlugin( plugins, BX_WINDOW_PLUGIN_NAME );
    const BXWindow* window = win_plugin->GetWindow();
    ::Startup( &rdidev, &rdicmdq, window->GetSystemHandle( window ), window->width, window->height, 0, allocator );

    GUI::StartUp( win_plugin, rdidev );

    RSM* rsm = RSM::StartUp( filesystem, allocator );

    RDIXDebug::StartUp( rdidev, rsm, allocator );

    GFXDesc gfxdesc = {};
    GFX* gfx = GFX::StartUp( rdidev, rsm, gfxdesc, filesystem, allocator );

    ENT* ent = ENT::StartUp( allocator );

    e->_filesystem = filesystem;
    e->_rdidev = rdidev;
    e->_rdicmdq = rdicmdq;

    e->_rsm = rsm;
    e->_gfx = gfx;
    e->_ent = ent;

    return true;
}

void Shutdown( CMNEngine* e, BXIAllocator* allocator )
{
    {
        ENTSystemInfo ent_sys_info = {};
        ent_sys_info.ent = e->_ent;
        ent_sys_info.gfx = e->_gfx;
        ENT::ShutDown( &e->_ent, &ent_sys_info );
    }

    GFX::ShutDown( &e->_gfx, e->_rsm );
    RDIXDebug::ShutDown( e->_rdidev );
    RSM::ShutDown( &e->_rsm );
    GUI::ShutDown();
    ::Shutdown( &e->_rdidev, &e->_rdicmdq, allocator );

    e->_filesystem = nullptr;
    e->_rdidev = nullptr;
    e->_rdicmdq = nullptr;
    e->_rsm = nullptr;
    e->_gfx = nullptr;
    e->_ent = nullptr;
}