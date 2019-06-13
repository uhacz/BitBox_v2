#include "tool_context.h"

static void StartUpFolder( common::FolderContext* ctx, const char* folder, BXIFilesystem* fs, BXIAllocator* allocator )
{
    ctx->SetUp( fs, allocator );
    ctx->SetFolder( folder );
}

void TOOLFolders::StartUp( TOOLFolders* folders, BXIFilesystem* fs, BXIAllocator* allocator )
{
    StartUpFolder( &folders->material, "material/", fs, allocator );
    StartUpFolder( &folders->mesh, "mesh/", fs, allocator );
    StartUpFolder( &folders->anim, "anim/", fs, allocator );
    StartUpFolder( &folders->node, "node/", fs, allocator );
}

void TOOLFolders::ShutDown( TOOLFolders* folders )
{

}
