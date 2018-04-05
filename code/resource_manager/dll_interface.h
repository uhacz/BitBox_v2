#pragma once

#ifdef BX_DLL_resource_manager
#define RSM_EXPORT __declspec(dllexport)
#else
#define RSM_EXPORT __declspec(dllimport)
#endif

