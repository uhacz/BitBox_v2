#pragma once

#ifdef BX_DLL_gfx
#define GFX_EXPORT __declspec(dllexport)
#else
#define GFX_EXPORT __declspec(dllimport)
#endif