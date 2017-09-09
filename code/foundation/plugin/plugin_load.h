#pragma once

struct BXPluginRegistry;
struct BXIAllocator;

void* BXPluginLoad( BXPluginRegistry* reg, const char* name, BXIAllocator* pluginAllocator );
void BXPluginUnload( BXPluginRegistry* reg, const char* name, BXIAllocator* pluginAllocator );
