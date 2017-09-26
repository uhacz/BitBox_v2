#pragma once

#include <application/application_plugin.h>

struct BXIFilesystem;
struct BXTestApp : BXIApplication
{
    bool Startup( int argc, const char** argv, BXPluginRegistry* plugins, BXIAllocator* allocator ) override;
    void Shutdown( BXPluginRegistry* plugins, BXIAllocator* allocator ) override;
    bool Update( BXWindow* win, unsigned long long deltaTimeUS, BXIAllocator* allocator ) override;


private:
	BXIFilesystem* _filesystem = nullptr;
};

BX_APPLICATION_PLUGIN_DECLARE( test_app );