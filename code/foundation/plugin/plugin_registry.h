#pragma once


struct BXPluginRegistry;
struct BXIAllocator;

void  BXPluginRegistryStartup ( BXPluginRegistry** regPP, BXIAllocator* allocator );
void  BXPluginRegistryShutdown( BXPluginRegistry** regPP, BXIAllocator* allocator );
void  BXAddPlugin   ( BXPluginRegistry* reg, const char* name, void* plugin );
void  BXRemovePlugin( BXPluginRegistry* reg, const char* name );
void* BXGetPlugin   ( BXPluginRegistry* reg, const char* name );


