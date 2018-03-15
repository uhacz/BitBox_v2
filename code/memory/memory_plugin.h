#pragma once

#include "dll_interface.h"

struct BXIAllocator;

extern "C"
{
    MEMORY_PLUGIN_EXPORT void BXMemoryStartUp();
    MEMORY_PLUGIN_EXPORT void BXMemoryShutDown();
}//