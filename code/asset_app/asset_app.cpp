#include "asset_app.h"



#include <window\window.h>
#include <window\window_interface.h>

#include <filesystem\filesystem_plugin.h>

#include <foundation\plugin\plugin_registry.h>
#include <foundation\memory\memory.h>

bool BXAssetApp::Startup( int argc, const char** argv, BXPluginRegistry* plugins, BXIAllocator* allocator )
{
    _filesystem = (BXIFilesystem*)BXGetPlugin( plugins, BX_FILESYSTEM_PLUGIN_NAME );
    _filesystem->SetRoot( "x:/dev/assets/" );

    BXIWindow* win_plugin = (BXIWindow*)BXGetPlugin( plugins, BX_WINDOW_PLUGIN_NAME );
    const BXWindow* window = win_plugin->GetWindow();
    ::Startup( &_rdidev, &_rdicmdq, window->GetSystemHandle( window ), 800, 600, 0, allocator );

    return true;
}

void BXAssetApp::Shutdown( BXPluginRegistry* plugins, BXIAllocator* allocator )
{
    ::Shutdown( &_rdidev, &_rdicmdq, allocator );
    _filesystem = nullptr;
}

bool BXAssetApp::Update( BXWindow* win, unsigned long long deltaTimeUS, BXIAllocator* allocator )
{
    if( win->input.IsKeyPressedOnce( BXInput::eKEY_ESC ) )
        return false;



    return true;
}

BX_APPLICATION_PLUGIN_DEFINE( asset_app, BXAssetApp )