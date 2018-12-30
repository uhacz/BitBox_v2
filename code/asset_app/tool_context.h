#pragma once

#include <entity/entity_id.h>
#include <gfx/gfx_forward_decl.h>

struct TOOLContext
{
    ECSEntityID entity;
    GFXSceneID  gfx_scene;
    GFXCameraID camera;
};