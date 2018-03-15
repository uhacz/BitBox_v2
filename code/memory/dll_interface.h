#pragma once

#ifdef BX_DLL_memory
#define MEMORY_PLUGIN_EXPORT __declspec(dllexport)
#else
#define MEMORY_PLUGIN_EXPORT __declspec(dllimport)
#endif
