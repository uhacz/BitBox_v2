#pragma once

#include <entity/entity_id.h>
#include <gfx/gfx_forward_decl.h>
#include "common/common.h"

struct CMNEngine;

struct TOOLFolders
{
    common::FolderContext material;
    common::FolderContext mesh;
    common::FolderContext anim;
    common::FolderContext node;

    static void StartUp( TOOLFolders* folders, BXIFilesystem* fs, BXIAllocator* allocator );
    static void ShutDown( TOOLFolders* folders );
};


struct TOOLContext
{
    ECSEntityID entity;
    GFXSceneID  gfx_scene;
    GFXCameraID camera;

    TOOLFolders* folders;
};

struct TOOLInterface
{
    virtual ~TOOLInterface() {}
    virtual void StartUp( CMNEngine* e, const char* src_root, BXIAllocator* allocator ) = 0;
    virtual void ShutDown( CMNEngine* e ) = 0;
    virtual void Tick( CMNEngine* e, const TOOLContext& ctx, float dt ) = 0;
};