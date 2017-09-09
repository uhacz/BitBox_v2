#pragma once

#include <application/application_plugin.h>

struct BXTestApp : BXIApplication
{
    bool Startup( int argc, const char** argv, BXIAllocator* allocator ) override;
    void Shutdown( BXIAllocator* allocator ) override;
    bool Update( BXWindow* win, unsigned long long deltaTimeUS, BXIAllocator* allocator ) override;
};

BX_APPLICATION_PLUGIN_DECLARE( test_app );