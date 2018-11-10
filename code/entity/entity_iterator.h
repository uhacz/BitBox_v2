#pragma once

#include "entity_id.h"

struct ECS;
struct ECSImpl;

struct ECSEntityIterator
{
    ECSEntityIterator( ECS* ecs, ECSEntityID id );

    bool IsValid() const;
    void GoToSlibling();
    void GoToChild();
    void GoToParent();

    ECSEntityID Entity() const { return _current_id; }

private:
    ECSEntityID _current_id;
    const ECSImpl* _impl;
};
