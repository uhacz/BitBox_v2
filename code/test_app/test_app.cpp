#include "test_app.h"
#include <window\window.h>
#include <foundation\memory\memory.h>

bool BXTestApp::Startup( int argc, const char** argv, BXIAllocator* allocator )
{
    return true;
}

void BXTestApp::Shutdown( BXIAllocator* allocator )
{

}

bool BXTestApp::Update( BXWindow* win, unsigned long long deltaTimeUS, BXIAllocator* allocator )
{
    if( win->input.IsKeyPressedOnce( BXInput::eKEY_ESC ) )
        return false;
	 
    return true;
}

BX_APPLICATION_PLUGIN_DEFINE( test_app, BXTestApp );