#pragma once

#include <entity/entity_id.h>
#include <gfx/gfx_forward_decl.h>

struct CMNEngine;

struct TOOLContext
{
    ECSEntityID entity;
    GFXSceneID  gfx_scene;
    GFXCameraID camera;


};

struct TOOLInterface
{
    virtual ~TOOLInterface() {}
    virtual void StartUp( CMNEngine* e, const char* src_root, const char* dst_root, BXIAllocator* allocator ) = 0;
    virtual void ShutDown( CMNEngine* e ) = 0;
    virtual void Tick( CMNEngine* e, const TOOLContext& ctx, float dt ) = 0;
};