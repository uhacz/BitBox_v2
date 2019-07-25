#include "sky.h"
#include "../filesystem/filesystem_plugin.h"
#include "../memory/allocator.h"
#include "../gfx/gfx.h"

void CreateSky( const char* cubemap_file, GFXSceneID scene_id, GFX* gfx, BXIFilesystem* filesystem, BXIAllocator* allocator )
{
    BXFileWaitResult filewait = filesystem->LoadFileSync( filesystem, "texture/sky_cubemap.dds", BXEFIleMode::BIN, allocator );
    if( filewait.file.pointer )
    {
        if( gfx->SetSkyTextureDDS( scene_id, filewait.file.pointer, filewait.file.size ) )
        {
            gfx->EnableSky( scene_id, true );
        }
    }
    filesystem->CloseFile( &filewait.handle );
}
