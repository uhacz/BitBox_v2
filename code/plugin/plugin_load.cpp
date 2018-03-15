#include "plugin_load.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <stdio.h>

#include <foundation/debug.h>
#include <memory/memory.h>
#include "plugin_interface.h"
#include "plugin_registry.h"

namespace bx
{
    static const int NAME_LEN = 255;
    static const int NAME_SIZE = NAME_LEN + 1;
    static inline void GetDLLName( char* buff, const char* name )
    {
        sprintf_s( buff, NAME_LEN, "%s.dll", name );
    }
    static inline void GetDLLModuleName( char* buff, const char* name )
    {
        sprintf_s( buff, NAME_LEN, "%s.dll", name );
    }
    static inline void GetLoadFunctionName( char* buff, const char* name )
    {
        sprintf_s( buff, NAME_LEN, "BXLoad_%s", name );
    }
    static inline void GetUnloadFunctionName( char* buff, const char* name )
    {
        sprintf_s( buff, NAME_LEN, "BXUnload_%s", name );
    }

}

void* BXPluginLoad( BXPluginRegistry* reg, const char* name, BXIAllocator* pluginAllocator )
{
    char dll_name[bx::NAME_SIZE];
    char load_func_name[bx::NAME_SIZE];
    bx::GetDLLName( dll_name, name );
    bx::GetLoadFunctionName( load_func_name, name );

    HMODULE module = LoadLibrary( dll_name );
    if( module == NULL )
    {
        SYS_LOG_ERROR( "plugin '%s' load failed!", name );
        return nullptr;
    }

    PluginLoadFunc load_func = (PluginLoadFunc)GetProcAddress( module, load_func_name );
    if( load_func == nullptr )
    {
        SYS_LOG_ERROR( "plugin '%s' load function not found", name );
        FreeLibrary( module );
        return nullptr;
    }

    void* plugin = load_func( pluginAllocator );
    if( plugin == nullptr )
    {
        SYS_LOG_ERROR( "plugin '%s' load failed", name );
        FreeLibrary( module );
        return nullptr;
    }

    BXAddPlugin( reg, name, plugin );

    return plugin;
}

void BXPluginUnload( BXPluginRegistry* reg, const char* name, BXIAllocator* pluginAllocator )
{
    void* plugin = BXGetPlugin( reg, name );
    if( plugin == nullptr )
    {
        SYS_LOG_ERROR( "plugin '%s' not found in registry!", name );
        return;
    }

    BXRemovePlugin( reg, name );

    char module_name[bx::NAME_SIZE];
    char unload_func_name[bx::NAME_SIZE];
    bx::GetDLLModuleName( module_name, name );
    bx::GetUnloadFunctionName( unload_func_name, name );

    HMODULE module = GetModuleHandle( module_name );
    if( module == NULL )
    {
        SYS_LOG_ERROR( "module '%s' not found!", module_name );
        return;
    }

    PluginUnloadFunc unload_func = (PluginUnloadFunc)GetProcAddress( module, unload_func_name );
    if( unload_func == nullptr )
    {
        SYS_LOG_ERROR( "plugin '%s' unload function not found", name );
        return;
    }

    unload_func( plugin, pluginAllocator );
    FreeLibrary( module );
}
