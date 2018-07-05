#include "snake_app.h"
#include <memory\memory.h>

bool SnakeApp::Startup( int argc, const char** argv, BXPluginRegistry* plugins, BXIAllocator* allocator )
{
    return true;
}

void SnakeApp::Shutdown( BXPluginRegistry* plugins, BXIAllocator* allocator )
{

}

bool SnakeApp::Update( BXWindow* win, unsigned long long deltaTimeUS, BXIAllocator* allocator )
{
    return true;
}


BX_APPLICATION_PLUGIN_DEFINE( snake_app, SnakeApp )