#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>

#include <foundation/memory/memory.h>
#include <foundation/plugin/plugin_registry.h>
#include <foundation/plugin/plugin_load.h>

#include <window/window_interface.h>
#include <window/window.h>

#include <filesystem/filesystem_plugin.h>

#include <test_app/test_app.h>
#include "foundation/time.h"

#include <3rd_party\pugixml\pugixml.hpp>

/*
<boot>
<rootDir path=""/>
</boot>

*/
static char testxml[] =
{
"<pass\n"
"	name = \"object\"\n"
"	vertex = \"vs_object\"\n"
"	pixel = \"ps_main\">\n"
"	<define USE_TEXTURES = \"1\"\n"
"	USE_LIGHTNING = \"0\" />\n"
"	<hwstate\n"
"	depth_test = \"1\"\n"
"	depth_write = \"1\"\n"
"	fill_mode = \"WIREFRAME\"\n"
"	/>\n"

"</pass>\n"
};
int main( int argc, const char** argv )
{
	pugi::xml_document doc;
	doc.load_buffer_inplace( testxml, sizeof( testxml ) );

	for( pugi::xml_node pass : doc.children( "pass" ) )
	{
		const char* name = pass.attribute( "name" ).as_string();
		const char* vertex = pass.attribute( "vertex" ).as_string();
		
		pugi::xml_node hw_state = pass.child( "hwstate" );
		pugi::xml_node define = pass.child( "define" );

		for( pugi::xml_attribute macro : define.attributes() )
		{
			const char* name = macro.name();
			const int value = macro.as_int();
			int a = 0;
		}

	}



    // --- startup
    BXIAllocator* default_allocator = nullptr;
    BXMemoryStartUp( &default_allocator );
    
    BXPluginRegistry* plug_reg = nullptr;
    BXPluginRegistryStartup( &plug_reg, default_allocator );

	BXPluginLoad( plug_reg, BX_FILESYSTEM_PLUGIN_NAME, default_allocator );

    BXIWindow* window_plug = (BXIWindow*)BXPluginLoad( plug_reg, BX_WINDOW_PLUGIN_NAME, default_allocator );
    BXWindow* window = window_plug->Create( "BitBox", 1600, 900, false, default_allocator );

    BXIApplication* app_plug = (BXIApplication*)BXPluginLoad( plug_reg, BX_APPLICATION_PLUGIN_NAME_test_app(), default_allocator );
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

            //if( input->IsKeyPressedOnce( BXInput::eKEY_ESC ) )
            //    ret = 0;

            //ret = app->update( deltaTimeUS );
            ret = app_plug->Update( window, delta_time_us, default_allocator );

            InputSwap( &window->input );
            InputClear( &window->input, true, false, true );

        } while( ret );
    }// --- if

    // --- shutdown
    app_plug->Shutdown( plug_reg, default_allocator );
    window_plug->Destroy();

    BXPluginUnload( plug_reg, BX_APPLICATION_PLUGIN_NAME_test_app(), default_allocator );
    BXPluginUnload( plug_reg, BX_WINDOW_PLUGIN_NAME, default_allocator );
	BXPluginUnload( plug_reg, BX_FILESYSTEM_PLUGIN_NAME, default_allocator );

    BXPluginRegistryShutdown( &plug_reg, default_allocator );
    BXMemoryShutDown( &default_allocator );
    return 0;
}