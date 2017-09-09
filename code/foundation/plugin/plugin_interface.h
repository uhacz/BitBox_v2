#pragma once

#define PLUGIN_EXPORT __declspec(dllexport)

struct BXIAllocator;
typedef void*( *PluginLoadFunc )( BXIAllocator* );
typedef void ( *PluginUnloadFunc )( void*, BXIAllocator* );

