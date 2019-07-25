#pragma once

#include "gfx/gfx_forward_decl.h"

struct BXIFilesystem;
struct BXIAllocator;

void CreateSky( const char* cubemap_file, GFXSceneID scene_id, GFX* gfx, BXIFilesystem* filesystem, BXIAllocator* allocator );
