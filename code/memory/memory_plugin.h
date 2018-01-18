#pragma once

#include <foundation/memory/memory.h>

#ifdef BX_DLL_memory
#define MEMORY_PLUGIN_EXPORT __declspec(dllexport)
#else
#define MEMORY_PLUGIN_EXPORT __declspec(dllimport)
#endif


extern "C"
{
    MEMORY_PLUGIN_EXPORT void BXMemoryStartUp();
    MEMORY_PLUGIN_EXPORT void BXMemoryShutDown();

    MEMORY_PLUGIN_EXPORT BXIAllocator* BXDefaultAllocator();
}//