#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>

#include <memory/memory_plugin.h>
#include <memory/memory.h>
#include <plugin/plugin_registry.h>
#include <plugin/plugin_load.h>

#include <window/window_interface.h>
#include <window/window.h>

#include <filesystem/filesystem_plugin.h>
#include <rtti/rtti.h>

#include <application/application_plugin.h>

#include <foundation/time.h>

#include <3rd_party\pugixml\pugixml.hpp>

int main( int argc, const char** argv )
{
    // --- startup
    BXMemoryStartUp();
    BXIAllocator* default_allocator = BXDefaultAllocator();

    BXPluginRegistry* plug_reg = nullptr;
    BXPluginRegistryStartup( &plug_reg, default_allocator );

    BXPluginLoad( plug_reg, BX_RTTI_PLUGIN_NAME, default_allocator );
    BXPluginLoad( plug_reg, BX_FILESYSTEM_PLUGIN_NAME, default_allocator );

    BXIWindow* window_plug = (BXIWindow*)BXPluginLoad( plug_reg, BX_WINDOW_PLUGIN_NAME, default_allocator );
    BXWindow* window = window_plug->Create( "BitBox", 1600, 900, false, default_allocator );

	const char* app_plug_name = "test_app"; // BX_APPLICATION_PLUGIN_NAME_asset_app();
    BXIApplication* app_plug = (BXIApplication*)BXPluginLoad( plug_reg, app_plug_name, default_allocator );
    if( app_plug->Startup( argc, argv, plug_reg, default_allocator ) )
    {
        HWND hwnd = (HWND)window->GetSystemHandle( window );

        bool ret = 1;

        BXTimeQuery time_query = BXTimeQuery::Begin();
        
        do
        {
            BXTimeQuery::End( &time_query );
            const uint64_t delta_time_us = time_query.duration_US;
            time_query = BXTimeQuery::Begin();

            MSG msg = { 0 };
            while( PeekMessage( &msg, hwnd, 0U, 0U, PM_REMOVE ) != 0 )
            {
                TranslateMessage( &msg );
                DispatchMessage( &msg );
            }

            BXInput* input = &window->input;
            BXInput::KeyboardState* kbdState = input->kbd.CurrentState();
            for( int i = 0; i < 256; ++i )
            {
                kbdState->keys[i] = ( GetAsyncKeyState( i ) & 0x8000 ) != 0;
            }

            kbdState->keys[BXInput::eKEY_CAPSLOCK] = ( GetKeyState( VK_CAPITAL ) & 0x0001 );

            InputUpdatePad_XInput( &input->pad, 1 );
            BXInput::ComputeMouseDelta( &window->input.mouse );

            ret = app_plug->Update( window, delta_time_us, default_allocator );

            InputSwap( &window->input );
            InputClear( &window->input, true, false, true );

        } while( ret );
    }// --- if

    // --- shutdown
    app_plug->Shutdown( plug_reg, default_allocator );
    window_plug->Destroy();

    BXPluginUnload( plug_reg, app_plug_name, default_allocator );
    BXPluginUnload( plug_reg, BX_WINDOW_PLUGIN_NAME, default_allocator );
	BXPluginUnload( plug_reg, BX_FILESYSTEM_PLUGIN_NAME, default_allocator );
    BXPluginUnload( plug_reg, BX_RTTI_PLUGIN_NAME, default_allocator );

    BXPluginRegistryShutdown( &plug_reg, default_allocator );
    BXMemoryShutDown();
    return 0;
}