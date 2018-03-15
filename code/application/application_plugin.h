#pragma once

#include <plugin/plugin_interface.h>

#define BX_APPLICATION_PLUGIN_DECLARE( name )\
constexpr const char* BX_APPLICATION_PLUGIN_NAME_##name() { return #name; }\
extern "C" {\
PLUGIN_EXPORT void* BXLoad_##name( BXIAllocator* allocator );\
PLUGIN_EXPORT void  BXUnload_##name( void* ptr, BXIAllocator* allocator );\
}

#define BX_APPLICATION_PLUGIN_DEFINE( name, class_name )\
PLUGIN_EXPORT void* BXLoad_##name( BXIAllocator* allocator )\
{\
    BXIApplication* plugin = BX_NEW( allocator, class_name );\
    return plugin;\
}\
PLUGIN_EXPORT void BXUnload_##name( void* ptr, BXIAllocator* allocator )\
{\
    auto* plugin = (BXIApplication*)ptr;\
    BX_DELETE( allocator, plugin );\
}

struct BXWindow;
struct BXPluginRegistry;
struct BXIApplication
{
    virtual bool Startup( int argc, const char** argv, BXPluginRegistry* plugins, BXIAllocator* allocator ) = 0;
    virtual void Shutdown( BXPluginRegistry* plugins, BXIAllocator* allocator ) = 0;
    virtual bool Update( BXWindow* win, unsigned long long deltaTimeUS, BXIAllocator* allocator ) = 0;
};

