#include "test_app.h"
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

struct CMPMesh
{
    GFXMeshInstanceID mesh_id;
    GFXMaterialID material_id;
};
struct CMPWorldXForm
{
    xform_t data;
};



bool BXTestApp::Startup( int argc, const char** argv, BXPluginRegistry* plugins, BXIAllocator* allocator )
{
    CMNEngine::Startup( this, argc, argv, plugins, allocator );

    RegisterComponent<CMPMesh>( _ecs, "Mesh" );
    RegisterComponent<CMPWorldXForm>( _ecs, "WorldXForm" );
    
    ECSComponentID meshid = CreateComponent<CMPMesh>( _ecs );
    ECSComponentID xformid = CreateComponent<CMPWorldXForm>( _ecs );

    CMPWorldXForm* xform_data = Component<CMPWorldXForm>( _ecs, xformid );
    xform_data->data = xform_t::identity();

    array_span_t<CMPWorldXForm*> xforms = Components<CMPWorldXForm>( _ecs );

	return true;
}

void BXTestApp::Shutdown( BXPluginRegistry* plugins, BXIAllocator* allocator )
{
    CMNEngine::Shutdown( this, allocator );
}

bool BXTestApp::Update( BXWindow* win, unsigned long long deltaTimeUS, BXIAllocator* allocator )
{
    if( win->input.IsKeyPressedOnce( BXInput::eKEY_ESC ) )
        return false;
	
    return true;
}

BX_APPLICATION_PLUGIN_DEFINE( test_app, BXTestApp );