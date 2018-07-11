#pragma once

#include <application/application_plugin.h>

#include <common/base_engine_init.h>

struct SnakeApp : BXIApplication, CMNEngine
{
    bool Startup( int argc, const char** argv, BXPluginRegistry* plugins, BXIAllocator* allocator ) override;
    void Shutdown( BXPluginRegistry* plugins, BXIAllocator* allocator ) override;
    bool Update( BXWindow* win, unsigned long long deltaTimeUS, BXIAllocator* allocator ) override;
};

BX_APPLICATION_PLUGIN_DECLARE( snake_app )