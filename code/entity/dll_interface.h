#pragma once

#ifdef BX_DLL_entity
#define ENT_EXPORT __declspec(dllexport)
#else
#define ENT_EXPORT __declspec(dllimport)
#endif
